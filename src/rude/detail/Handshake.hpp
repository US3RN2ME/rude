#ifndef RUDE_DETAIL_HANDSHAKE_HPP
#define RUDE_DETAIL_HANDSHAKE_HPP

#include <array>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <cstddef>
#include <memory>
#include <rude/concepts/PacketCodec.hpp>
#include <rude/concepts/Socket.hpp>
#include <rude/core/Error.hpp>
#include <rude/protocol/Packet.hpp>
#include <rude/protocol/Types.hpp>
#include <rude/session/SessionConfig.hpp>
#include <system_error>
#include <utility>

namespace rude::detail {

   class Handshake {
   public:
      template <Socket Sock, PacketCodec Codec, typename Handler>
      static void asyncClientHandshake(Sock& sock, boost::asio::ip::udp::endpoint remote, Codec& codec, SessionConfig const&,
                                       Handler handler) {
         auto state = std::make_shared<ClientState<Handler>>(remote, std::move(handler));

         Packet initPacket;
         initPacket.type_ = PacketType::HandshakeInit;

         const auto encodedSize = codec.encode(initPacket, boost::asio::buffer(state->sendBuffer));
         if (encodedSize == 0) {
            state->handler(makeErrorCode(Error::BadPacket));
            return;
         }

         sock.asyncSendTo(boost::asio::buffer(state->sendBuffer.data(), encodedSize), state->remote,
                          [&sock, &codec, state](std::error_code ec, std::size_t) mutable {
                             if (ec) {
                                state->handler(ec);
                                return;
                             }

                             sock.asyncRecvFrom(
                                 boost::asio::buffer(state->recvBuffer), state->sender,
                                 [&codec, state](std::error_code recvError, std::size_t size) mutable {
                                    if (recvError) {
                                       state->handler(recvError);
                                       return;
                                    }

                                    const auto decoded = codec.decode(boost::asio::buffer(state->recvBuffer.data(), size));
                                    if (!decoded) {
                                       state->handler(makeErrorCode(decoded.error()));
                                       return;
                                    }

                                    if (decoded->type_ != PacketType::HandshakeAck || state->sender != state->remote) {
                                       state->handler(makeErrorCode(Error::BadPacket));
                                       return;
                                    }

                                    state->handler({});
                                 });
                          });
      }

      template <Socket Sock, PacketCodec Codec, typename Handler>
      static void asyncServerHandshake(Sock& sock, Codec& codec, SessionConfig const&, Handler handler) {
         auto state = std::make_shared<ServerState<Handler>>(std::move(handler));

         sock.asyncRecvFrom(boost::asio::buffer(state->recvBuffer), state->remote,
                            [&sock, &codec, state](std::error_code ec, std::size_t size) mutable {
                               if (ec) {
                                  state->handler(ec, {});
                                  return;
                               }

                               const auto decoded = codec.decode(boost::asio::buffer(state->recvBuffer.data(), size));
                               if (!decoded) {
                                  state->handler(makeErrorCode(decoded.error()), {});
                                  return;
                               }

                               if (decoded->type_ != PacketType::HandshakeInit) {
                                  state->handler(makeErrorCode(Error::BadPacket), {});
                                  return;
                               }

                               Packet ackPacket;
                               ackPacket.type_ = PacketType::HandshakeAck;
                               ackPacket.ack_ = decoded->seq_;

                               const auto encodedSize = codec.encode(ackPacket, boost::asio::buffer(state->sendBuffer));
                               if (encodedSize == 0) {
                                  state->handler(makeErrorCode(Error::BadPacket), {});
                                  return;
                               }

                               sock.asyncSendTo(boost::asio::buffer(state->sendBuffer.data(), encodedSize), state->remote,
                                                [state](std::error_code sendError, std::size_t) mutable {
                                                   state->handler(sendError, state->remote);
                                                });
                            });
      }

   private:
      static constexpr std::size_t bufferSize = 1500;

      template <typename Handler>
      struct ClientState {
         explicit ClientState(boost::asio::ip::udp::endpoint remoteEndpoint, Handler completionHandler)
             : remote{std::move(remoteEndpoint)}
             , handler{std::move(completionHandler)} {}

         boost::asio::ip::udp::endpoint remote;
         boost::asio::ip::udp::endpoint sender;
         std::array<std::byte, bufferSize> sendBuffer{};
         std::array<std::byte, bufferSize> recvBuffer{};
         Handler handler;
      };

      template <typename Handler>
      struct ServerState {
         explicit ServerState(Handler completionHandler)
             : handler{std::move(completionHandler)} {}

         boost::asio::ip::udp::endpoint remote;
         std::array<std::byte, bufferSize> sendBuffer{};
         std::array<std::byte, bufferSize> recvBuffer{};
         Handler handler;
      };
   };

} // namespace rude::detail

#endif // RUDE_DETAIL_HANDSHAKE_HPP
