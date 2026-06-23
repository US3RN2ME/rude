#ifndef RUDE_TEST_HELPERS_TEST_HELPERS_HPP
#define RUDE_TEST_HELPERS_TEST_HELPERS_HPP

#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/udp.hpp>
#include <cstddef>
#include <cstring>
#include <rude/core/Error.hpp>
#include <rude/protocol/Packet.hpp>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace rude::test {

   struct MockSocket {
      std::vector<std::byte> lastSend;
      std::size_t sends = 0;
      std::error_code sendError;
      boost::asio::ip::udp::endpoint lastEndpoint;

      template <typename Handler>
      void asyncSendTo(boost::asio::const_buffer buffer, boost::asio::ip::udp::endpoint endpoint, Handler handler) {
         lastEndpoint = endpoint;
         auto const* first = static_cast<std::byte const*>(buffer.data());
         lastSend.assign(first, first + buffer.size());
         ++sends;
         handler(sendError, buffer.size());
      }

      template <typename Handler>
      void asyncRecvFrom(boost::asio::mutable_buffer, boost::asio::ip::udp::endpoint&, Handler handler) {
         handler(makeErrorCode(Error::Timeout), std::size_t{0});
      }

      boost::asio::ip::udp::endpoint localEndpoint() {
         return {};
      }
      void cancel() {}
      void close() {}
   };

   [[nodiscard]] inline std::vector<std::byte> bytes(std::string const& text) {
      std::vector<std::byte> out(text.size());
      std::memcpy(out.data(), text.data(), text.size());
      return out;
   }

   [[nodiscard]] inline std::string text(std::span<std::byte const> payload) {
      return {reinterpret_cast<char const*>(payload.data()), payload.size()};
   }

   [[nodiscard]] inline Packet dataPacket(std::uint16_t seq, std::span<std::byte const> payload) {
      Packet packet;
      packet.type_ = PacketType::Data;
      packet.seq_ = seq;
      packet.payload_ = payload;
      return packet;
   }

} // namespace rude::test

#endif // RUDE_TEST_HELPERS_TEST_HELPERS_HPP
