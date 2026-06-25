#include <cstddef>
#include <cstdint>
#include <rude/session/ChannelConfig.hpp>
#include <rude/session/SessionConfig.hpp>

#include "ut_main.hpp"

namespace {
   suite<"[SessionConfig]"> _ = [] {
      "[ProvidesSaneDefaults]"_test = [] {
         rude::SessionConfig session;
         rude::ChannelConfig channel;

         expect(eq(session.mtu_, std::size_t{1400}));
         expect(eq(session.maxChannels_, std::uint8_t{8}));
         expect(session.timeoutMs_ > session.keepaliveMs_);
         expect(channel.mode_ == rude::ChannelMode::ReliableOrdered);
         expect(eq(channel.sendWindow_, std::uint16_t{128}));
      };

      "[ProvidesNamedChannelProfiles]"_test = [] {
         auto ordered = rude::ChannelConfig::reliableOrdered(0);
         auto unordered = rude::ChannelConfig::reliableUnordered(1);
         auto realtime = rude::ChannelConfig::unreliableSequenced(2);

         expect(ordered.mode_ == rude::ChannelMode::ReliableOrdered);
         expect(unordered.mode_ == rude::ChannelMode::ReliableUnordered);
         expect(realtime.mode_ == rude::ChannelMode::UnreliableSequenced);
         expect(eq(ordered.reliableOptions().id_, std::uint8_t{0}));
         expect(eq(realtime.unreliableOptions().id_, std::uint8_t{2}));
      };
   };
} // namespace
