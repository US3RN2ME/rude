#include <array>
#include <boost/asio/buffer.hpp>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <rude/detail/channel/ReliableOrderedChannel.hpp>
#include <rude/detail/channel/ReliableUnorderedChannel.hpp>
#include <rude/detail/channel/UnreliableChannel.hpp>
#include <rude/detail/congestion/Null.hpp>
#include <rude/protocol/Packet.hpp>
#include <rude/session/ChannelConfig.hpp>
#include <rude/session/SocketStats.hpp>
#include <string>
#include <vector>

namespace {
   [[nodiscard]] std::vector<std::byte> bytes(std::string const& text) {
      std::vector<std::byte> out(text.size());
      std::memcpy(out.data(), text.data(), text.size());
      return out;
   }

   [[nodiscard]] rude::Packet data(std::uint16_t seq, std::span<std::byte const> payload) {
      rude::Packet packet;
      packet.type_ = rude::PacketType::Data;
      packet.seq_ = seq;
      packet.payload_ = payload;
      return packet;
   }

   [[nodiscard]] std::string text(std::byte const* data, std::size_t size) {
      return {reinterpret_cast<char const*>(data), size};
   }
} // namespace

int main() {
   rude::ReliableChannelConfig reliableCfg;
   reliableCfg.sendWindow_ = 8;
   int strandPlaceholder = 0;
   rude::SocketStats stats;

   rude::ReliableOrderedChannel<rude::detail::Null> ordered{reliableCfg, strandPlaceholder};
   auto first = bytes("first");
   auto second = bytes("second");
   std::array<std::byte, 32> orderedBuffer{};
   std::size_t orderedSize = 0;

   ordered.asyncRecv(boost::asio::buffer(orderedBuffer), [&](std::error_code, std::size_t size) {
      orderedSize = size;
   });
   ordered.onRecv(data(1, second), stats);
   ordered.onRecv(data(0, first), stats);

   std::cout << "reliable ordered delivers seq=0 first: " << text(orderedBuffer.data(), orderedSize) << '\n';

   orderedSize = 0;
   ordered.asyncRecv(boost::asio::buffer(orderedBuffer), [&](std::error_code, std::size_t size) {
      orderedSize = size;
   });
   std::cout << "reliable ordered then releases buffered seq=1: " << text(orderedBuffer.data(), orderedSize) << '\n';

   rude::ReliableUnorderedChannel<rude::detail::Null> unordered{reliableCfg, strandPlaceholder};
   auto duplicate = bytes("duplicate");
   unordered.onRecv(data(10, duplicate), stats);
   unordered.onRecv(data(10, duplicate), stats);

   std::array<std::byte, 32> unorderedBuffer{};
   std::size_t unorderedSize = 0;
   unordered.asyncRecv(boost::asio::buffer(unorderedBuffer), [&](std::error_code, std::size_t size) {
      unorderedSize = size;
   });

   std::cout << "reliable unordered suppresses duplicate seq=10: " << text(unorderedBuffer.data(), unorderedSize) << '\n';

   rude::UnreliableChannelConfig realtimeCfg;
   rude::UnreliableChannel unreliable{realtimeCfg};
   auto newest = bytes("newest");
   auto stale = bytes("stale");
   std::array<std::byte, 32> unreliableBuffer{};
   std::size_t unreliableSize = 0;

   unreliable.asyncRecv(boost::asio::buffer(unreliableBuffer), [&](std::error_code, std::size_t size) {
      unreliableSize = size;
   });
   unreliable.onRecv(data(20, newest), stats);
   unreliable.onRecv(data(19, stale), stats);

   std::cout << "unreliable sequenced drops stale seq=19 after seq=20: " << text(unreliableBuffer.data(), unreliableSize)
             << '\n';
}
