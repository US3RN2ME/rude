#include <chrono>
#include <iostream>
#include <rude/detail/congestion/BbrLite.hpp>
#include <rude/detail/congestion/LeakyBucket.hpp>
#include <rude/detail/congestion/Null.hpp>

int main() {
   rude::detail::Null noCongestion;
   std::cout << "null controller window: " << noCongestion.sendWindow() << '\n';

   rude::LeakyBucket leakyBucket{1000.0, 16.0};
   std::cout << "leaky bucket initial window: " << leakyBucket.sendWindow() << '\n';
   leakyBucket.onLoss(1);
   leakyBucket.onAck(1, 1'000);
   std::cout << "leaky bucket pacing ns: " << leakyBucket.pacingDelay().count() << '\n';

   rude::BbrLite bbr;
   bbr.onAck(1, 2'000);
   std::cout << "bbr-lite estimated window: " << bbr.sendWindow() << '\n';
   std::cout << "bbr-lite pacing ns: " << bbr.pacingDelay().count() << '\n';
}
