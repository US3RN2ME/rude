
#ifndef RUDE_PROTOCOL_TYPES_HPP
#define RUDE_PROTOCOL_TYPES_HPP

#pragma once

#include <cstdint>

namespace rude {

   /**
    * @brief Protocol frame discriminator.
    *
    * PacketType identifies how the session or channel should interpret
    * a
    * decoded packet. Data and Ack packets are channel-scoped; handshake,
    * keepalive, and disconnect packets are
    * session-scoped.
    */
   enum class PacketType : std::uint8_t {
      HandshakeInit = 0x01, ///< Client → Server: open connection
      HandshakeAck = 0x02,  ///< Server → Client: confirm + echo nonce
      Data = 0x10,          ///< Payload data (reliable or unreliable)
      Ack = 0x11,           ///< Cumulative ACK with bitmask
      Keepalive = 0x20,     ///< Heartbeat — no payload
      Disconnect = 0x30,    ///< Graceful teardown notification
   };

} // namespace rude

#endif // RUDE_PROTOCOL_TYPES_HPP
