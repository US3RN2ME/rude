#include <rude/detail/AckedSet.hpp>

#include "ut_main.hpp"

namespace {
   suite<"[AckedSet]"> _ = [] {
      "[SuppressesDuplicates]"_test = [] {
         rude::detail::AckedSet<8> acked;

         expect(!acked.seen(2));
         acked.mark(2);
         expect(acked.seen(2));
      };
   };
} // namespace
