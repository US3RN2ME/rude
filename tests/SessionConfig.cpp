#include <cstddef>
#include <cstdint>
#include <rude/protocol/Types.hpp>
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
         expect(channel.reliability_ == rude::ReliabilityMode::Reliable);
         expect(channel.ordering_ == rude::OrderingMode::Ordered);
         expect(eq(channel.sendWindow_, std::uint16_t{128}));
      };
   };
} // namespace
