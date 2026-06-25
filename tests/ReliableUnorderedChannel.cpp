#include <array>
#include <boost/asio/buffer.hpp>
#include <cstddef>
#include <cstdint>
#include <rude/detail/channel/ReliableUnorderedChannel.hpp>
#include <rude/detail/congestion/Null.hpp>
#include <rude/session/ChannelConfig.hpp>
#include <rude/session/SocketStats.hpp>

#include "Helpers.hpp"
#include "ut_main.hpp"

namespace {
   suite<"[ReliableUnorderedChannel]"> _ = [] {
      "[SuppressesDuplicateDelivery]"_test = [] {
         rude::ReliableChannelConfig cfg;
         cfg.sendWindow_ = 4;
         int strand = 0;
         rude::ReliableUnorderedChannel<rude::detail::Null> channel{cfg, strand};
         rude::SocketStats stats;
         auto payload = rude::test::bytes("dup");

         channel.onRecv(rude::test::dataPacket(7, payload), stats);
         channel.onRecv(rude::test::dataPacket(7, payload), stats);

         std::array<std::byte, 8> recvBuffer{};
         std::size_t receivedSize = 0;
         channel.asyncRecv(boost::asio::buffer(recvBuffer), [&](std::error_code ec, std::size_t size) {
            expect(!ec);
            receivedSize = size;
         });

         expect(eq(receivedSize, payload.size()));
         expect(eq(stats.snapshot().packetsRecv, std::uint64_t{1}));
      };
   };
} // namespace
