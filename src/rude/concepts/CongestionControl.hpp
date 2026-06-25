
#ifndef RUDE_CONCEPTS_CONGESTIONCONTROL_HPP
#define RUDE_CONCEPTS_CONGESTIONCONTROL_HPP

#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>

namespace rude {
   /**
    * @brief Concept for reliable-channel congestion controllers.
    *
    * Controllers observe acknowledgements and
    * losses, expose a current send
    * window, and optionally provide pacing and RTT estimates.
    *
    * @tparam T
    *
    * Candidate congestion controller type.
    */
   template <typename T>
   concept CongestionCtrl = requires(T cc, std::uint16_t seq, std::uint64_t rttUs) {
      { cc.onAck(seq, rttUs) } noexcept -> std::same_as<void>;
      { cc.onLoss(seq) } noexcept -> std::same_as<void>;
      { cc.sendWindow() } noexcept -> std::same_as<std::size_t>;
      { cc.pacingDelay() } noexcept -> std::same_as<std::chrono::nanoseconds>;
      { cc.rttEstimate() } noexcept -> std::same_as<std::chrono::microseconds>;
      { cc.reset() } noexcept -> std::same_as<void>;
   };

} // namespace rude

#endif // RUDE_CONCEPTS_CONGESTIONCONTROL_HPP
