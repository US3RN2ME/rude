
#ifndef RUDE_DETAIL_REORDERBUFFER_HPP
#define RUDE_DETAIL_REORDERBUFFER_HPP

#include <array>
#include <cstdint>
#include <optional>
#include <rude/protocol/Packet.hpp>
#include <span>
#include <vector>

namespace rude::detail {

   /// Receive-side reorder buffer for ReliableOrderedChannel.
   ///
   /// Holds up to N out-of-order packets and delivers them in sequence.
   /// Packets are inserted at their seq slot; drain() yields a contiguous
   /// run of in-order packets starting from nextSeq_.
   template <std::size_t N = 128>
   struct ReorderBuffer {
      /// Insert a packet into its slot. Silently drops duplicates and
      /// packets outside the receive window [nextSeq_, nextSeq_ + N).
      void insert(Packet pkt) noexcept {
         std::uint16_t const offset = static_cast<std::uint16_t>(pkt.seq_ - nextSeq_);
         if (offset >= N)
            return; // outside window or duplicate
         auto& slot = slots_[pkt.seq_ % N];
         if (slot.has_value())
            return; // duplicate
         slot = std::move(pkt);
      }

      /// Drain and return all consecutively available packets starting at nextSeq_.
      /// The returned span is valid until the next call to insert() or drain().
      [[nodiscard]] std::span<Packet const> drain() noexcept {
         ready_.clear();
         while (slots_[nextSeq_ % N].has_value()) {
            ready_.push_back(std::move(*slots_[nextSeq_ % N]));
            slots_[nextSeq_ % N].reset();
            ++nextSeq_;
         }
         return ready_;
      }

      [[nodiscard]] bool empty() const noexcept {
         return !slots_[nextSeq_ % N].has_value();
      }

      [[nodiscard]] std::uint16_t nextSeq() const noexcept {
         return nextSeq_;
      }

   private:
      std::array<std::optional<Packet>, N> slots_{};
      std::uint16_t nextSeq_ = 0;
      std::vector<Packet> ready_; ///< Scratch buffer for drain()
   };

} // namespace rude::detail

#endif // RUDE_DETAIL_REORDERBUFFER_HPP
