
#ifndef RUDE_SESSION_CHANELCONFIG_HPP
#define RUDE_SESSION_CHANELCONFIG_HPP

#include <cstdint>
#include <rude/protocol/Types.hpp>

namespace rude {

   /// Per-channel configuration. One ChannelConfig is provided for each channel
   /// when constructing a Session via Acceptor or Connector.
   struct ChannelConfig {
      /// Channel index [0, SessionConfig::maxChannels_). Must be unique per session.
      std::uint8_t id_ = 0;

      /// Whether the channel retransmits lost packets.
      ReliabilityMode reliability_ = ReliabilityMode::Reliable;

      /// Whether the channel enforces send order at the receiver.
      OrderingMode ordering_ = OrderingMode::Ordered;

      /// Sliding window size: max unacknowledged packets in-flight [1, 32768].
      std::uint16_t sendWindow_ = 128;

      /// Initial retransmit timeout (ms). Doubles on each loss up to 8× (TCP-like).
      std::uint32_t retxTimeoutMs_ = 200;

      /// Soft send priority relative to other channels [0 = highest, 255 = lowest].
      /// Used by SendGuard to order flush calls when the socket is congested.
      std::uint8_t priority_ = 0;
   };

} // namespace rude

#endif // RUDE_SESSION_CHANELCONFIG_HPP
