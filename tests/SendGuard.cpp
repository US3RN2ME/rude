#include <cstddef>
#include <functional>
#include <rude/detail/SendGuard.hpp>
#include <system_error>
#include <vector>

#include "ut_main.hpp"

namespace {
   suite<"[SendGuard]"> _ = [] {
      "[RunsQueuedJobsByPriority]"_test = [] {
         rude::detail::SendGuard guard;
         std::vector<int> order;
         rude::detail::SendGuard::Completion firstCompletion;

         guard.enqueue(10, [&](auto completion) {
            order.push_back(1);
            firstCompletion = std::move(completion);
         });
         guard.enqueue(20, [&](auto completion) {
            order.push_back(3);
            completion({});
         });
         guard.enqueue(5, [&](auto completion) {
            order.push_back(2);
            completion({});
         });

         expect(eq(order.size(), std::size_t{1}));
         firstCompletion({});
         expect(eq(order.size(), std::size_t{3}));
         expect(eq(order[0], 1));
         expect(eq(order[1], 2));
         expect(eq(order[2], 3));
         expect(guard.empty());
      };
   };
} // namespace
