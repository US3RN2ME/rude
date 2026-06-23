#include <cstddef>
#include <cstdint>
#include <rude/detail/ReorderBuffer.hpp>

#include "Helpers.hpp"
#include "ut_main.hpp"

namespace {
   suite<"[ReorderBuffer]"> _ = [] {
      "[DrainsContiguousPackets]"_test = [] {
         rude::detail::ReorderBuffer<4> reorder;
         auto first = rude::test::bytes("first");
         auto second = rude::test::bytes("second");

         reorder.insert(rude::test::dataPacket(1, second));
         expect(reorder.drain().empty());
         reorder.insert(rude::test::dataPacket(0, first));
         auto drained = reorder.drain();

         expect(eq(drained.size(), std::size_t{2}));
         expect(eq(drained[0].seq_, std::uint16_t{0}));
         expect(eq(drained[1].seq_, std::uint16_t{1}));
         expect(eq(reorder.nextSeq(), std::uint16_t{2}));
      };
   };
} // namespace
