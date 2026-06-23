
#ifndef RUDE_DETAIL_CONGESTION_BBRLITE_HPP
#define RUDE_DETAIL_CONGESTION_BBRLITE_HPP

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace rude {

   /// Simplified BBR-inspired congestion controller.
   ///
   /// Maintains estimates of bottleneck bandwidth (btlBw_) and minimum RTT (rtProp_)
   /// to derive a send window that keeps the pipe full without building a queue.
   ///
   /// Phase cycle (simplified):
   ///   startup   — exponential window growth until BDP is found
   ///   drain     — one RTT at reduced rate to drain any queue built in startup
   ///   probeBw   — steady state: probe for extra bandwidth once per 8 RTTs
   ///   probeRtt  — briefly shrink window to refresh rtProp_ estimate
   ///
   /// Prefer over LeakyBucketCC on WAN links, mobile networks, or when
   /// cross-traffic from other applications is expected.
   class BbrLite {
      using Clock = std::chrono::steady_clock;
      using Micros = std::chrono::microseconds;
      using Nanos = std::chrono::nanoseconds;

   public:
      void onAck(std::uint16_t, std::uint64_t rttUs) noexcept {
         auto const rtt = Micros{rttUs};

         // Update minimum RTT (rtProp_).
         if (rtt < rtProp_ || rtPropExpired()) {
            rtProp_ = rtt;
            rtPropStamp_ = Clock::now();
         }

         // Update bandwidth estimate: delivered / RTT.
         if (rttUs > 0) {
            double const bwSample = 1.0e6 / static_cast<double>(rttUs); // packets/s
            btlBw_ = std::max(btlBw_, bwSample);
         }

         ++ackCount_;
         advancePhase();
      }

      void onLoss(std::uint16_t) noexcept {
         // BBR does not reduce btlBw_ on loss — loss is not the congestion signal.
         // We do briefly halve the inflight cap to drain the queue.
         inflightCap_ = std::max<std::size_t>(4, inflightCap_ / 2);
      }

      [[nodiscard]] std::size_t sendWindow() noexcept {
         // BDP = btlBw_ (pkt/s) × rtProp_ (s)
         double const rtPropSecs = static_cast<double>(rtProp_.count()) / 1.0e6;
         double const bdp = btlBw_ * rtPropSecs;
         std::size_t const target = static_cast<std::size_t>(bdp * paceGain_) + 4;
         return std::min(target, inflightCap_);
      }

      [[nodiscard]] Nanos pacingDelay() noexcept {
         if (btlBw_ <= 0.0)
            return Nanos{1'000'000}; // 1 ms until estimate arrives
         double const secsBetweenPkts = paceGain_ / btlBw_;
         return std::chrono::duration_cast<Nanos>(std::chrono::duration<double>{secsBetweenPkts});
      }

      [[nodiscard]] Micros rttEstimate() noexcept {
         return rtProp_;
      }

      void reset() noexcept {
         phase_ = Phase::startup;
         btlBw_ = 0.0;
         rtProp_ = Micros::max();
         rtPropStamp_ = Clock::now();
         inflightCap_ = std::numeric_limits<std::size_t>::max();
         ackCount_ = 0;
         paceGain_ = kStartupGain;
      }

   private:
      enum class Phase { startup, drain, probeBw, probeRtt };

      static constexpr double kStartupGain = 2.0;
      static constexpr double kDrainGain = 0.75;
      static constexpr double kProbeBwGain = 1.25;
      static constexpr double kSteadyGain = 1.0;
      static constexpr std::size_t kProbeRttInterval = 200; // ACKs

      bool rtPropExpired() const noexcept {
         // Refresh rtProp_ every 10 seconds.
         return Clock::now() - rtPropStamp_ > std::chrono::seconds{10};
      }

      void advancePhase() noexcept {
         switch (phase_) {
            case Phase::startup:
               if (btlBw_ > 0.0) {
                  phase_ = Phase::drain;
                  paceGain_ = kDrainGain;
               }
               break;
            case Phase::drain:
               phase_ = Phase::probeBw;
               paceGain_ = kProbeBwGain;
               inflightCap_ = std::numeric_limits<std::size_t>::max();
               break;
            case Phase::probeBw:
               if (ackCount_ % kProbeRttInterval == 0) {
                  phase_ = Phase::probeRtt;
                  inflightCap_ = 4;
                  paceGain_ = kSteadyGain;
               }
               break;
            case Phase::probeRtt:
               phase_ = Phase::probeBw;
               paceGain_ = kProbeBwGain;
               inflightCap_ = std::numeric_limits<std::size_t>::max();
               break;
         }
      }

      Phase phase_ = Phase::startup;
      double btlBw_ = 0.0;            ///< Bottleneck bandwidth estimate (pkt/s)
      Micros rtProp_ = Micros::max(); ///< Minimum observed RTT
      Clock::time_point rtPropStamp_ = Clock::now();
      std::size_t inflightCap_ = std::numeric_limits<std::size_t>::max();
      std::size_t ackCount_ = 0;
      double paceGain_ = kStartupGain;
   };

} // namespace rude

#endif // RUDE_DETAIL_CONGESTION_BBRLITE_HPP
