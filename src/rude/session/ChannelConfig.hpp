
#ifndef RUDE_SESSION_CHANNELCONFIG_HPP
#define RUDE_SESSION_CHANNELCONFIG_HPP

#include <cstdint>

namespace rude {

   enum class ChannelMode : std::uint8_t {
      ReliableOrdered,
      ReliableUnordered,
      UnreliableSequenced,
   };

   /// Options shared by concrete channel implementations.
   struct ChannelOptions {
      /// Channel index [0, SessionConfig::maxChannels_). Must be unique per session.
      std::uint8_t id_ = 0;

      /// Soft send priority relative to other channels [0 = highest, 255 = lowest].
      /// Used by SendGuard to order flush calls when the socket is congested.
      std::uint8_t priority_ = 0;
   };

   /// Options accepted by reliable concrete channels.
   struct ReliableChannelConfig : ChannelOptions {
      /// Sliding window size: max unacknowledged packets in-flight [1, 32768].
      std::uint16_t sendWindow_ = 128;

      /// Initial retransmit timeout (ms). Doubles on each loss up to 8× (TCP-like).
      std::uint32_t retxTimeoutMs_ = 200;
   };

   /// Options accepted by unreliable concrete channels.
   struct UnreliableChannelConfig : ChannelOptions {};

   /// Session-facing channel profile. The mode chooses the concrete channel
   /// implementation; tuning fields are then applied only when relevant.
   struct ChannelConfig {
      /// Channel index [0, SessionConfig::maxChannels_). Must be unique per session.
      std::uint8_t id_ = 0;

      /// Delivery behavior for this session channel.
      ChannelMode mode_ = ChannelMode::ReliableOrdered;

      /// Sliding window size for reliable modes [1, 32768]. Ignored by unreliable channels.
      std::uint16_t sendWindow_ = 128;

      /// Initial retransmit timeout for reliable modes. Ignored by unreliable channels.
      std::uint32_t retxTimeoutMs_ = 200;

      /// Soft send priority relative to other channels [0 = highest, 255 = lowest].
      std::uint8_t priority_ = 0;

      [[nodiscard]] static constexpr ChannelConfig reliableOrdered(std::uint8_t id) noexcept {
         ChannelConfig cfg;
         cfg.id_ = id;
         cfg.mode_ = ChannelMode::ReliableOrdered;
         return cfg;
      }

      [[nodiscard]] static constexpr ChannelConfig reliableUnordered(std::uint8_t id) noexcept {
         ChannelConfig cfg;
         cfg.id_ = id;
         cfg.mode_ = ChannelMode::ReliableUnordered;
         return cfg;
      }

      [[nodiscard]] static constexpr ChannelConfig unreliableSequenced(std::uint8_t id) noexcept {
         ChannelConfig cfg;
         cfg.id_ = id;
         cfg.mode_ = ChannelMode::UnreliableSequenced;
         return cfg;
      }

      [[nodiscard]] constexpr ReliableChannelConfig reliableOptions() const noexcept {
         ReliableChannelConfig options;
         options.id_ = id_;
         options.priority_ = priority_;
         options.sendWindow_ = sendWindow_;
         options.retxTimeoutMs_ = retxTimeoutMs_;
         return options;
      }

      [[nodiscard]] constexpr UnreliableChannelConfig unreliableOptions() const noexcept {
         UnreliableChannelConfig options;
         options.id_ = id_;
         options.priority_ = priority_;
         return options;
      }
   };

} // namespace rude

#endif // RUDE_SESSION_CHANNELCONFIG_HPP
