
#ifndef RUDE_SESSION_CHANNELCONFIG_HPP
#define RUDE_SESSION_CHANNELCONFIG_HPP

#include <cstdint>

namespace rude {

   /**
    * @brief Delivery profile selected for a session channel.
    *
    * The mode selects the concrete channel implementation used by
    * BasicSession::setChannel(). Concrete channel classes do not accept this
    * enum because their C++ type already defines the delivery contract.
    */
   enum class ChannelMode : std::uint8_t {
      ReliableOrdered,
      ReliableUnordered,
      UnreliableSequenced,
   };

   /**
    * @brief Options shared by concrete channel implementations.
    *
    * These fields are meaningful for both reliable and unreliable channels.
    * Use the narrower derived config type accepted by the concrete channel
    * constructor to avoid passing irrelevant reliability settings.
    */
   struct ChannelOptions {
      /// Channel index [0, SessionConfig::maxChannels_). Must be unique per session.
      std::uint8_t id_ = 0;

      /// Soft send priority relative to other channels [0 = highest, 255 = lowest].
      /// Used by SendGuard to order flush calls when the socket is congested.
      std::uint8_t priority_ = 0;
   };

   /**
    * @brief Options accepted by reliable concrete channels.
    *
    * Reliable channels retain sent packets until they are acknowledged and use
    * the send window to bound in-flight data. The same type configures both
    * reliable ordered and reliable unordered concrete channels.
    */
   struct ReliableChannelConfig : ChannelOptions {
      /// Sliding window size: max unacknowledged packets in-flight [1, 32768].
      std::uint16_t sendWindow_ = 128;

      /// Initial retransmit timeout (ms). Doubles on each loss up to 8× (TCP-like).
      std::uint32_t retxTimeoutMs_ = 200;
   };

   /**
    * @brief Options accepted by unreliable concrete channels.
    *
    * Unreliable channels do not retransmit, acknowledge, or maintain a reliable
    * send window. Only shared fields such as channel id and priority are
    * accepted.
    */
   struct UnreliableChannelConfig : ChannelOptions {};

   /**
    * @brief Session-facing channel profile.
    *
    * The mode chooses the concrete channel implementation created by a session.
    * Named factory functions express the intended delivery behavior directly,
    * avoiding invalid combinations such as unreliable ordered channels.
    */
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

      /**
       * @brief Creates a reliable ordered session channel profile.
       *
       * Packets are retransmitted until acknowledged and delivered to receivers
       * in sequence order.
       *
       * @param id
       * Session channel id.
       *
       * @return Channel profile configured for reliable ordered delivery.
       */
      [[nodiscard]] static constexpr ChannelConfig reliableOrdered(std::uint8_t id) noexcept {
         ChannelConfig cfg;
         cfg.id_ = id;
         cfg.mode_ = ChannelMode::ReliableOrdered;
         return cfg;
      }

      /**
       * @brief Creates a reliable unordered session channel profile.
       *
       * Packets are retransmitted until acknowledged. Receivers suppress
       * duplicates but do not wait for missing lower sequence numbers.
       *
       * @param id
       * Session channel id.
       *
       * @return Channel profile configured for reliable unordered delivery.
       */
      [[nodiscard]] static constexpr ChannelConfig reliableUnordered(std::uint8_t id) noexcept {
         ChannelConfig cfg;
         cfg.id_ = id;
         cfg.mode_ = ChannelMode::ReliableUnordered;
         return cfg;
      }

      /**
       * @brief Creates an unreliable sequenced session channel profile.
       *
       * Packets are sent once. Receivers deliver newer sequence numbers and
       * drop stale or duplicate packets.
       *
       * @param id
       * Session channel id.
       *
       * @return Channel profile configured for unreliable sequenced delivery.
       */
      [[nodiscard]] static constexpr ChannelConfig unreliableSequenced(std::uint8_t id) noexcept {
         ChannelConfig cfg;
         cfg.id_ = id;
         cfg.mode_ = ChannelMode::UnreliableSequenced;
         return cfg;
      }

      /**
       * @brief Extracts concrete reliable-channel constructor options.
       *
       * This is used internally by BasicSession and may also be useful in tests
       * that construct concrete reliable channels directly.
       *
       * @return Reliable channel options copied from this profile.
       */
      [[nodiscard]] constexpr ReliableChannelConfig reliableOptions() const noexcept {
         ReliableChannelConfig options;
         options.id_ = id_;
         options.priority_ = priority_;
         options.sendWindow_ = sendWindow_;
         options.retxTimeoutMs_ = retxTimeoutMs_;
         return options;
      }

      /**
       * @brief Extracts concrete unreliable-channel constructor options.
       *
       * Reliable-only fields such as send window and retransmit timeout are not
       * copied because unreliable channels do not use them.
       *
       * @return Unreliable channel options copied from this profile.
       */
      [[nodiscard]] constexpr UnreliableChannelConfig unreliableOptions() const noexcept {
         UnreliableChannelConfig options;
         options.id_ = id_;
         options.priority_ = priority_;
         return options;
      }
   };

} // namespace rude

#endif // RUDE_SESSION_CHANNELCONFIG_HPP
