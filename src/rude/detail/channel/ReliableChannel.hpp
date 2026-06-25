#ifndef RUDE_DETAIL_CHANNEL_RELIABLECHANNEL_HPP
#define RUDE_DETAIL_CHANNEL_RELIABLECHANNEL_HPP

#include <algorithm>
#include <atomic>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/udp.hpp>
#include <cstddef>
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
#include <utility>
#include <vector>

namespace rude::detail::channel {

   template <typename CongestionController, bool Ordered>
   class ReliableChannel {
   public:
      using RecvHandler = detail::channel::RecvHandler;

      ReliableChannel() = default;

      template <typename Strand>
      explicit ReliableChannel(ReliableChannelConfig const& cfg, Strand&)
          : id_{cfg.id_}
          , sendWindow_{normalizeWindow(cfg.sendWindow_)}
          , sentPackets_(windowCapacity(sendWindow_))
          , receiveBuffer_(windowCapacity(sendWindow_)) {}

      template <PacketCodec Codec, Socket Sock>
      void asyncSend(std::span<std::byte const> payload, Sock& sock, boost::asio::ip::udp::endpoint const& remoteEndpoint,
                     Codec& codec, SocketStats& stats, std::function<void(std::error_code)> handler) {
         if (inFlightCount_ >= effectiveSendWindow()) {
            handler(makeErrorCode(Error::SendWindowFull));
            return;
         }

         Packet packet;
         packet.type_ = PacketType::Data;
         packet.seq_ = nextSeq_++;
         packet.channel_ = id_;
         packet.payload_ = payload;
         attachAck(packet);

         auto encodedPacket = encodePacket(packet, codec, payload.size());
         if (encodedPacket.empty()) {
            handler(makeErrorCode(Error::BadPacket));
            return;
         }

         const auto sequenceNumber = packet.seq_;
         const auto encodedSize = encodedPacket.size();
         if (!storeSentPacket(sequenceNumber, encodedPacket)) {
            handler(makeErrorCode(Error::SendWindowFull));
            return;
         }

         stats.sendWindowUsed_.store(inFlightCount_, std::memory_order_relaxed);
         stats.packetsSent_.fetch_add(1, std::memory_order_relaxed);
         stats.bytesSent_.fetch_add(encodedSize, std::memory_order_relaxed);

         sendEncoded(sock, remoteEndpoint, std::move(encodedPacket),
                     [this, sequenceNumber, &stats, handler = std::move(handler)](std::error_code sendError) mutable {
                        if (sendError) {
                           eraseSentPacket(sequenceNumber);
                           stats.sendWindowUsed_.store(inFlightCount_, std::memory_order_relaxed);
                           cc_.onLoss(sequenceNumber);
                        }

                        handler(sendError);
                     });
      }

      void asyncRecv(boost::asio::mutable_buffer buf, RecvHandler handler) {
         detail::channel::asyncRecv(queuedMessages_, pendingRecv_, buf, std::move(handler));
      }

      void onRecv(Packet const& packet, SocketStats& stats) {
         if (packet.type_ == PacketType::Ack) {
            processAck(packet.ack_, packet.ackBits_, stats);
            return;
         }

         if (packet.type_ != PacketType::Data) {
            return;
         }

         if constexpr (Ordered) {
            receiveOrdered(packet, stats);
         } else {
            receiveUnordered(packet, stats);
         }
      }

      template <PacketCodec Codec, Socket Sock>
      void flush(Sock& sock, boost::asio::ip::udp::endpoint const& remoteEndpoint, Codec&, std::error_code& ec) {
         for (auto const& sentPacket : sentPackets_) {
            if (!sentPacket) {
               continue;
            }

            sendEncoded(sock, remoteEndpoint, sentPacket->encodedPacket, [&ec](std::error_code sendError) {
               if (sendError && !ec) {
                  ec = sendError;
               }
            });
         }
      }

      [[nodiscard]] std::optional<Packet> buildAck() const noexcept {
         if (!recvAckTracker_.hasReceived()) {
            return std::nullopt;
         }

         Packet packet;
         packet.type_ = PacketType::Ack;
         packet.channel_ = id_;
         packet.ack_ = recvAckTracker_.latestSequence();
         packet.ackBits_ = recvAckTracker_.ackBits();
         return packet;
      }

   private:
      struct SentPacket {
         std::uint16_t sequenceNumber;
         std::vector<std::byte> encodedPacket;
      };

      struct BufferedPacket {
         std::uint16_t sequenceNumber;
         std::vector<std::byte> payload;
      };

      static constexpr std::uint16_t kAckHistorySize = 32;
      static constexpr std::uint16_t kDefaultWindowSize = 128;
      static constexpr std::uint16_t kMaxWindowSize = 32768;

      [[nodiscard]] static constexpr std::uint16_t normalizeWindow(std::uint16_t configuredWindow) noexcept {
         if (configuredWindow == 0) {
            return 1;
         }

         return std::min(configuredWindow, kMaxWindowSize);
      }

      [[nodiscard]] static constexpr std::size_t windowCapacity(std::uint16_t configuredWindow) noexcept {
         return normalizeWindow(configuredWindow);
      }

      [[nodiscard]] static std::size_t slotIndex(std::uint16_t sequenceNumber, std::size_t capacity) noexcept {
         return sequenceNumber % capacity;
      }

      template <PacketCodec Codec>
      [[nodiscard]] static std::vector<std::byte> encodePacket(Packet const& packet, Codec& codec, std::size_t payloadSize) {
         std::vector<std::byte> encodedPacket(payloadSize + codec.maxOverhead());
         const auto encodedSize = codec.encode(packet, boost::asio::buffer(encodedPacket));
         if (encodedSize == 0) {
            return {};
         }

         encodedPacket.resize(encodedSize);
         return encodedPacket;
      }

      void attachAck(Packet& packet) const noexcept {
         if (!recvAckTracker_.hasReceived()) {
            return;
         }

         packet.ack_ = recvAckTracker_.latestSequence();
         packet.ackBits_ = recvAckTracker_.ackBits();
      }

      [[nodiscard]] std::size_t effectiveSendWindow() noexcept {
         const auto congestionWindow = cc_.sendWindow();
         return std::max<std::size_t>(1, std::min<std::size_t>(sendWindow_, congestionWindow));
      }

      void processAck(std::uint16_t ack, std::uint32_t ackBits, SocketStats& stats) {
         for (auto& sentPacket : sentPackets_) {
            if (!sentPacket || !isAcknowledged(sentPacket->sequenceNumber, ack, ackBits)) {
               continue;
            }

            const auto sequenceNumber = sentPacket->sequenceNumber;
            sentPacket.reset();
            --inFlightCount_;
            cc_.onAck(sequenceNumber, 0);
         }

         stats.sendWindowUsed_.store(inFlightCount_, std::memory_order_relaxed);
      }

      void receiveUnordered(Packet const& packet, SocketStats& stats) {
         if (isDuplicateOrStaleUnordered(packet.seq_)) {
            return;
         }

         recvAckTracker_.markReceived(packet.seq_);
         recordRecv(packet, stats);
         deliverOrQueue(queuedMessages_, pendingRecv_, packet.payload_);
      }

      void receiveOrdered(Packet const& packet, SocketStats& stats) {
         if (!isInOrderedReceiveWindow(packet.seq_)) {
            return;
         }

         recvAckTracker_.markReceived(packet.seq_);
         auto& bufferedPacket = receiveBuffer_[slotIndex(packet.seq_, receiveBuffer_.size())];
         if (bufferedPacket) {
            return;
         }

         bufferedPacket = BufferedPacket{packet.seq_, std::vector<std::byte>{packet.payload_.begin(), packet.payload_.end()}};
         drainReadyPackets(stats);
      }

      [[nodiscard]] bool isDuplicateOrStaleUnordered(std::uint16_t sequenceNumber) const noexcept {
         if (!recvAckTracker_.hasReceived()) {
            return false;
         }

         if (isNewerSequence(sequenceNumber, recvAckTracker_.latestSequence())) {
            return false;
         }

         const auto backDelta = static_cast<std::uint16_t>(recvAckTracker_.latestSequence() - sequenceNumber);
         return backDelta > kAckHistorySize ||
                isAcknowledged(sequenceNumber, recvAckTracker_.latestSequence(), recvAckTracker_.ackBits());
      }

      [[nodiscard]] bool isInOrderedReceiveWindow(std::uint16_t sequenceNumber) const noexcept {
         const auto delta = static_cast<std::uint16_t>(sequenceNumber - expectedRecvSeq_);
         return delta < std::max<std::uint16_t>(1, sendWindow_) && delta < 32768U;
      }

      void drainReadyPackets(SocketStats& stats) {
         while (true) {
            auto& bufferedPacket = receiveBuffer_[slotIndex(expectedRecvSeq_, receiveBuffer_.size())];
            if (!bufferedPacket || bufferedPacket->sequenceNumber != expectedRecvSeq_) {
               return;
            }

            recordRecv(bufferedPacket->payload.size(), stats);
            deliverOrQueue(queuedMessages_, pendingRecv_, bufferedPacket->payload);
            bufferedPacket.reset();
            ++expectedRecvSeq_;
         }
      }

      [[nodiscard]] bool storeSentPacket(std::uint16_t sequenceNumber, std::vector<std::byte> const& encodedPacket) {
         auto& sentPacket = sentPackets_[slotIndex(sequenceNumber, sentPackets_.size())];
         if (sentPacket) {
            return false;
         }

         sentPacket = SentPacket{sequenceNumber, encodedPacket};
         ++inFlightCount_;
         return true;
      }

      bool eraseSentPacket(std::uint16_t sequenceNumber) {
         auto& sentPacket = sentPackets_[slotIndex(sequenceNumber, sentPackets_.size())];
         if (!sentPacket || sentPacket->sequenceNumber != sequenceNumber) {
            return false;
         }

         sentPacket.reset();
         --inFlightCount_;
         return true;
      }

      static void recordRecv(Packet const& packet, SocketStats& stats) {
         recordRecv(packet.payload_.size(), stats);
      }

      static void recordRecv(std::size_t payloadSize, SocketStats& stats) {
         stats.packetsRecv_.fetch_add(1, std::memory_order_relaxed);
         stats.bytesRecv_.fetch_add(payloadSize, std::memory_order_relaxed);
      }

      std::uint8_t id_ = 0;
      std::uint16_t sendWindow_ = kDefaultWindowSize;
      std::uint16_t nextSeq_ = 0;
      std::uint16_t expectedRecvSeq_ = 0;
      std::vector<std::optional<SentPacket>> sentPackets_{windowCapacity(kDefaultWindowSize)};
      std::size_t inFlightCount_ = 0;
      std::vector<std::optional<BufferedPacket>> receiveBuffer_{windowCapacity(kDefaultWindowSize)};
      ReceiveAckTracker recvAckTracker_;
      std::deque<ReceivedMessage> queuedMessages_;
      std::optional<PendingRecv> pendingRecv_;
      [[no_unique_address]] CongestionController cc_;
   };

} // namespace rude::detail::channel

#endif // RUDE_DETAIL_CHANNEL_RELIABLECHANNEL_HPP
