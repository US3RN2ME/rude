#ifndef RUDE_DETAIL_CHANNEL_UNRELIABLECHANNEL_HPP
#define RUDE_DETAIL_CHANNEL_UNRELIABLECHANNEL_HPP

#include <atomic>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/udp.hpp>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <rude/concepts/PacketCodec.hpp>
#include <rude/concepts/Socket.hpp>
#include <rude/core/Error.hpp>
#include <rude/detail/channel/ChannelCommon.hpp>
#include <rude/protocol/Packet.hpp>
#include <rude/session/ChannelConfig.hpp>
#include <rude/session/SocketStats.hpp>
#include <span>
#include <system_error>
#include <vector>

namespace rude {

   /// Fire-and-forget channel with sequenced delivery.
   ///
   /// Send path never buffers or retransmits. Receive path drops stale sequence
   /// numbers and keeps only packets newer than the last delivered packet.
   class UnreliableChannel {
   public:
      using RecvHandler = detail::channel::RecvHandler;

      explicit UnreliableChannel(ChannelConfig const& cfg) noexcept
          : id_{cfg.id_} {}

      UnreliableChannel() = default;

      template <PacketCodec Codec, Socket Sock>
      void asyncSend(std::span<std::byte const> payload, Sock& sock, boost::asio::ip::udp::endpoint const& remoteEndpoint,
                     Codec& codec, SocketStats& stats, std::function<void(std::error_code)> handler) {
         Packet packet;
         packet.type_ = PacketType::Data;
         packet.seq_ = nextSeq_++;
         packet.channel_ = id_;
         packet.payload_ = payload;

         std::vector<std::byte> encodedPacket(payload.size() + codec.maxOverhead());
         const auto encodedSize = codec.encode(packet, boost::asio::buffer(encodedPacket));
         if (encodedSize == 0) {
            handler(makeErrorCode(Error::BadPacket));
            return;
         }

         encodedPacket.resize(encodedSize);

         stats.packetsSent_.fetch_add(1, std::memory_order_relaxed);
         stats.bytesSent_.fetch_add(encodedSize, std::memory_order_relaxed);

         detail::channel::sendEncoded(sock, remoteEndpoint, std::move(encodedPacket), std::move(handler));
      }

      void asyncRecv(boost::asio::mutable_buffer buf, RecvHandler handler) {
         detail::channel::asyncRecv(queuedMessages_, pendingRecv_, buf, std::move(handler));
      }

      void onRecv(Packet const& packet, SocketStats& stats) {
         if (packet.type_ != PacketType::Data) {
            return;
         }

         if (hasDelivered_ && detail::channel::isStaleOrDuplicate(packet.seq_, lastSeq_)) {
            return;
         }

         hasDelivered_ = true;
         lastSeq_ = packet.seq_;

         stats.packetsRecv_.fetch_add(1, std::memory_order_relaxed);
         stats.bytesRecv_.fetch_add(packet.payload_.size(), std::memory_order_relaxed);

         detail::channel::deliverOrQueue(queuedMessages_, pendingRecv_, packet.payload_);
      }

      template <PacketCodec Codec, Socket Sock>
      void flush(Sock&, boost::asio::ip::udp::endpoint const&, Codec&, std::error_code&) noexcept {}

      [[nodiscard]] std::optional<Packet> buildAck() const noexcept {
         return std::nullopt;
      }

   private:
      std::uint8_t id_ = 0;
      std::uint16_t nextSeq_ = 0;
      std::uint16_t lastSeq_ = 0;
      bool hasDelivered_ = false;
      std::deque<detail::channel::ReceivedMessage> queuedMessages_;
      std::optional<detail::channel::PendingRecv> pendingRecv_;
   };

} // namespace rude

#endif // RUDE_DETAIL_CHANNEL_UNRELIABLECHANNEL_HPP
