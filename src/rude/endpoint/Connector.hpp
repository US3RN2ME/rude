
#ifndef RUDE_ENDPOINT_CONNECTOR_HPP
#define RUDE_ENDPOINT_CONNECTOR_HPP

#include <boost/asio/async_result.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/system/error_code.hpp>
#include <memory>
#include <rude/concepts/CongestionControl.hpp>
#include <rude/concepts/PacketCodec.hpp>
#include <rude/concepts/SessionPolicy.hpp>
#include <rude/concepts/Socket.hpp>
#include <rude/core/Executor.hpp>
#include <rude/detail/DefaultCodec.hpp>
#include <rude/detail/Handshake.hpp>
#include <rude/detail/UdpSocket.hpp>
#include <rude/detail/congestion/LeakyBucket.hpp>
#include <rude/session/BasicSession.hpp>
#include <rude/session/SessionConfig.hpp>
#include <utility>

namespace rude {

   /// Initiates an outgoing RUDP connection to a remote endpoint.
   ///
   /// Example (coroutine):
   ///   Connector connector{executor};
   ///   auto session = co_await connector.asyncConnect(
   ///      {address::from_string("127.0.0.1"), 9000}, asio::use_awaitable);
   ///   co_await session->asyncSend(0, payload, asio::use_awaitable);
   template <PacketCodec Codec = DefaultCodec, CongestionCtrl OrderedCC = LeakyBucket, CongestionCtrl UnorderedCC = LeakyBucket,
             SessionPolicy Policy = DefaultPolicy, Socket Sock = UdpSocket>
   class BasicConnector {
   public:
      using SessionType = BasicSession<Codec, OrderedCC, UnorderedCC, Policy, Sock>;
      using SessionPtr = std::shared_ptr<SessionType>;

      explicit BasicConnector(Executor executor)
          : executor_{std::move(executor)} {}

      /// Connect to remote. Completes with a started session on success.
      /// Completion signature: void(boost::system::error_code, SessionPtr).
      template <boost::asio::completion_token_for<void(boost::system::error_code, SessionPtr)> Token>
      auto asyncConnect(boost::asio::ip::udp::endpoint remote, Token&& token) {
         return boost::asio::async_initiate<Token, void(boost::system::error_code, SessionPtr)>(
             [this, remote](auto handler) {
                auto session = SessionType::create(executor_, remote.protocol(), codec_, policy_, config_);
                pending_ = session;
                detail::Handshake::asyncClientHandshake(
                    session->socket(), remote, codec_, config_, Executor{session->strand()},
                    [session, h = std::move(handler)](boost::system::error_code ec,
                                                      boost::asio::ip::udp::endpoint adopted) mutable {
                       if (ec) {
                          std::move(h)(ec, SessionPtr{});
                          return;
                       }
                       session->start(adopted);
                       std::move(h)(boost::system::error_code{}, session);
                    });
             },
             token);
      }

      // ── Fluent configuration ──────────────────────────────────────────────────

      BasicConnector& withConfig(SessionConfig cfg) {
         config_ = std::move(cfg);
         return *this;
      }
      BasicConnector& withCodec(Codec codec) {
         codec_ = std::move(codec);
         return *this;
      }
      BasicConnector& withPolicy(Policy policy) {
         policy_ = std::move(policy);
         return *this;
      }

      /// Abort an in-flight asyncConnect (its handler completes with an error).
      void cancel() {
         if (auto session = pending_.lock()) {
            session->cancel();
         }
      }

   private:
      Executor executor_;
      Codec codec_{};
      Policy policy_{};
      SessionConfig config_{};
      std::weak_ptr<SessionType> pending_;
   };

   // ── Default alias ──────────────────────────────────────────────────────────────

   using Connector = BasicConnector<>;

} // namespace rude

#endif // RUDE_ENDPOINT_CONNECTOR_HPP
