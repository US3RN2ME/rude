#include <cstdint>
#include <iostream>
#include <rude/session/ChannelConfig.hpp>
#include <rude/session/SessionConfig.hpp>
#include <vector>

namespace {
   [[nodiscard]] char const* modeName(rude::ChannelMode mode) noexcept {
      switch (mode) {
         case rude::ChannelMode::ReliableOrdered:
            return "reliable ordered";
         case rude::ChannelMode::ReliableUnordered:
            return "reliable unordered";
         case rude::ChannelMode::UnreliableSequenced:
            return "unreliable sequenced";
      }

      return "unknown";
   }
} // namespace

int main() {
   rude::SessionConfig session;
   session.mtu_ = 1200;
   session.keepaliveMs_ = 2'000;
   session.timeoutMs_ = 10'000;
   session.maxChannels_ = 3;

   std::vector<rude::ChannelConfig> channels;

   auto ordered = rude::ChannelConfig::reliableOrdered(0);
   ordered.sendWindow_ = 128;
   channels.push_back(ordered);

   auto unordered = rude::ChannelConfig::reliableUnordered(1);
   unordered.sendWindow_ = 256;
   channels.push_back(unordered);

   auto realtime = rude::ChannelConfig::unreliableSequenced(2);
   channels.push_back(realtime);

   std::cout << "session mtu: " << session.mtu_ << '\n';
   std::cout << "channels configured: " << static_cast<int>(session.maxChannels_) << '\n';

   for (auto const& channel : channels) {
      std::cout << "channel " << static_cast<int>(channel.id_) << ": " << modeName(channel.mode_);
      if (channel.mode_ != rude::ChannelMode::UnreliableSequenced) {
         std::cout << ", window=" << channel.sendWindow_;
      }
      std::cout << '\n';
   }
}
