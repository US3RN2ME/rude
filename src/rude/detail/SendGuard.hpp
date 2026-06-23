#ifndef RUDE_DETAIL_SENDGUARD_HPP
#define RUDE_DETAIL_SENDGUARD_HPP

#include <boost/asio/dispatch.hpp>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <rude/core/Executor.hpp>
#include <system_error>
#include <utility>

namespace rude::detail {

   /// Serializes asynchronous send work and gives lower numeric priority first.
   ///
   /// A job receives a completion callback and must invoke it exactly once when
   /// its async send operation finishes. The guard then starts the next job.
   class SendGuard {
   public:
      using Completion = std::function<void(std::error_code)>;
      using Job = std::function<void(Completion)>;

      SendGuard() = default;

      explicit SendGuard(Strand& strand) noexcept
          : strand_{&strand} {}

      void reset(Strand& strand) noexcept {
         strand_ = &strand;
         active_ = false;
         jobs_.clear();
      }

      void clear() noexcept {
         active_ = false;
         jobs_.clear();
      }

      void enqueue(std::uint8_t priority, Job job) {
         auto entry = Entry{priority, std::move(job)};
         auto insertAt = jobs_.begin();
         while (insertAt != jobs_.end() && insertAt->priority <= priority) {
            ++insertAt;
         }
         jobs_.insert(insertAt, std::move(entry));
         pump();
      }

      [[nodiscard]] bool active() const noexcept {
         return active_;
      }
      [[nodiscard]] bool empty() const noexcept {
         return jobs_.empty() && !active_;
      }
      [[nodiscard]] std::size_t size() const noexcept {
         return jobs_.size() + (active_ ? 1U : 0U);
      }

   private:
      struct Entry {
         std::uint8_t priority = 0;
         Job job;
      };

      void pump() {
         if (active_ || jobs_.empty()) {
            return;
         }

         active_ = true;
         auto entry = std::move(jobs_.front());
         jobs_.pop_front();

         auto run = [this, entry = std::move(entry)]() mutable {
            entry.job([this](std::error_code) {
               active_ = false;
               pump();
            });
         };

         if (strand_ == nullptr) {
            run();
            return;
         }

         boost::asio::dispatch(*strand_, std::move(run));
      }

      Strand* strand_ = nullptr;
      bool active_ = false;
      std::deque<Entry> jobs_;
   };

} // namespace rude::detail

#endif // RUDE_DETAIL_SENDGUARD_HPP
