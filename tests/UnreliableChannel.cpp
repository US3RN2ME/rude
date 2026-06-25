#include <array>
#include <boost/asio/buffer.hpp>
#include <cstddef>
#include <cstdint>
#include <rude/detail/DefaultCodec.hpp>
#include <rude/detail/channel/UnreliableChannel.hpp>
#include <rude/session/ChannelConfig.hpp>
#include <rude/session/SocketStats.hpp>
#include <string>

#include "Helpers.hpp"
#include "ut_main.hpp"

namespace {
   suite<"[UnreliableChannel]"> _ = [] {
      "[SendsEncodedPacketAndDropsStaleReceive]"_test = [] {
         rude::UnreliableChannelConfig cfg;
         cfg.id_ = 2;
         rude::UnreliableChannel channel{cfg};
         rude::test::MockSocket socket;
         rude::DefaultCodec codec;
         rude::SocketStats stats;
         boost::asio::ip::udp::endpoint remote;
         auto payload = rude::test::bytes("hello");
         bool sent = false;

         channel.asyncSend(payload, socket, remote, codec, stats, [&](std::error_code ec) {
            expect(!ec);
            sent = true;
         });

         expect(sent);
         expect(eq(socket.sends, std::size_t{1}));
         expect(eq(stats.snapshot().packetsSent, std::uint64_t{1}));

         std::array<std::byte, 8> recvBuffer{};
         std::size_t receivedSize = 0;
         channel.asyncRecv(boost::asio::buffer(recvBuffer), [&](std::error_code ec, std::size_t size) {
            expect(!ec);
            receivedSize = size;
         });
         channel.onRecv(rude::test::dataPacket(3, payload), stats);
         channel.onRecv(rude::test::dataPacket(2, payload), stats);

         expect(eq(receivedSize, payload.size()));
         expect(eq(std::string{reinterpret_cast<char const*>(recvBuffer.data()), receivedSize}, std::string{"hello"}));
         expect(eq(stats.snapshot().packetsRecv, std::uint64_t{1}));
      };
   };
} // namespace
