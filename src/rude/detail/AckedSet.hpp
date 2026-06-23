
#ifndef RUDE_DETAIL_ACKEDSET_HPP
#define RUDE_DETAIL_ACKEDSET_HPP

#pragma once

#include <bitset>
#include <cstdint>

namespace rude::detail {

   /// Duplicate-suppression bitmap for ReliableUnorderedChannel.
   ///
   /// Tracks which sequence numbers in the window [base_, base_ + N) have
   /// been received. Packets older than base_ are always considered seen.
   /// When the window fills, base_ advances by N/2 (sliding).
   template <std::size_t N = 256>
   struct AckedSet {
      static_assert(N % 2 == 0, "N must be even for clean sliding");

      /// Returns true if seq has already been delivered.
      [[nodiscard]] bool seen(std::uint16_t seq) const noexcept {
         std::uint16_t const offset = static_cast<std::uint16_t>(seq - base_);
         if (offset >= N) {
            // Either very old (treat as seen) or far future (treat as not seen).
            // Far future: offset wraps to a large positive value > N.
            return offset > 32768u; // older than base_ in wrapping arithmetic
         }
         return bits_.test(offset);
      }

      /// Mark seq as received. Slides the window if we are past the halfway point.
      void mark(std::uint16_t seq) noexcept {
         std::uint16_t const offset = static_cast<std::uint16_t>(seq - base_);
         if (offset >= N)
            return;
         bits_.set(offset);

         // Slide when the lower half is fully consumed.
         if (offset >= N / 2) {
            bits_ >>= (N / 2);
            base_ += static_cast<std::uint16_t>(N / 2);
         }
      }

   private:
      std::bitset<N> bits_{};
      std::uint16_t base_ = 0;
   };

} // namespace rude::detail

#endif // RUDE_DETAIL_ACKEDSET_HPP
