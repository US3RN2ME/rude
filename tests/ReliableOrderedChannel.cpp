#include <array>
#include <boost/asio/buffer.hpp>
#include <cstddef>
#include <cstdint>
#include <rude/detail/DefaultCodec.hpp>
#include <rude/detail/channel/ReliableOrderedChannel.hpp>
#include <rude/detail/congestion/Null.hpp>
#include <rude/session/ChannelConfig.hpp>
#include <rude/session/SocketStats.hpp>
#include <string>

#include "Helpers.hpp"
#include "ut_main.hpp"

namespace {
   suite<"[ReliableOrderedChannel]"> _ = [] {
      "[ReordersBeforeDelivery]"_test = [] {
         rude::ReliableChannelConfig cfg;
         cfg.sendWindow_ = 4;
         int strand = 0;
         rude::ReliableOrderedChannel<rude::detail::Null> channel{cfg, strand};
         rude::SocketStats stats;
         auto zero = rude::test::bytes("zero");
         auto one = rude::test::bytes("one");
         std::array<std::byte, 8> recvBuffer{};
         std::size_t receivedSize = 0;

         channel.asyncRecv(boost::asio::buffer(recvBuffer), [&](std::error_code ec, std::size_t size) {
            expect(!ec);
            receivedSize = size;
         });
         channel.onRecv(rude::test::dataPacket(1, one), stats);
         expect(eq(receivedSize, std::size_t{0}));
         channel.onRecv(rude::test::dataPacket(0, zero), stats);

         expect(eq(receivedSize, zero.size()));
         expect(eq(std::string{reinterpret_cast<char const*>(recvBuffer.data()), receivedSize}, std::string{"zero"}));
         expect(eq(stats.snapshot().packetsRecv, std::uint64_t{2}));

         receivedSize = 0;
         channel.asyncRecv(boost::asio::buffer(recvBuffer), [&](std::error_code ec, std::size_t size) {
            expect(!ec);
            receivedSize = size;
         });
         expect(eq(receivedSize, one.size()));
         expect(eq(std::string{reinterpret_cast<char const*>(recvBuffer.data()), receivedSize}, std::string{"one"}));
      };

      "[AckFreesSendWindow]"_test = [] {
         rude::ReliableChannelConfig cfg;
         cfg.sendWindow_ = 1;
         int strand = 0;
         rude::ReliableOrderedChannel<rude::detail::Null> channel{cfg, strand};
         rude::test::MockSocket socket;
         rude::DefaultCodec codec;
         rude::SocketStats stats;
         boost::asio::ip::udp::endpoint remote;
         auto payload = rude::test::bytes("x");
         std::error_code first;
         std::error_code second;
         std::error_code third;

         channel.asyncSend(payload, socket, remote, codec, stats, [&](std::error_code ec) {
            first = ec;
         });
         channel.asyncSend(payload, socket, remote, codec, stats, [&](std::error_code ec) {
            second = ec;
         });

         rude::Packet ack;
         ack.type_ = rude::PacketType::Ack;
         ack.ack_ = 0;
         channel.onRecv(ack, stats);

         channel.asyncSend(payload, socket, remote, codec, stats, [&](std::error_code ec) {
            third = ec;
         });

         expect(!first);
         expect(eq(second, rude::makeErrorCode(rude::Error::SendWindowFull)));
         expect(!third);
         expect(eq(stats.snapshot().sendWindowUsed, std::size_t{1}));
      };
   };
} // namespace
