#include <cstdint>
#include <rude/detail/congestion/BbrLite.hpp>
#include <rude/detail/congestion/LeakyBucket.hpp>
#include <rude/detail/congestion/Null.hpp>

#include "ut_main.hpp"

namespace {
   suite<"[Congestion]"> _ = [] {
      "[ControllersExposeUsableWindows]"_test = [] {
         rude::detail::Null nullController;
         expect(nullController.sendWindow() > 0_u);
         expect(eq(nullController.pacingDelay().count(), std::int64_t{0}));

         rude::LeakyBucket bucket{10.0, 4.0};
         expect(bucket.sendWindow() <= 4_u);
         bucket.onLoss(1);
         bucket.onAck(1, 100);
         expect(bucket.pacingDelay().count() >= 0_i);

         rude::BbrLite bbr;
         bbr.onAck(1, 1000);
         expect(bbr.sendWindow() > 0_u);
         bbr.onLoss(1);
         expect(bbr.pacingDelay().count() >= 0_i);
      };
   };
} // namespace
