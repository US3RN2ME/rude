
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

   /**
    * @brief Concept for packet codec implementations.
    *
    * A codec converts Packet values to byte buffers and
    * decodes byte buffers
    * back into Packet views. Decode failures are reported as rude::Error values
    * through
    * std::expected.
    *
    * @tparam T
    * Candidate codec type.
    */
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
