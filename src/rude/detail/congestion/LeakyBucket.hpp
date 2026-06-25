
#ifndef RUDE_DETAIL_CONGESTION_LEAKYBUCKET_HPP
#define RUDE_DETAIL_CONGESTION_LEAKYBUCKET_HPP

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace rude {

   /**
    * @brief Token-bucket congestion controller.
    *
    * The controller maintains a bucket of tokens that refills at
    * a configured
    * packet rate. Each packet costs one token. If the bucket is empty,
    * pacingDelay() returns the delay
    * until another token is available.
    *
    * ACKs gradually recover toward the configured peak rate. Losses apply a
    *
    * multiplicative decrease to reduce pressure on the path.
    */
   class LeakyBucket {
      using Clock = std::chrono::steady_clock;

   public:
      /// rate: target packets per second (default: 1000 pps — 1 Mbps at 1 KB packets)
      /// burst: maximum tokens the bucket can hold (allow short bursts)
      explicit LeakyBucket(double rate = 1000.0, double burst = 64.0) noexcept
          : rate_{rate}
          , peakRate_{rate}
          , burst_{burst}
          , tokens_{burst}
          , lastTick_{Clock::now()} {}

      void onAck(std::uint16_t, std::uint64_t) noexcept {
         refill();
         // Slowly recover towards peak on clean ACKs.
         rate_ = std::min(peakRate_, rate_ * 1.01);
      }

      void onLoss(std::uint16_t) noexcept {
         rate_ = std::max(rate_ * 0.5, 10.0); // floor at 10 pps
      }

      [[nodiscard]] std::size_t sendWindow() noexcept {
         refill();
         return static_cast<std::size_t>(std::max(0.0, tokens_));
      }

      [[nodiscard]] std::chrono::nanoseconds pacingDelay() noexcept {
         if (tokens_ >= 1.0)
            return std::chrono::nanoseconds{0};
         double const secsUntilToken = (1.0 - tokens_) / rate_;
         return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double>{secsUntilToken});
      }

      [[nodiscard]] std::chrono::microseconds rttEstimate() noexcept {
         return smoothedRtt_;
      }

      void reset() noexcept {
         tokens_ = burst_;
         rate_ = peakRate_;
         lastTick_ = Clock::now();
         smoothedRtt_ = std::chrono::microseconds{0};
      }

   private:
      void refill() noexcept {
         auto const now = Clock::now();
         double const elapsed = std::chrono::duration<double>{now - lastTick_}.count();
         tokens_ = std::min(burst_, tokens_ + rate_ * elapsed);
         lastTick_ = now;
      }

      double rate_;
      double const peakRate_;
      double const burst_;
      double tokens_;
      Clock::time_point lastTick_;
      std::chrono::microseconds smoothedRtt_{0};
   };

} // namespace rude

#endif // RUDE_DETAIL_CONGESTION_LEAKYBUCKET_HPP
