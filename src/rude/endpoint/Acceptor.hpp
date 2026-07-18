
#ifndef RUDE_ENDPOINT_ACCEPTOR_HPP
#define RUDE_ENDPOINT_ACCEPTOR_HPP

#include <array>
#include <boost/asio/async_result.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/system/error_code.hpp>
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
#include <rude/detail/congestion/LeakyBucket.hpp>
#include <rude/protocol/Packet.hpp>
#include <rude/protocol/Types.hpp>
#include <rude/session/BasicSession.hpp>
#include <rude/session/SessionConfig.hpp>
#include <utility>
#include <vector>

namespace rude {

   /// Listens for incoming RUDP connections and produces one session per peer.
   ///
   /// Each accepted session gets its own UDP socket bound to an ephemeral port;
   /// the handshake ACK is sent from that socket so all further traffic flows
   /// peer-socket to peer-socket (TFTP style) and sessions never contend for
   /// the listen socket. Keep exactly one asyncAccept outstanding at a time.
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
      using SessionPtr = std::shared_ptr<SessionType>;

      explicit BasicAcceptor(Executor executor, boost::asio::ip::udp::endpoint localEp)
          : executor_{std::move(executor)}
          , sock_{executor_, localEp} {}

      /// Accept one incoming connection. Completes with a started session.
      /// Completion signature: void(boost::system::error_code, SessionPtr).
      template <boost::asio::completion_token_for<void(boost::system::error_code, SessionPtr)> Token>
      auto asyncAccept(Token&& token) {
         return boost::asio::async_initiate<Token, void(boost::system::error_code, SessionPtr)>(
             [this](auto handler) {
                startAccept(std::move(handler));
             },
             token);
      }

      // ── Fluent configuration ──────────────────────────────────────────────────

      BasicAcceptor& withConfig(SessionConfig cfg) {
         config_ = std::move(cfg);
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
      static constexpr std::size_t kRecvBufSize = 1500;

      template <typename Handler>
      void startAccept(Handler handler) {
         sock_.asyncRecvFrom(boost::asio::buffer(recvBuf_), sender_,
                             [this, h = std::move(handler)](boost::system::error_code ec, std::size_t size) mutable {
                                if (ec) {
                                   std::move(h)(ec, SessionPtr{});
                                   return;
                                }

                                const auto decoded = codec_.decode(boost::asio::buffer(recvBuf_.data(), size));
                                if (!decoded || decoded->type_ != PacketType::HandshakeInit) {
                                   // Junk or non-handshake traffic on the listen
                                   // port: keep listening.
                                   startAccept(std::move(h));
                                   return;
                                }

                                openSession(sender_, decoded->seq_, std::move(h));
                             });
      }

      template <typename Handler>
      void openSession(boost::asio::ip::udp::endpoint client, std::uint16_t initSeq, Handler handler) {
         auto session = SessionType::create(executor_, client.protocol(), codec_, policy_, config_);

         Packet ack;
         ack.type_ = PacketType::HandshakeAck;
         ack.ack_ = initSeq;

         auto encoded = std::make_shared<std::vector<std::byte>>(codec_.maxOverhead());
         const auto encodedSize = codec_.encode(ack, boost::asio::buffer(*encoded));
         if (encodedSize == 0) {
            std::move(handler)(makeErrorCode(Error::BadPacket), SessionPtr{});
            return;
         }
         encoded->resize(encodedSize);

         // Answer from the session's own socket so the client adopts it as the
         // remote and the listen socket stays free for new handshakes.
         session->socket().asyncSendTo(
             boost::asio::buffer(*encoded), client,
             [session, client, encoded, h = std::move(handler)](boost::system::error_code ec, std::size_t) mutable {
                if (ec) {
                   std::move(h)(ec, SessionPtr{});
                   return;
                }
                session->start(client);
                std::move(h)(boost::system::error_code{}, session);
             });
      }

      Executor executor_;
      Sock sock_;
      Codec codec_{};
      Policy policy_{};
      SessionConfig config_{};
      std::array<std::byte, kRecvBufSize> recvBuf_{};
      boost::asio::ip::udp::endpoint sender_;
   };

   // ── Default alias ──────────────────────────────────────────────────────────────

   using Acceptor = BasicAcceptor<>;

} // namespace rude

#endif // RUDE_ENDPOINT_ACCEPTOR_HPP
