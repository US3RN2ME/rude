
#ifndef RUDE_SESSION_SESSIONCONFIG_HPP
#define RUDE_SESSION_SESSIONCONFIG_HPP

#include <cstddef>
#include <cstdint>

namespace rude {

   /**
    * @brief Session-level transport configuration.
    *
    * The values control global session behavior such as MTU,
    * keepalive cadence,
    * handshake timeout, and the maximum number of channel ids the session will
    * accept. All
    * fields have conservative defaults suitable for component tests
    * and local development.
    */
   struct SessionConfig {
      /// Maximum transmission unit for the underlying network path (bytes).
      /// Payloads larger than mtu_ - codec overhead are fragmented by the caller.
      std::size_t mtu_ = 1400;

      /// Interval between keepalive packets when no data is in-flight (ms).
      std::uint32_t keepaliveMs_ = 5000;

      /// Duration of silence after which the session is declared dead (ms).
      /// Must be > keepaliveMs_.
      std::uint32_t timeoutMs_ = 30'000;

      /// Maximum number of simultaneous channels per session [1, 256].
      std::uint8_t maxChannels_ = 8;

      /// Smoothing factor for the EWMA RTT estimator (RFC 6298 alpha). [0.0, 1.0]
      double rttAlpha_ = 0.125;

      /// How long to wait for the three-way handshake to complete (ms).
      std::uint32_t maxHandshakeMs_ = 5'000;

      /// Receive buffer size allocated inside UdpSocket (bytes).
      std::size_t recvBufSize_ = 65'536;
   };

} // namespace rude

#endif // RUDE_SESSION_SESSIONCONFIG_HPP
