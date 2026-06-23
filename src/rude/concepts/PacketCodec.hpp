
#ifndef RUDE_CONCEPTS_PACKETCODEC_HPP
#define RUDE_CONCEPTS_PACKETCODEC_HPP

#include <boost/asio/buffer.hpp>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <rude/core/Error.hpp>
#include <rude/protocol/Packet.hpp>

namespace rude {

   template <typename T>
   concept PacketCodec =
       requires(T codec, Packet const& pkt, boost::asio::mutable_buffer outBuf, boost::asio::const_buffer inBuf) {
          { codec.encode(pkt, outBuf) } -> std::same_as<std::size_t>;
          { codec.decode(inBuf) } -> std::same_as<std::expected<Packet, Error>>;
          { codec.maxOverhead() } -> std::same_as<std::size_t>;
          { codec.version() } -> std::same_as<std::uint8_t>;
       };

} // namespace rude

#endif // RUDE_CONCEPTS_PACKETCODEC_HPP
