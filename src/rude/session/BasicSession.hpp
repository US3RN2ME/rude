
#ifndef RUDE_SESSION_BASICSESSION_HPP
#define RUDE_SESSION_BASICSESSION_HPP

#include <array>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <cstdint>
#include <rude/concepts/CongestionControl.hpp>
#include <rude/concepts/PacketCodec.hpp>
#include <rude/concepts/SessionPolicy.hpp>
#include <rude/concepts/Socket.hpp>
#include <rude/core/Executor.hpp>
#include <rude/detail/channel/ChannelVariant.hpp>
#include <rude/detail/DefaultCodec.hpp>
#include <rude/detail/RecvLoop.hpp>
#include <rude/detail/SendGuard.hpp>
#include <rude/detail/UdpSocket.hpp>
#include <rude/detail/congestion/LeakyBucket.hpp>
#include <rude/session/ChannelConfig.hpp>
#include <rude/session/SessionConfig.hpp>
#include <rude/session/SocketStats.hpp>
#include <span>
#include <system_error>

namespace rude {

   /// Non-template base class holding the erased interface that SessionPolicy
   /// callbacks receive. Avoids circular template dependencies.
   class SessionBase {
   public:
      virtual ~SessionBase() = default;

      [[nodiscard]] virtual boost::asio::ip::udp::endpoint remoteEndpoint() const noexcept = 0;
      [[nodiscard]] virtual SocketStats::Snapshot stats() const noexcept = 0;
      virtual void cancel() noexcept = 0;
   };

   /// The central session type. Owns a strand, a socket, a codec, a policy,
   /// and an array of channel variants. All async operations are dispatched
   /// on strand_ so the session is safe to use from multiple threads.
   ///
   /// Template parameters:
   ///   Codec      — satisfies PacketCodec    (wire format)
   ///   OrderedCC  — satisfies CongestionCtrl (used by ReliableOrderedChannel)
   ///   UnorderedCC— satisfies CongestionCtrl (used by ReliableUnorderedChannel)
   ///   Policy     — satisfies SessionPolicy  (lifecycle callbacks)
   ///   Sock       — satisfies Socket         (UDP transport, swappable for tests)
   template <PacketCodec Codec, CongestionCtrl OrderedCC, CongestionCtrl UnorderedCC, SessionPolicy Policy, Socket Sock>
   class BasicSession : public SessionBase {
   public:
      using ChannelVar = ChannelVariant<OrderedCC, UnorderedCC>;

      static constexpr std::uint8_t kMaxChannels = 8;

      /// Construct with a pre-connected socket (called by BasicAcceptor / BasicConnector).
      explicit BasicSession(Executor executor, Sock sock, Codec codec, Policy policy, SessionConfig config,
                            boost::asio::ip::udp::endpoint remote)
          : strand_{std::move(executor)}
          , sock_{std::move(sock)}
          , codec_{std::move(codec)}
          , policy_{std::move(policy)}
          , config_{config}
          , remote_{remote}
          , keepaliveTimer_{strand_} {}

      // ── Async operations ──────────────────────────────────────────────────────

      /// Send payload on channel ch. Completion: void(std::error_code).
      /// Supports any Asio CompletionToken: co_await, callback, use_future, deferred.
      template <boost::asio::completion_token_for<void(std::error_code)> Token>
      auto asyncSend(std::uint8_t ch, std::span<std::byte const> payload, Token&& token) {
         return boost::asio::async_initiate<Token, void(std::error_code)>(
             [this, ch, payload](auto handler) {
                boost::asio::dispatch(strand_, [this, ch, payload, h = std::move(handler)]() mutable {
                   if (ch >= kMaxChannels) {
                      h(makeErrorCode(Error::ChannelClosed));
                      return;
                   }
                   std::visit(
                       [&](auto& channel) {
                          channel.asyncSend(payload, sock_, remote_, codec_, stats_, std::move(h));
                       },
                       channels_[ch]);
                });
             },
             token);
      }

      /// Receive next message on channel ch into buf. Completion: void(std::error_code, std::size_t).
      template <boost::asio::completion_token_for<void(std::error_code, std::size_t)> Token>
      auto asyncRecv(std::uint8_t ch, boost::asio::mutable_buffer buf, Token&& token) {
         return boost::asio::async_initiate<Token, void(std::error_code, std::size_t)>(
             [this, ch, buf](auto handler) {
                boost::asio::dispatch(strand_, [this, ch, buf, h = std::move(handler)]() mutable {
                   if (ch >= kMaxChannels) {
                      h(makeErrorCode(Error::ChannelClosed), 0);
                      return;
                   }
                   std::visit(
                       [&](auto& channel) {
                          channel.asyncRecv(buf, std::move(h));
                       },
                       channels_[ch]);
                });
             },
             token);
      }

      /// Flush all pending retransmits on all channels. Completion: void(std::error_code).
      template <boost::asio::completion_token_for<void(std::error_code)> Token>
      auto asyncFlush(Token&& token) {
         return boost::asio::async_initiate<Token, void(std::error_code)>(
             [this](auto handler) {
                boost::asio::dispatch(strand_, [this, h = std::move(handler)]() mutable {
                   std::error_code ec;
                   for (auto& ch : channels_) {
                      std::visit(
                          [&](auto& channel) {
                             channel.flush(sock_, remote_, codec_, ec);
                          },
                          ch);
                   }
                   h(ec);
                });
             },
             token);
      }

      /// Send DISCONNECT and close. Completion: void(std::error_code).
      template <boost::asio::completion_token_for<void(std::error_code)> Token>
      auto asyncDisconnect(Token&& token) {
         return boost::asio::async_initiate<Token, void(std::error_code)>(
             [this](auto handler) {
                boost::asio::dispatch(strand_, [this, h = std::move(handler)]() mutable {
                   keepaliveTimer_.cancel();
                   Packet disc;
                   disc.type_ = PacketType::Disconnect;
                   std::array<std::byte, 16> buf{};
                   auto n = codec_.encode(disc, boost::asio::buffer(buf));
                   sock_.asyncSendTo(boost::asio::buffer(buf.data(), n), remote_,
                                     [h = std::move(h)](std::error_code ec, std::size_t) mutable {
                                        h(ec);
                                     });
                });
             },
             token);
      }

      // ── Accessors ─────────────────────────────────────────────────────────────

      [[nodiscard]] Executor executor() const noexcept {
         return strand_.get_inner_executor();
      }
      [[nodiscard]] Strand& strand() noexcept {
         return strand_;
      }
      [[nodiscard]] boost::asio::ip::udp::endpoint remoteEndpoint() const noexcept override {
         return remote_;
      }
      [[nodiscard]] SocketStats::Snapshot stats() const noexcept override {
         return stats_.snapshot();
      }

      void cancel() noexcept override {
         boost::asio::dispatch(strand_, [this] {
            keepaliveTimer_.cancel();
            sock_.cancel();
         });
      }

      /// Configure a channel. Call before starting the recv loop.
      void setChannel(ChannelConfig const& cfg) {
         if (cfg.id_ >= kMaxChannels) {
            return;
         }

         auto& slot = channels_[cfg.id_];
         switch (cfg.reliability_) {
            case ReliabilityMode::Reliable:
               if (cfg.ordering_ == OrderingMode::Ordered)
                  slot = ReliableOrderedChannel<OrderedCC>{cfg, strand_};
               else
                  slot = ReliableUnorderedChannel<UnorderedCC>{cfg, strand_};
               break;
            case ReliabilityMode::Unreliable:
               slot = UnreliableChannel{cfg};
               break;
         }
      }

   private:
      Strand strand_;
      Sock sock_;
      Codec codec_;
      Policy policy_;
      SessionConfig config_;
      boost::asio::ip::udp::endpoint remote_;
      boost::asio::steady_timer keepaliveTimer_;
      std::array<ChannelVar, kMaxChannels> channels_;
      SocketStats stats_;
   };

   // ── Default alias ──────────────────────────────────────────────────────────────
   // Most users never touch the template parameters.

   using Session = BasicSession<DefaultCodec, LeakyBucket, LeakyBucket, DefaultPolicy, UdpSocket>;

} // namespace rude

#endif // RUDE_SESSION_BASICSESSION_HPP
