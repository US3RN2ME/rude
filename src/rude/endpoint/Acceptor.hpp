
#ifndef RUDE_ENDPOINT_ACCEPTOR_HPP
#define RUDE_ENDPOINT_ACCEPTOR_HPP

#include <boost/asio/async_result.hpp>
#include <boost/asio/ip/udp.hpp>
#include <rude/concepts/CongestionControl.hpp>
#include <rude/concepts/PacketCodec.hpp>
#include <rude/concepts/SessionPolicy.hpp>
#include <rude/concepts/Socket.hpp>
#include <rude/core/Executor.hpp>
#include <rude/detail/DefaultCodec.hpp>
#include <rude/detail/UdpSocket.hpp>
#include <rude/detail/congestion/LeakyBucket.hpp>
#include <rude/detail/handshake.hpp>
#include <rude/session/BasicSession.hpp>
#include <rude/session/SessionConfig.hpp>

namespace rude {

   /// Listens for incoming RUDP connections and produces a BasicSession per peer.
   /// Wrap in an accept loop using co_await or a callback to handle multiple clients.
   ///
   /// Example (coroutine):
   ///   Acceptor acceptor{executor, udp::endpoint{udp::v4(), 9000}};
   ///   while (true) {
   ///      auto session = co_await acceptor.asyncAccept(asio::use_awaitable);
   ///      co_spawn(executor, handleClient(std::move(session)), asio::detached);
   ///   }
   template <PacketCodec Codec = DefaultCodec, CongestionCtrl OrderedCC = LeakyBucket, CongestionCtrl UnorderedCC = LeakyBucket,
             SessionPolicy Policy = DefaultPolicy, Socket Sock = UdpSocket>
   class BasicAcceptor {
   public:
      using SessionType = BasicSession<Codec, OrderedCC, UnorderedCC, Policy, Sock>;

      explicit BasicAcceptor(Executor executor, boost::asio::ip::udp::endpoint localEp)
          : executor_{std::move(executor)}
          , sock_{executor_, localEp} {}

      /// Accept one incoming connection. Completes with the new Session.
      /// Completion signature: void(std::error_code, SessionType).
      template <boost::asio::completion_token_for<void(std::error_code, SessionType)> Token>
      auto asyncAccept(Token&& token) {
         return boost::asio::async_initiate<Token, void(std::error_code, SessionType)>(
             [this](auto handler) {
                detail::Handshake::asyncServerHandshake(
                    sock_, codec_, config_,
                    [this, h = std::move(handler)](std::error_code ec, boost::asio::ip::udp::endpoint remote) mutable {
                       if (ec) {
                          h(ec, SessionType{});
                          return;
                       }
                       h(ec, SessionType{executor_, sock_, codec_, policy_, config_, remote});
                    });
             },
             token);
      }

      // ── Fluent configuration ──────────────────────────────────────────────────

      BasicAcceptor& withConfig(SessionConfig cfg) {
         config_ = cfg;
         return *this;
      }
      BasicAcceptor& withCodec(Codec codec) {
         codec_ = std::move(codec);
         return *this;
      }
      BasicAcceptor& withPolicy(Policy policy) {
         policy_ = std::move(policy);
         return *this;
      }

      [[nodiscard]] boost::asio::ip::udp::endpoint localEndpoint() const {
         return sock_.localEndpoint();
      }
      void cancel() {
         sock_.cancel();
      }
      void close() {
         sock_.close();
      }

   private:
      Executor executor_;
      Sock sock_;
      Codec codec_{};
      Policy policy_{};
      SessionConfig config_{};
   };

   // ── Default alias ──────────────────────────────────────────────────────────────

   using Acceptor = BasicAcceptor<>;

} // namespace rude

#endif // RUDE_ENDPOINT_ACCEPTOR_HPP
