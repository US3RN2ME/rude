
#ifndef RUDE_PROTOCOL_TYPES_HPP
#define RUDE_PROTOCOL_TYPES_HPP

#pragma once

#include <cstdint>

namespace rude {

   enum class PacketType : std::uint8_t {
      HandshakeInit = 0x01, ///< Client → Server: open connection
      HandshakeAck = 0x02,  ///< Server → Client: confirm + echo nonce
      Data = 0x10,          ///< Payload data (reliable or unreliable)
      Ack = 0x11,           ///< Cumulative ACK with bitmask
      Keepalive = 0x20,     ///< Heartbeat — no payload
      Disconnect = 0x30,    ///< Graceful teardown notification
   };

   enum class ReliabilityMode : std::uint8_t {
      Unreliable = 0, ///< Fire-and-forget; no retransmission
      Reliable = 1,   ///< Retransmit until ACKed or session closes
   };

   enum class OrderingMode : std::uint8_t {
      None = 0,      ///< Deliver immediately in any order (reliable-unordered)
      Ordered = 1,   ///< Deliver in send order; hold back out-of-order packets (reliable-ordered)
      Sequenced = 2, ///< Deliver newest only; drop packets older than last delivered (unreliable)
   };

} // namespace rude

#endif // RUDE_TYPES_HPP
