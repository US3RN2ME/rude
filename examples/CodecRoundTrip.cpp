#include <boost/asio/buffer.hpp>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <rude/detail/DefaultCodec.hpp>
#include <rude/protocol/Packet.hpp>
#include <string>
#include <vector>

namespace {
   [[nodiscard]] std::vector<std::byte> bytes(std::string const& text) {
      std::vector<std::byte> out(text.size());
      std::memcpy(out.data(), text.data(), text.size());
      return out;
   }

   [[nodiscard]] std::string text(std::span<std::byte const> payload) {
      return {reinterpret_cast<char const*>(payload.data()), payload.size()};
   }
} // namespace

int main() {
   rude::DefaultCodec codec;
   auto payload = bytes("hello over rude");

   rude::Packet packet;
   packet.type_ = rude::PacketType::Data;
   packet.seq_ = 7;
   packet.ack_ = 5;
   packet.ackBits_ = 0b11;
   packet.channel_ = 1;
   packet.payload_ = payload;

   std::vector<std::byte> wire(payload.size() + codec.maxOverhead());
   auto const encodedSize = codec.encode(packet, boost::asio::buffer(wire));
   wire.resize(encodedSize);

   auto decoded = codec.decode(boost::asio::buffer(wire));
   if (!decoded) {
      std::cerr << "decode failed\n";
      return 1;
   }

   std::cout << "encoded bytes: " << wire.size() << '\n';
   std::cout << "channel: " << static_cast<int>(decoded->channel_) << '\n';
   std::cout << "seq: " << decoded->seq_ << ", ack: " << decoded->ack_ << '\n';
   std::cout << "payload: " << text(decoded->payload_) << '\n';
}
