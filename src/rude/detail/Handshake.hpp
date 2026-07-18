#ifndef RUDE_DETAIL_HANDSHAKE_HPP
#define RUDE_DETAIL_HANDSHAKE_HPP

#include <algorithm>
#include <array>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <cstddef>
#include <memory>
#include <rude/concepts/PacketCodec.hpp>
#include <rude/concepts/Socket.hpp>
#include <rude/core/Error.hpp>
#include <rude/core/Executor.hpp>
#include <rude/protocol/Packet.hpp>
#include <rude/protocol/Types.hpp>
#include <rude/session/SessionConfig.hpp>
#include <utility>

namespace rude::detail {

   /**
    * @brief Client side of the two-packet UDP handshake.
    *
    * Sends HANDSHAKE_INIT to the server and waits for HANDSHAKE_ACK. The INIT
    * is retransmitted at a fixed interval until the ACK arrives or the
    * SessionConfig::maxHandshakeMs_ deadline expires.
    *
    * The ACK may arrive from a different port than the one the INIT was sent
    * to: the acceptor answers from a per-session socket (TFTP style). The
    * handshake therefore only requires the sender address to match, and it
    * completes with the endpoint the session should adopt as its remote.
    *
    * The caller supplies the executor its socket runs on so timer and socket
    * completions are serialized together.
    */
   class Handshake {
   public:
      using Clock = std::chrono::steady_clock;

      /// Completion: void(boost::system::error_code, udp::endpoint adoptedRemote).
      template <Socket Sock, PacketCodec Codec, typename Handler>
      static void asyncClientHandshake(Sock& sock, boost::asio::ip::udp::endpoint server, Codec codec, SessionConfig const& cfg,
                                       Executor executor, Handler handler) {
         const auto timeout = std::chrono::milliseconds{cfg.maxHandshakeMs_ == 0 ? 5000 : cfg.maxHandshakeMs_};
         const auto retry = std::clamp<std::chrono::milliseconds>(timeout / 5, std::chrono::milliseconds{100},
                                                                  std::chrono::milliseconds{1000});

         auto state = std::make_shared<ClientState<Codec, Handler>>(server, std::move(codec), std::move(handler),
                                                                    std::move(executor), Clock::now() + timeout, retry);
         sendInit(sock, state);
         receiveNext(sock, state);
         scheduleRetry(sock, state);
      }

   private:
      static constexpr std::size_t bufferSize = 1500;

      template <typename Codec, typename Handler>
      struct ClientState {
         ClientState(boost::asio::ip::udp::endpoint serverEndpoint, Codec packetCodec, Handler completionHandler,
                     Executor executor, Clock::time_point deadlineAt, std::chrono::milliseconds retryEvery)
             : server{serverEndpoint}
             , codec{std::move(packetCodec)}
             , handler{std::move(completionHandler)}
             , timer{std::move(executor)}
             , deadline{deadlineAt}
             , retryInterval{retryEvery} {}

         boost::asio::ip::udp::endpoint server;
         boost::asio::ip::udp::endpoint sender;
         std::array<std::byte, bufferSize> sendBuffer{};
         std::array<std::byte, bufferSize> recvBuffer{};
         Codec codec;
         Handler handler;
         boost::asio::steady_timer timer;
         Clock::time_point deadline;
         std::chrono::milliseconds retryInterval;
         bool done = false;
      };

      template <typename State>
      static void complete(State& state, boost::system::error_code ec, boost::asio::ip::udp::endpoint adopted) {
         if (state->done) {
            return;
         }
         state->done = true;
         state->timer.cancel();
         std::move(state->handler)(ec, adopted);
      }

      template <Socket Sock, typename State>
      static void sendInit(Sock& sock, std::shared_ptr<State> state) {
         Packet init;
         init.type_ = PacketType::HandshakeInit;

         const auto encodedSize = state->codec.encode(init, boost::asio::buffer(state->sendBuffer));
         if (encodedSize == 0) {
            complete(state, makeErrorCode(Error::BadPacket), {});
            return;
         }

         sock.asyncSendTo(boost::asio::buffer(state->sendBuffer.data(), encodedSize), state->server,
                          [state](boost::system::error_code ec, std::size_t) {
                             // Send failures are not fatal: the retry timer will try
                             // again until the deadline expires.
                             static_cast<void>(ec);
                          });
      }

      template <Socket Sock, typename State>
      static void receiveNext(Sock& sock, std::shared_ptr<State> state) {
         sock.asyncRecvFrom(boost::asio::buffer(state->recvBuffer), state->sender,
                            [&sock, state](boost::system::error_code ec, std::size_t size) {
                               if (state->done) {
                                  return;
                               }
                               if (ec) {
                                  complete(state, ec, {});
                                  return;
                               }

                               const auto decoded = state->codec.decode(boost::asio::buffer(state->recvBuffer.data(), size));
                               if (decoded && decoded->type_ == PacketType::HandshakeAck &&
                                   state->sender.address() == state->server.address()) {
                                  complete(state, {}, state->sender);
                                  return;
                               }

                               // Junk or unrelated datagram: keep waiting.
                               receiveNext(sock, state);
                            });
      }

      template <Socket Sock, typename State>
      static void scheduleRetry(Sock& sock, std::shared_ptr<State> state) {
         state->timer.expires_after(state->retryInterval);
         state->timer.async_wait([&sock, state](boost::system::error_code ec) {
            if (ec || state->done) {
               return;
            }
            if (Clock::now() >= state->deadline) {
               complete(state, makeErrorCode(Error::Timeout), {});
               sock.cancel();
               return;
            }
            sendInit(sock, state);
            scheduleRetry(sock, state);
         });
      }
   };

} // namespace rude::detail

#endif // RUDE_DETAIL_HANDSHAKE_HPP
