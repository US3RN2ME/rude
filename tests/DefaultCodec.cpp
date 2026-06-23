#include <boost/asio/buffer.hpp>
#include <cstdint>
#include <rude/detail/DefaultCodec.hpp>
#include <rude/protocol/Packet.hpp>
#include <string>
#include <vector>

#include "Helpers.hpp"
#include "ut_main.hpp"

namespace {
   suite<"[DefaultCodec]"> _ = [] {
      "[RoundTripsPacket]"_test = [] {
         rude::DefaultCodec codec;
         auto payload = rude::test::bytes("payload");
         rude::Packet packet;
         packet.type_ = rude::PacketType::Data;
         packet.seq_ = 42;
         packet.ack_ = 40;
         packet.ackBits_ = 0b11;
         packet.channel_ = 3;
         packet.payload_ = payload;

         std::vector<std::byte> encoded(payload.size() + codec.maxOverhead());
         auto const encodedSize = codec.encode(packet, boost::asio::buffer(encoded));
         encoded.resize(encodedSize);

         auto decoded = codec.decode(boost::asio::buffer(encoded));

         expect(decoded.has_value());
         expect(decoded->type_ == rude::PacketType::Data);
         expect(eq(decoded->seq_, std::uint16_t{42}));
         expect(eq(decoded->ack_, std::uint16_t{40}));
         expect(eq(decoded->ackBits_, std::uint32_t{0b11}));
         expect(eq(decoded->channel_, std::uint8_t{3}));
         expect(eq(rude::test::text(decoded->payload_), std::string{"payload"}));
      };

      "[RejectsCorruptPacket]"_test = [] {
         rude::DefaultCodec codec;
         auto payload = rude::test::bytes("payload");
         rude::Packet packet;
         packet.payload_ = payload;

         std::vector<std::byte> encoded(payload.size() + codec.maxOverhead());
         auto const encodedSize = codec.encode(packet, boost::asio::buffer(encoded));
         encoded.resize(encodedSize);
         encoded[0] = static_cast<std::byte>(0xFF);

         auto decoded = codec.decode(boost::asio::buffer(encoded));

         expect(!decoded.has_value());
         expect(decoded.error() == rude::Error::BadPacket);
      };
   };
} // namespace
