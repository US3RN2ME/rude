
#ifndef RUDE_DETAIL_CONGESTION_NULL_HPP
#define RUDE_DETAIL_CONGESTION_NULL_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace rude::detail {

   /**
    * @brief No-op congestion controller.
    *
    * All packets are always permitted. There is no pacing, RTT tracking,
    * or
    * loss response. This is useful for tests and for paths that already have
    * external rate limiting.
    */
   struct Null {
      constexpr void onAck(std::uint16_t, std::uint64_t) noexcept {}
      constexpr void onLoss(std::uint16_t) noexcept {}

      [[nodiscard]] constexpr std::size_t sendWindow() noexcept {
         return std::numeric_limits<std::size_t>::max();
      }
      [[nodiscard]] constexpr std::chrono::nanoseconds pacingDelay() noexcept {
         return std::chrono::nanoseconds{0};
      }
      [[nodiscard]] constexpr std::chrono::microseconds rttEstimate() noexcept {
         return std::chrono::microseconds{0};
      }
      constexpr void reset() noexcept {}
   };

} // namespace rude::detail

#endif // RUDE_DETAIL_CONGESTION_NULL_HPP
