
#ifndef RUDE_DETAIL_SLIDINGWINDOW_HPP
#define RUDE_DETAIL_SLIDINGWINDOW_HPP

#include <cstddef>
#include <cstdint>

namespace rude::detail {

   /// Tracks the send-side sliding window for a reliable channel.
   ///
   /// The window is represented as a base sequence number plus a 32-bit bitmask
   /// that records which of the 32 packets below base_ have been acknowledged.
   /// This matches the ackBits_ field in the Packet header exactly, so building
   /// an ACK packet is a single copy of ackBits_.
   struct SlidingWindow {
      explicit SlidingWindow(std::uint16_t capacity) noexcept
          : capacity_{capacity} {}

      /// True if no more packets can be sent (all window slots occupied).
      [[nodiscard]] bool isFull() const noexcept {
         return inFlight_ >= capacity_;
      }

      /// True if seq falls within the window [base_, base_ + capacity_).
      [[nodiscard]] bool contains(std::uint16_t seq) const noexcept {
         return static_cast<std::uint16_t>(seq - base_) < capacity_;
      }

      /// Record that we are sending a new packet. Call before enqueuing to retxQ_.
      void onSend() noexcept {
         ++inFlight_;
      }

      /// Process an incoming ACK. Updates base_ and ackBits_, decrements inFlight_.
      /// Returns true if the window advanced (new packets can now be sent).
      bool onAck(std::uint16_t ack, std::uint32_t ackBits) noexcept {
         std::uint16_t const delta = static_cast<std::uint16_t>(ack - base_);
         if (delta == 0 || delta >= capacity_)
            return false;

         // Slide the window forward by delta positions.
         std::uint16_t advanced = 0;
         for (std::uint16_t i = 0; i < delta; ++i) {
            if (inFlight_ > 0) {
               --inFlight_;
               ++advanced;
            }
         }
         // Acknowledge additional packets recorded in the bitmask.
         std::uint32_t bits = ackBits;
         while (bits) {
            bits &= bits - 1;
            if (inFlight_ > 0)
               --inFlight_;
         }
         base_ = ack;
         ackBits_ = ackBits;
         return advanced > 0;
      }

      /// Returns the current ACK bitmask to embed in an outgoing packet.
      [[nodiscard]] std::uint32_t buildAckBits() const noexcept {
         return ackBits_;
      }

      [[nodiscard]] std::uint16_t base() const noexcept {
         return base_;
      }
      [[nodiscard]] std::size_t inFlight() const noexcept {
         return inFlight_;
      }

   private:
      std::uint16_t base_ = 0;
      std::uint32_t ackBits_ = 0;
      std::uint16_t capacity_;
      std::size_t inFlight_ = 0;
   };

} // namespace rude::detail

#endif // RUDE_DETAIL_SLIDINGWINDOW_HPP
