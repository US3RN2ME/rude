#include <cstddef>
#include <cstdint>
#include <rude/detail/SlidingWindow.hpp>

#include "ut_main.hpp"

namespace {
   suite<"[SlidingWindow]"> _ = [] {
      "[AccountsForAckBits]"_test = [] {
         rude::detail::SlidingWindow window{8};
         window.onSend();
         window.onSend();
         window.onSend();

         expect(eq(window.inFlight(), std::size_t{3}));
         expect(window.onAck(2, 0b11));
         expect(eq(window.inFlight(), std::size_t{0}));
         expect(eq(window.base(), std::uint16_t{2}));
      };
   };
} // namespace
