#include <cstdint>
#include <iostream>
#include <rude/protocol/Types.hpp>
#include <rude/session/ChannelConfig.hpp>
#include <rude/session/SessionConfig.hpp>
#include <vector>

int main() {
   rude::SessionConfig session;
   session.mtu_ = 1200;
   session.keepaliveMs_ = 2'000;
   session.timeoutMs_ = 10'000;
   session.maxChannels_ = 3;

   std::vector<rude::ChannelConfig> channels;

   rude::ChannelConfig ordered;
   ordered.id_ = 0;
   ordered.reliability_ = rude::ReliabilityMode::Reliable;
   ordered.ordering_ = rude::OrderingMode::Ordered;
   ordered.sendWindow_ = 128;
   channels.push_back(ordered);

   rude::ChannelConfig unordered;
   unordered.id_ = 1;
   unordered.reliability_ = rude::ReliabilityMode::Reliable;
   unordered.ordering_ = rude::OrderingMode::None;
   unordered.sendWindow_ = 256;
   channels.push_back(unordered);

   rude::ChannelConfig realtime;
   realtime.id_ = 2;
   realtime.reliability_ = rude::ReliabilityMode::Unreliable;
   realtime.ordering_ = rude::OrderingMode::Sequenced;
   channels.push_back(realtime);

   std::cout << "session mtu: " << session.mtu_ << '\n';
   std::cout << "channels configured: " << static_cast<int>(session.maxChannels_) << '\n';

   for (auto const& channel : channels) {
      std::cout << "channel " << static_cast<int>(channel.id_) << " window=" << channel.sendWindow_ << '\n';
   }
}
