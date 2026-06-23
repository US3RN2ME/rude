#include <cstddef>
#include <cstdint>
#include <rude/session/SocketStats.hpp>

#include "ut_main.hpp"

namespace {
   suite<"[SocketStats]"> _ = [] {
      "[SnapshotsAtomicCounters]"_test = [] {
         rude::SocketStats stats;
         stats.bytesSent_.store(10);
         stats.bytesRecv_.store(20);
         stats.packetsSent_.store(1);
         stats.packetsRecv_.store(2);
         stats.sendWindowUsed_.store(3);

         auto snapshot = stats.snapshot();

         expect(eq(snapshot.bytesSent, std::uint64_t{10}));
         expect(eq(snapshot.bytesRecv, std::uint64_t{20}));
         expect(eq(snapshot.packetsSent, std::uint64_t{1}));
         expect(eq(snapshot.packetsRecv, std::uint64_t{2}));
         expect(eq(snapshot.sendWindowUsed, std::size_t{3}));
      };
   };
} // namespace
