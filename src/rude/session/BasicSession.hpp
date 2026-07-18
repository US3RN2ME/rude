
#ifndef RUDE_SESSION_BASICSESSION_HPP
#define RUDE_SESSION_BASICSESSION_HPP

#include <algorithm>
#include <array>
#include <boost/asio/buffer.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <rude/concepts/CongestionControl.hpp>
#include <rude/concepts/PacketCodec.hpp>
#include <rude/concepts/SessionPolicy.hpp>
#include <rude/concepts/Socket.hpp>
#include <rude/core/Error.hpp>
#include <rude/core/Executor.hpp>
#include <rude/detail/DefaultCodec.hpp>
#include <rude/detail/UdpSocket.hpp>
#include <rude/detail/channel/ChannelVariant.hpp>
#include <rude/detail/congestion/LeakyBucket.hpp>
#include <rude/protocol/Packet.hpp>
#include <rude/session/ChannelConfig.hpp>
#include <rude/session/SessionConfig.hpp>
#include <rude/session/SocketStats.hpp>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace rude {

   /**
    * @brief Type-erased session interface exposed to session policies.
    *
    * Policy callbacks receive this base class so they can inspect endpoint
    * identity, read statistics, or cancel a session without depending on the
    * full BasicSession template instantiation.
    */
   class SessionBase {
   public:
      virtual ~SessionBase() = default;

      [[nodiscard]] virtual boost::asio::ip::udp::endpoint remoteEndpoint() const noexcept = 0;
      [[nodiscard]] virtual SocketStats::Snapshot stats() const noexcept = 0;
      virtual void cancel() noexcept = 0;
   };

   /**
    * @brief Owns transport, codec, policy, and per-channel state for one peer.
    *
    * A session is created by BasicAcceptor or BasicConnector and handed to the
    * application as std::shared_ptr. It owns its UDP socket and drives its own
    * receive loop, retransmission, and keepalive timers, so after connect the
    * only calls an application needs are asyncSend / asyncRecv /
    * asyncDisconnect.
    *
    * All asynchronous operations are dispatched through the session strand and
    * the socket itself runs on that strand, so callers may initiate operations
    * from multiple threads while channel state remains serialized internally.
    *
    * @tparam Codec       Packet codec satisfying PacketCodec.
    * @tparam OrderedCC   Congestion controller for reliable ordered channels.
    * @tparam UnorderedCC Congestion controller for reliable unordered channels.
    * @tparam Policy      Lifecycle callback policy satisfying SessionPolicy.
    * @tparam Sock        UDP-like transport satisfying Socket, constructible
    *                     from (Executor, udp::endpoint).
    */
   template <PacketCodec Codec, CongestionCtrl OrderedCC, CongestionCtrl UnorderedCC, SessionPolicy Policy, Socket Sock>
   class BasicSession : public SessionBase,
                        public std::enable_shared_from_this<BasicSession<Codec, OrderedCC, UnorderedCC, Policy, Sock>> {
   public:
      using ChannelVar = ChannelVariant<OrderedCC, UnorderedCC>;
      using Clock = std::chrono::steady_clock;

      static constexpr std::uint8_t kMaxChannels = 8;

      /// Create a session bound to a fresh local UDP socket. Used by
      /// BasicAcceptor / BasicConnector; call start() once the peer is known.
      [[nodiscard]] static std::shared_ptr<BasicSession> create(Executor executor, boost::asio::ip::udp protocol, Codec codec,
                                                                Policy policy, SessionConfig config) {
         return std::shared_ptr<BasicSession>{
             new BasicSession{std::move(executor), protocol, std::move(codec), std::move(policy), std::move(config)}};
      }

      BasicSession(BasicSession const&) = delete;
      BasicSession& operator=(BasicSession const&) = delete;

      // ── Async operations ──────────────────────────────────────────────────────

      /// Send payload on channel ch. Completion: void(boost::system::error_code).
      /// The payload must stay alive until the completion handler runs.
      /// Supports any Asio CompletionToken: co_await, callback, use_future, deferred.
      template <boost::asio::completion_token_for<void(boost::system::error_code)> Token>
      auto asyncSend(std::uint8_t ch, std::span<std::byte const> payload, Token&& token) {
         return boost::asio::async_initiate<Token, void(boost::system::error_code)>(
             [this, ch, payload](auto handler) {
                boost::asio::dispatch(
                    strand_, [self = this->shared_from_this(), ch, payload, h = std::move(handler)]() mutable {
                       if (self->closed_ || ch >= kMaxChannels || !self->channelOpen_[ch]) {
                          std::move(h)(makeErrorCode(Error::ChannelClosed));
                          return;
                       }
                       if (payload.size() + self->codec_.maxOverhead() > self->config_.mtu_) {
                          std::move(h)(makeErrorCode(Error::MessageTooLarge));
                          return;
                       }
                       std::visit(
                           [&](auto& channel) {
                              channel.asyncSend(payload, self->sock_, self->remote_, self->codec_, self->stats_, std::move(h));
                           },
                           self->channels_[ch]);
                    });
             },
             token);
      }

      /// Receive the next message on channel ch into buf.
      /// Completion: void(boost::system::error_code, std::size_t bytesReceived).
      template <boost::asio::completion_token_for<void(boost::system::error_code, std::size_t)> Token>
      auto asyncRecv(std::uint8_t ch, boost::asio::mutable_buffer buf, Token&& token) {
         return boost::asio::async_initiate<Token, void(boost::system::error_code, std::size_t)>(
             [this, ch, buf](auto handler) {
                boost::asio::dispatch(strand_, [self = this->shared_from_this(), ch, buf, h = std::move(handler)]() mutable {
                   if (ch >= kMaxChannels || !self->channelOpen_[ch]) {
                      std::move(h)(makeErrorCode(Error::ChannelClosed), std::size_t{0});
                      return;
                   }
                   std::visit(
                       [&](auto& channel) {
                          channel.asyncRecv(buf, detail::channel::RecvHandler{std::move(h)});
                       },
                       self->channels_[ch]);
                });
             },
             token);
      }

      /// Immediately resend every unacknowledged reliable packet on all
      /// channels. Normally unnecessary — the session retransmits expired
      /// packets automatically. Completion: void(boost::system::error_code).
      template <boost::asio::completion_token_for<void(boost::system::error_code)> Token>
      auto asyncFlush(Token&& token) {
         return boost::asio::async_initiate<Token, void(boost::system::error_code)>(
             [this](auto handler) {
                boost::asio::dispatch(strand_, [self = this->shared_from_this(), h = std::move(handler)]() mutable {
                   boost::system::error_code ec;
                   if (self->closed_) {
                      ec = makeErrorCode(Error::ChannelClosed);
                   } else {
                      for (std::uint8_t ch = 0; ch < kMaxChannels; ++ch) {
                         if (!self->channelOpen_[ch]) {
                            continue;
                         }
                         std::visit(
                             [&](auto& channel) {
                                channel.flush(self->sock_, self->remote_, self->codec_, ec);
                             },
                             self->channels_[ch]);
                      }
                   }
                   std::move(h)(ec);
                });
             },
             token);
      }

      /// Send DISCONNECT to the peer and close the session.
      /// Completion: void(boost::system::error_code) with the result of the final send.
      template <boost::asio::completion_token_for<void(boost::system::error_code)> Token>
      auto asyncDisconnect(Token&& token) {
         return boost::asio::async_initiate<Token, void(boost::system::error_code)>(
             [this](auto handler) {
                boost::asio::dispatch(strand_, [self = this->shared_from_this(), h = std::move(handler)]() mutable {
                   if (self->closed_) {
                      std::move(h)(boost::system::error_code{});
                      return;
                   }
                   Packet disc;
                   disc.type_ = PacketType::Disconnect;
                   auto encoded = self->encodePacket(disc);
                   if (encoded.empty()) {
                      self->close(makeErrorCode(Error::BadPacket));
                      std::move(h)(makeErrorCode(Error::BadPacket));
                      return;
                   }
                   detail::channel::sendEncoded(self->sock_, self->remote_, std::move(encoded),
                                                [self, h = std::move(h)](boost::system::error_code ec) mutable {
                                                   self->close({});
                                                   std::move(h)(ec);
                                                });
                });
             },
             token);
      }

      // ── Lifecycle ─────────────────────────────────────────────────────────────

      /// Begin the receive loop and timers against the given peer. Called once
      /// by BasicAcceptor / BasicConnector after the handshake succeeds.
      void start(boost::asio::ip::udp::endpoint remote) {
         boost::asio::dispatch(strand_, [self = this->shared_from_this(), remote] {
            if (self->started_ || self->closed_) {
               return;
            }
            self->started_ = true;
            self->remote_ = remote;
            self->lastRecv_ = Clock::now();
            self->policy_.onConnected(*self);
            self->receiveNext();
            self->scheduleRetransmit();
            self->scheduleKeepalive();
         });
      }

      void cancel() noexcept override {
         boost::asio::dispatch(strand_, [self = this->shared_from_this()] {
            self->close(boost::asio::error::operation_aborted);
         });
      }

      // ── Accessors ─────────────────────────────────────────────────────────────

      [[nodiscard]] Executor executor() const noexcept {
         return strand_.get_inner_executor();
      }
      [[nodiscard]] Strand& strand() noexcept {
         return strand_;
      }
      /// The session transport. Exposed for the handshake performed by
      /// BasicAcceptor / BasicConnector before start(); applications normally
      /// never touch it.
      [[nodiscard]] Sock& socket() noexcept {
         return sock_;
      }
      [[nodiscard]] boost::asio::ip::udp::endpoint remoteEndpoint() const noexcept override {
         return remote_;
      }
      [[nodiscard]] boost::asio::ip::udp::endpoint localEndpoint() const {
         return sock_.localEndpoint();
      }
      [[nodiscard]] SocketStats::Snapshot stats() const noexcept override {
         return stats_.snapshot();
      }
      [[nodiscard]] SessionConfig const& config() const noexcept {
         return config_;
      }
      [[nodiscard]] Codec& codec() noexcept {
         return codec_;
      }

   private:
      explicit BasicSession(Executor executor, boost::asio::ip::udp protocol, Codec codec, Policy policy, SessionConfig config)
          : strand_{std::move(executor)}
          , sock_{Executor{strand_}, boost::asio::ip::udp::endpoint{protocol, 0}}
          , codec_{std::move(codec)}
          , policy_{std::move(policy)}
          , config_{std::move(config)}
          , keepaliveTimer_{strand_}
          , retxTimer_{strand_}
          , recvBuf_(std::max<std::size_t>(config_.recvBufSize_, 2048)) {
         for (auto const& channelCfg : config_.channels_) {
            openChannel(channelCfg);
         }
         retxTick_ = retransmitTick();
      }

      // ── Channel setup ─────────────────────────────────────────────────────────

      void openChannel(ChannelConfig const& cfg) {
         if (cfg.id_ >= kMaxChannels) {
            return;
         }

         auto& slot = channels_[cfg.id_];
         switch (cfg.mode_) {
            case ChannelMode::ReliableOrdered:
               slot = ReliableOrderedChannel<OrderedCC>{cfg.reliableOptions(), strand_};
               break;
            case ChannelMode::ReliableUnordered:
               slot = ReliableUnorderedChannel<UnorderedCC>{cfg.reliableOptions(), strand_};
               break;
            case ChannelMode::UnreliableSequenced:
               slot = UnreliableChannel{cfg.unreliableOptions()};
               break;
         }
         channelOpen_[cfg.id_] = true;
      }

      /// Retransmit timer granularity: half the smallest configured reliable
      /// retransmit timeout, clamped to [10ms, 500ms].
      [[nodiscard]] std::chrono::milliseconds retransmitTick() const noexcept {
         std::uint32_t smallest = 200;
         for (auto const& cfg : config_.channels_) {
            if (cfg.mode_ != ChannelMode::UnreliableSequenced && cfg.retxTimeoutMs_ > 0) {
               smallest = std::min(smallest, cfg.retxTimeoutMs_);
            }
         }
         return std::chrono::milliseconds{std::clamp<std::uint32_t>(smallest / 2, 10, 500)};
      }

      // ── Receive loop ──────────────────────────────────────────────────────────

      void receiveNext() {
         sock_.asyncRecvFrom(boost::asio::buffer(recvBuf_), sender_,
                             [self = this->shared_from_this()](boost::system::error_code ec, std::size_t size) {
                                self->onSocketRecv(ec, size);
                             });
      }

      void onSocketRecv(boost::system::error_code ec, std::size_t size) {
         // close() runs on this strand and sets closed_ before cancelling the
         // socket, so an aborted receive always lands here with closed_ set.
         if (closed_) {
            return;
         }
         if (ec) {
            close(ec);
            return;
         }

         if (sender_ == remote_) {
            handlePacket(size);
         }
         if (!closed_) {
            receiveNext();
         }
      }

      void handlePacket(std::size_t size) {
         const auto decoded = codec_.decode(boost::asio::buffer(recvBuf_.data(), size));
         if (!decoded) {
            // Corrupt or foreign datagram; UDP junk must never kill the session.
            return;
         }

         lastRecv_ = Clock::now();

         switch (decoded->type_) {
            case PacketType::Disconnect:
               close(makeErrorCode(Error::ConnectionReset));
               return;
            case PacketType::Keepalive:
               return;
            case PacketType::HandshakeInit: {
               // The peer retried its handshake because our ACK got lost.
               Packet ack;
               ack.type_ = PacketType::HandshakeAck;
               ack.ack_ = decoded->seq_;
               sendPacket(ack);
               return;
            }
            case PacketType::HandshakeAck:
               return;
            default:
               break;
         }

         const auto ch = decoded->channel_;
         if (ch >= kMaxChannels || !channelOpen_[ch]) {
            return;
         }

         std::visit(
             [&](auto& channel) {
                channel.onRecv(*decoded, stats_);
                if (decoded->type_ != PacketType::Data) {
                   return;
                }
                if (auto ack = channel.buildAck()) {
                   sendPacket(*ack);
                }
             },
             channels_[ch]);
      }

      // ── Timers ────────────────────────────────────────────────────────────────

      void scheduleRetransmit() {
         retxTimer_.expires_after(retxTick_);
         retxTimer_.async_wait([self = this->shared_from_this()](boost::system::error_code ec) {
            if (ec || self->closed_) {
               return;
            }
            const auto now = Clock::now();
            for (std::uint8_t ch = 0; ch < kMaxChannels; ++ch) {
               if (!self->channelOpen_[ch]) {
                  continue;
               }
               std::visit(
                   [&](auto& channel) {
                      channel.onTick(now, self->sock_, self->remote_, self->stats_);
                   },
                   self->channels_[ch]);
            }
            self->scheduleRetransmit();
         });
      }

      void scheduleKeepalive() {
         keepaliveTimer_.expires_after(std::chrono::milliseconds{config_.keepaliveMs_});
         keepaliveTimer_.async_wait([self = this->shared_from_this()](boost::system::error_code ec) {
            if (ec || self->closed_) {
               return;
            }
            if (Clock::now() - self->lastRecv_ > std::chrono::milliseconds{self->config_.timeoutMs_}) {
               self->close(makeErrorCode(Error::Timeout));
               return;
            }
            Packet keepalive;
            keepalive.type_ = PacketType::Keepalive;
            self->sendPacket(keepalive);
            self->scheduleKeepalive();
         });
      }

      // ── Helpers ───────────────────────────────────────────────────────────────

      [[nodiscard]] std::vector<std::byte> encodePacket(Packet const& pkt) {
         std::vector<std::byte> encoded(pkt.payload_.size() + codec_.maxOverhead());
         const auto size = codec_.encode(pkt, boost::asio::buffer(encoded));
         encoded.resize(size);
         return encoded;
      }

      void sendPacket(Packet const& pkt) {
         auto encoded = encodePacket(pkt);
         if (encoded.empty()) {
            return;
         }
         detail::channel::sendEncoded(sock_, remote_, std::move(encoded), [](boost::system::error_code) {});
      }

      /// Tear down the session exactly once: stop timers, close the socket,
      /// fail pending receives, and notify the policy.
      void close(boost::system::error_code reason) {
         if (closed_) {
            return;
         }
         closed_ = true;

         keepaliveTimer_.cancel();
         retxTimer_.cancel();
         sock_.cancel();
         sock_.close();

         for (std::uint8_t ch = 0; ch < kMaxChannels; ++ch) {
            if (!channelOpen_[ch]) {
               continue;
            }
            std::visit(
                [&](auto& channel) {
                   channel.close(reason ? reason : makeErrorCode(Error::ChannelClosed));
                },
                channels_[ch]);
         }

         policy_.onDisconnected(reason);
      }

      Strand strand_;
      Sock sock_;
      Codec codec_;
      Policy policy_;
      SessionConfig config_;
      boost::asio::ip::udp::endpoint remote_;
      boost::asio::steady_timer keepaliveTimer_;
      boost::asio::steady_timer retxTimer_;
      std::array<ChannelVar, kMaxChannels> channels_;
      std::array<bool, kMaxChannels> channelOpen_{};
      std::vector<std::byte> recvBuf_;
      boost::asio::ip::udp::endpoint sender_;
      SocketStats stats_;
      Clock::time_point lastRecv_{};
      std::chrono::milliseconds retxTick_{100};
      bool started_ = false;
      bool closed_ = false;
   };

   // ── Default aliases ────────────────────────────────────────────────────────────
   // Most users never touch the template parameters.

   using Session = BasicSession<DefaultCodec, LeakyBucket, LeakyBucket, DefaultPolicy, UdpSocket>;
   using SessionPtr = std::shared_ptr<Session>;

} // namespace rude

#endif // RUDE_SESSION_BASICSESSION_HPP
