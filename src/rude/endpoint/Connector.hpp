
#ifndef RUDE_ENDPOINT_CONNECTOR_HPP
#define RUDE_ENDPOINT_CONNECTOR_HPP

#include <boost/asio/ip/udp.hpp>
#include <memory>
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

   /// Initiates an outgoing RUDP connection to a remote endpoint.
   ///
   /// Example (coroutine):
   ///   Connector connector{executor};
   ///   auto session = co_await connector.asyncConnect(
   ///      {address::from_string("127.0.0.1"), 9000}, asio::use_awaitable);
   template <PacketCodec Codec = DefaultCodec, CongestionCtrl OrderedCC = LeakyBucket, CongestionCtrl UnorderedCC = LeakyBucket,
             SessionPolicy Policy = DefaultPolicy, Socket Sock = UdpSocket>
   class BasicConnector {
   public:
      using SessionType = BasicSession<Codec, OrderedCC, UnorderedCC, Policy, Sock>;

      explicit BasicConnector(Executor executor)
          : executor_{std::move(executor)} {}

      /// Connect to remote. Completes with the new Session on success.
      /// Completion signature: void(std::error_code, SessionType).
      template <boost::asio::completion_token_for<void(std::error_code, SessionType)> Token>
      auto asyncConnect(boost::asio::ip::udp::endpoint remote, Token&& token) {
         return boost::asio::async_initiate<Token, void(std::error_code, SessionType)>(
             [this, remote](auto handler) {
                auto sock = std::make_shared<Sock>(executor_, boost::asio::ip::udp::endpoint{boost::asio::ip::udp::v4(), 0});
                detail::Handshake::asyncClientHandshake(
                    *sock, remote, codec_, config_, [this, remote, sock, h = std::move(handler)](std::error_code ec) mutable {
                       if (ec) {
                          h(ec, SessionType{});
                          return;
                       }
                       h(ec, SessionType{executor_, std::move(*sock), codec_, policy_, config_, remote});
                    });
             },
             token);
      }

      // ── Fluent configuration ──────────────────────────────────────────────────

      BasicConnector& withConfig(SessionConfig cfg) {
         config_ = cfg;
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

      void cancel() { /* cancel pending asyncConnect via stored socket */ }

   private:
      Executor executor_;
      Codec codec_{};
      Policy policy_{};
      SessionConfig config_{};
   };

   // ── Default alias ──────────────────────────────────────────────────────────────

   using Connector = BasicConnector<>;

} // namespace rude

#endif // RUDE_ENDPOINT_CONNECTOR_HPP
