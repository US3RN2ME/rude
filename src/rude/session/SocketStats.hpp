
#ifndef RUDE_SESSION_SOCKETSTATS_HPP
#define RUDE_SESSION_SOCKETSTATS_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace rude {

   /// Live per-session counters. All fields are atomics so they can be read from
   /// any thread without locking. Call snapshot() for a consistent non-atomic copy.
   struct SocketStats {
      std::atomic<std::uint64_t> bytesSent_{0};
      std::atomic<std::uint64_t> bytesRecv_{0};
      std::atomic<std::uint64_t> packetsSent_{0};
      std::atomic<std::uint64_t> packetsRecv_{0};
      std::atomic<std::uint64_t> packetsLost_{0}; ///< Retransmit timeouts fired
      std::atomic<double> avgRttUs_{0.0};
      std::atomic<double> jitterUs_{0.0};
      std::atomic<std::size_t> sendWindowUsed_{0}; ///< Packets currently in-flight

      /// Returns a non-atomic copy. Not perfectly consistent across fields
      /// but sufficient for diagnostics and metrics export.
      struct Snapshot {
         std::uint64_t bytesSent;
         std::uint64_t bytesRecv;
         std::uint64_t packetsSent;
         std::uint64_t packetsRecv;
         std::uint64_t packetsLost;
         double avgRttUs;
         double jitterUs;
         std::size_t sendWindowUsed;
      };

      [[nodiscard]] Snapshot snapshot() const noexcept {
         return {
             bytesSent_.load(std::memory_order_relaxed),   bytesRecv_.load(std::memory_order_relaxed),
             packetsSent_.load(std::memory_order_relaxed), packetsRecv_.load(std::memory_order_relaxed),
             packetsLost_.load(std::memory_order_relaxed), avgRttUs_.load(std::memory_order_relaxed),
             jitterUs_.load(std::memory_order_relaxed),    sendWindowUsed_.load(std::memory_order_relaxed),
         };
      }

      SocketStats() = default;
      SocketStats(SocketStats const&) = delete;
      SocketStats& operator=(SocketStats const&) = delete;
   };

} // namespace rude

#endif // RUDE_SESSION_SOCKETSTATS_HPP
