
#ifndef RUDE_CONCEPTS_SOCKET_HPP
#define RUDE_CONCEPTS_SOCKET_HPP

#pragma once

#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/udp.hpp>
#include <concepts>

namespace rude {

   template <typename T>
   concept Socket = requires(T sock, boost::asio::const_buffer sendBuf, boost::asio::mutable_buffer recvBuf,
                             boost::asio::ip::udp::endpoint ep) {
      sock.asyncSendTo(sendBuf, ep, [](auto, auto) {});
      sock.asyncRecvFrom(recvBuf, ep, [](auto, auto) {});
      { sock.localEndpoint() } -> std::same_as<boost::asio::ip::udp::endpoint>;
      { sock.cancel() } -> std::same_as<void>;
      { sock.close() } -> std::same_as<void>;
   };

} // namespace rude

#endif // RUDE_CONCEPTS_SOCKET_HPP
