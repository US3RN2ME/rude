
#ifndef RUDE_DETAIL_RETRANSMITQUEUE_HPP
#define RUDE_DETAIL_RETRANSMITQUEUE_HPP

#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <functional>
#include <rude/core/Executor.hpp>
#include <rude/protocol/Packet.hpp>
#include <unordered_map>
#include <vector>

namespace rude::detail {

   /// Tracks in-flight reliable packets and fires retransmit callbacks when
   /// their deadlines expire. One instance per reliable channel.
   ///
   /// All methods must be called on the owning session's strand.
   class RetransmitQueue {
   public:
      using Clock = std::chrono::steady_clock;
      using TimePoint = Clock::time_point;
      using RetxCallback = std::function<void(Packet const&)>;

      explicit RetransmitQueue(Strand& strand) noexcept
          : timer_{strand.get_inner_executor()}
          , strand_{strand} {}

      /// Enqueue a packet for potential retransmission at deadline.
      /// The callback is invoked on the strand when the deadline fires.
      void push(Packet pkt, TimePoint deadline, RetxCallback onRetx) {
         entries_.emplace(pkt.seq_, Entry{std::move(pkt), deadline, std::move(onRetx)});
         scheduleNext();
      }

      /// Remove a packet from the queue when its ACK arrives.
      void acknowledge(std::uint16_t seq) noexcept {
         entries_.erase(seq);
      }

      /// Remove all entries (e.g. on disconnect).
      void clear() noexcept {
         timer_.cancel();
         entries_.clear();
      }

      [[nodiscard]] bool empty() const noexcept {
         return entries_.empty();
      }
      [[nodiscard]] std::size_t size() const noexcept {
         return entries_.size();
      }

   private:
      struct Entry {
         Packet pkt;
         TimePoint deadline;
         RetxCallback onRetx;
      };

      void scheduleNext() {
         if (entries_.empty())
            return;

         // Find the entry with the earliest deadline.
         TimePoint earliest = TimePoint::max();
         for (auto const& [seq, e] : entries_) {
            if (e.deadline < earliest)
               earliest = e.deadline;
         }

         timer_.expires_at(earliest);
         timer_.async_wait(boost::asio::bind_executor(strand_, [this](std::error_code ec) {
            if (ec)
               return; // cancelled
            fireExpired();
         }));
      }

      void fireExpired() {
         auto const now = Clock::now();
         std::vector<std::uint16_t> toRetx;

         for (auto& [seq, e] : entries_) {
            if (e.deadline <= now)
               toRetx.push_back(seq);
         }

         for (auto seq : toRetx) {
            auto it = entries_.find(seq);
            if (it == entries_.end())
               continue;
            it->second.onRetx(it->second.pkt);
            // Double the timeout (binary exponential backoff, cap at 8×).
            auto& e = it->second;
            auto timeout = std::chrono::duration_cast<std::chrono::milliseconds>(e.deadline - (now - std::chrono::milliseconds{200}));
            timeout = std::min(timeout * 2, std::chrono::milliseconds{1600});
            e.deadline = now + timeout;
         }

         scheduleNext();
      }

      boost::asio::steady_timer timer_;
      Strand& strand_;
      std::unordered_map<std::uint16_t, Entry> entries_;
   };

} // namespace rude::detail

#endif // RUDE_DETAIL_RETRANSMITQUEUE_HPP
