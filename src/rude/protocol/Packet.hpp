
#ifndef RUDE_PROTOCOL_PACKET_HPP
#define RUDE_PROTOCOL_PACKET_HPP

#include <cstdint>
#include <rude/protocol/Types.hpp>
#include <span>

namespace rude {

   /**
    * @brief Decoded protocol packet view.
    *
    * Packet payload is non-owning and points into the caller-provided
    * decode
    * buffer. Keep that buffer alive for as long as payload_ is inspected or
    * passed to channel receive
    * handlers.
    */
   struct Packet {
      PacketType type_ = PacketType::Data;
      std::uint16_t seq_ = 0;
      std::uint16_t ack_ = 0;     ///< Latest seq received from remote
      std::uint32_t ackBits_ = 0; ///< Bitmask: bit N set → seq - N - 1 received
      std::uint8_t channel_ = 0;
      std::span<std::byte const> payload_; ///< Non-owning; valid for lifetime of recv buffer
      std::uint32_t crc_ = 0;              ///< Populated by codec; checked on decode
   };

} // namespace rude

#endif // RUDE_PROTOCOL_PACKET_HPP
