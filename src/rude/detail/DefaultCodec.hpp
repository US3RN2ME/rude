
#ifndef RUDE_DETAIL_DEFAULTCODEC_HPP
#define RUDE_DETAIL_DEFAULTCODEC_HPP

#include <array>
#include <bit>
#include <boost/asio/buffer.hpp>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <rude/core/Error.hpp>
#include <rude/protocol/Packet.hpp>
#include <span>

namespace rude {

   /// Wire layout (all fields little-endian):
   ///   Offset  Size  Field
   ///   0       1     type_    (PacketType)
   ///   1       2     seq_
   ///   3       2     ack_
   ///   5       4     ackBits_
   ///   9       1     channel_
   ///   ---- 10 bytes header ----
   ///   10      N     payload
   ///   10+N    4     crc32 (xxHash-inspired; covers bytes 0..10+N-1)
   ///
   /// Total overhead = 14 bytes. maxOverhead() returns 14.
   class DefaultCodec {
   public:
      static constexpr std::size_t kHeaderSize = 10;
      static constexpr std::size_t kCrcSize = 4;
      static constexpr std::size_t kOverhead = kHeaderSize + kCrcSize;
      static constexpr std::uint8_t kVersion = 1;

      [[nodiscard]] std::size_t encode(Packet const& pkt, boost::asio::mutable_buffer outBuf) const noexcept {
         std::size_t const total = kHeaderSize + pkt.payload_.size() + kCrcSize;
         if (outBuf.size() < total)
            return 0;

         auto* p = static_cast<std::byte*>(outBuf.data());
         p[0] = static_cast<std::byte>(pkt.type_);
         writeU16(p + 1, pkt.seq_);
         writeU16(p + 3, pkt.ack_);
         writeU32(p + 5, pkt.ackBits_);
         p[9] = static_cast<std::byte>(pkt.channel_);
         std::memcpy(p + kHeaderSize, pkt.payload_.data(), pkt.payload_.size());

         std::uint32_t const crc = checksum(p, kHeaderSize + pkt.payload_.size());
         writeU32(p + kHeaderSize + pkt.payload_.size(), crc);
         return total;
      }

      [[nodiscard]] std::expected<Packet, Error> decode(boost::asio::const_buffer inBuf) const noexcept {
         if (inBuf.size() < kHeaderSize + kCrcSize)
            return std::unexpected(Error::BadPacket);

         auto const* p = static_cast<std::byte const*>(inBuf.data());
         std::size_t payloadSize = inBuf.size() - kHeaderSize - kCrcSize;

         // Verify checksum.
         std::uint32_t const storedCrc = readU32(p + kHeaderSize + payloadSize);
         std::uint32_t const computedCrc = checksum(p, kHeaderSize + payloadSize);
         if (storedCrc != computedCrc)
            return std::unexpected(Error::BadPacket);

         Packet pkt;
         pkt.type_ = static_cast<PacketType>(std::to_integer<std::uint8_t>(p[0]));
         pkt.seq_ = readU16(p + 1);
         pkt.ack_ = readU16(p + 3);
         pkt.ackBits_ = readU32(p + 5);
         pkt.channel_ = static_cast<std::uint8_t>(p[9]);
         pkt.payload_ = {reinterpret_cast<std::byte const*>(p + kHeaderSize), payloadSize};
         pkt.crc_ = storedCrc;
         return pkt;
      }

      [[nodiscard]] std::size_t maxOverhead() const noexcept {
         return kOverhead;
      }
      [[nodiscard]] std::uint8_t version() const noexcept {
         return kVersion;
      }

   private:
      // ── Byte helpers (little-endian) ────────────────────────────────────────
      static void writeU16(std::byte* p, std::uint16_t v) noexcept {
         std::uint16_t le = std::endian::native == std::endian::little ? v : std::byteswap(v);
         std::memcpy(p, &le, 2);
      }
      static void writeU32(std::byte* p, std::uint32_t v) noexcept {
         std::uint32_t le = std::endian::native == std::endian::little ? v : std::byteswap(v);
         std::memcpy(p, &le, 4);
      }
      static std::uint16_t readU16(std::byte const* p) noexcept {
         std::uint16_t v;
         std::memcpy(&v, p, 2);
         return std::endian::native == std::endian::little ? v : std::byteswap(v);
      }
      static std::uint32_t readU32(std::byte const* p) noexcept {
         std::uint32_t v;
         std::memcpy(&v, p, 4);
         return std::endian::native == std::endian::little ? v : std::byteswap(v);
      }

      // ── Lightweight FNV-1a 32-bit checksum ──────────────────────────────────
      static std::uint32_t checksum(std::byte const* data, std::size_t len) noexcept {
         std::uint32_t h = 2166136261u;
         for (std::size_t i = 0; i < len; ++i) {
            h ^= static_cast<std::uint8_t>(data[i]);
            h *= 16777619u;
         }
         return h;
      }
   };

} // namespace rude

#endif // RUDE_DETAIL_DEFAULTCODEC_HPP
