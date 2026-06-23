#ifndef RUDE_DETAIL_RECVLOOP_HPP
#define RUDE_DETAIL_RECVLOOP_HPP

#include <array>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/udp.hpp>
#include <cstddef>
#include <memory>
#include <optional>
#include <rude/concepts/PacketCodec.hpp>
#include <rude/concepts/Socket.hpp>
#include <rude/core/Error.hpp>
#include <rude/protocol/Packet.hpp>
#include <rude/protocol/Types.hpp>
#include <rude/session/SocketStats.hpp>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace rude::detail {

   class RecvLoop {
   public:
      template <Socket Sock, PacketCodec Codec, typename Channels, typename Handler>
      static void start(Sock& sock, Codec& codec, Channels& channels, SocketStats& stats, boost::asio::ip::udp::endpoint remote,
                        Handler handler) {
         auto state = std::make_shared<State<Handler>>(std::move(remote), std::move(handler));
         receiveNext(sock, codec, channels, stats, state);
      }

   private:
      static constexpr std::size_t bufferSize = 65'536;

      template <typename Handler>
      struct State {
         explicit State(boost::asio::ip::udp::endpoint remoteEndpoint, Handler completionHandler)
             : remote{std::move(remoteEndpoint)}
             , handler{std::move(completionHandler)} {}

         boost::asio::ip::udp::endpoint remote;
         boost::asio::ip::udp::endpoint sender;
         std::array<std::byte, bufferSize> recvBuffer{};
         Handler handler;
      };

      template <Socket Sock, PacketCodec Codec, typename Channels, typename Handler>
      static void receiveNext(Sock& sock, Codec& codec, Channels& channels, SocketStats& stats,
                              std::shared_ptr<State<Handler>> state) {
         sock.asyncRecvFrom(boost::asio::buffer(state->recvBuffer), state->sender,
                            [&sock, &codec, &channels, &stats, state](std::error_code ec, std::size_t size) mutable {
                               if (ec) {
                                  state->handler(ec);
                                  return;
                               }

                               if (state->sender != state->remote) {
                                  receiveNext(sock, codec, channels, stats, state);
                                  return;
                               }

                               const auto decoded = codec.decode(boost::asio::buffer(state->recvBuffer.data(), size));
                               if (!decoded) {
                                  state->handler(makeErrorCode(decoded.error()));
                                  return;
                               }

                               if (decoded->type_ == PacketType::Disconnect) {
                                  state->handler(makeErrorCode(Error::ConnectionReset));
                                  return;
                               }

                               dispatchPacket(sock, codec, channels, stats, state->remote, *decoded);
                               receiveNext(sock, codec, channels, stats, state);
                            });
      }

      template <Socket Sock, PacketCodec Codec, typename Channels>
      static void dispatchPacket(Sock& sock, Codec& codec, Channels& channels, SocketStats& stats,
                                 boost::asio::ip::udp::endpoint const& remote, Packet const& packet) {
         if (packet.channel_ >= channels.size()) {
            return;
         }

         std::visit(
             [&](auto& channel) {
                channel.onRecv(packet, stats);
                if (packet.type_ != PacketType::Data) {
                   return;
                }

                auto ack = channel.buildAck();
                if (!ack) {
                   return;
                }

                auto sendBuffer = std::make_shared<std::vector<std::byte>>(codec.maxOverhead());
                const auto encodedSize = codec.encode(*ack, boost::asio::buffer(*sendBuffer));
                if (encodedSize == 0) {
                   return;
                }

                sendBuffer->resize(encodedSize);
                sock.asyncSendTo(boost::asio::buffer(*sendBuffer), remote, [sendBuffer](std::error_code, std::size_t) {});
             },
             channels[packet.channel_]);
      }
   };

} // namespace rude::detail

#endif // RUDE_DETAIL_RECVLOOP_HPP
