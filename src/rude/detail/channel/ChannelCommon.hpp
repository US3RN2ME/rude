#ifndef RUDE_DETAIL_CHANNEL_CHANNELCOMMON_HPP
#define RUDE_DETAIL_CHANNEL_CHANNELCOMMON_HPP

#include <algorithm>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/system/error_code.hpp>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <rude/core/Error.hpp>
#include <rude/protocol/Packet.hpp>
#include <span>
#include <utility>
#include <vector>

namespace rude::detail::channel {
   /// Stored receive completion. move_only_function so move-only Asio handlers
   /// (use_awaitable, deferred) can be parked until a message arrives.
   using RecvHandler = std::move_only_function<void(boost::system::error_code, std::size_t)>;

   struct PendingRecv {
      boost::asio::mutable_buffer buf;
      RecvHandler handler;
   };

   struct ReceivedMessage {
      std::vector<std::byte> payload;
   };

   inline void completeRecv(PendingRecv& pendingRecv, ReceivedMessage const& message) {
      const auto size = std::min(message.payload.size(), pendingRecv.buf.size());
      std::memcpy(pendingRecv.buf.data(), message.payload.data(), size);
      pendingRecv.handler({}, size);
   }

   inline void deliverOrQueue(std::deque<ReceivedMessage>& queuedMessages, std::optional<PendingRecv>& pendingRecv,
                              std::span<std::byte const> payload) {
      ReceivedMessage message;
      message.payload.assign(payload.begin(), payload.end());

      if (!pendingRecv) {
         queuedMessages.push_back(std::move(message));
         return;
      }

      auto recv = std::exchange(pendingRecv, std::nullopt);
      completeRecv(*recv, message);
   }

   inline void asyncRecv(std::deque<ReceivedMessage>& queuedMessages, std::optional<PendingRecv>& pendingRecv,
                         boost::asio::mutable_buffer buf, RecvHandler handler) {
      if (!queuedMessages.empty()) {
         auto message = std::move(queuedMessages.front());
         queuedMessages.pop_front();
         PendingRecv recv{buf, std::move(handler)};
         completeRecv(recv, message);
         return;
      }

      if (pendingRecv) {
         handler(makeErrorCode(Error::ChannelClosed), 0);
         return;
      }

      pendingRecv = PendingRecv{buf, std::move(handler)};
   }

   /**
    * @brief Per-channel receive queue.
    *
    * Buffers delivered messages, parks at most one pending receive, and after
    * close() completes every receive immediately with the close reason.
    */
   class RecvQueue {
   public:
      void asyncRecv(boost::asio::mutable_buffer buf, RecvHandler handler) {
         if (queuedMessages_.empty() && closed_) {
            handler(closeReason_, 0);
            return;
         }

         channel::asyncRecv(queuedMessages_, pendingRecv_, buf, std::move(handler));
      }

      void deliver(std::span<std::byte const> payload) {
         deliverOrQueue(queuedMessages_, pendingRecv_, payload);
      }

      /// Fail the parked receive (if any) and make future receives fail once
      /// buffered messages are drained.
      void close(boost::system::error_code reason) {
         closed_ = true;
         closeReason_ = reason;
         if (pendingRecv_) {
            auto recv = std::exchange(pendingRecv_, std::nullopt);
            recv->handler(reason, 0);
         }
      }

      [[nodiscard]] bool closed() const noexcept {
         return closed_;
      }

   private:
      std::deque<ReceivedMessage> queuedMessages_;
      std::optional<PendingRecv> pendingRecv_;
      bool closed_ = false;
      boost::system::error_code closeReason_;
   };

   inline bool isNewerSequence(std::uint16_t sequenceNumber, std::uint16_t previousSequenceNumber) noexcept {
      const auto delta = static_cast<std::uint16_t>(sequenceNumber - previousSequenceNumber);
      return delta != 0 && delta < 32768U;
   }

   inline bool isStaleOrDuplicate(std::uint16_t sequenceNumber, std::uint16_t previousSequenceNumber) noexcept {
      return !isNewerSequence(sequenceNumber, previousSequenceNumber);
   }

   class ReceiveAckTracker {
   public:
      void markReceived(std::uint16_t sequenceNumber) noexcept {
         if (!hasReceived_) {
            hasReceived_ = true;
            latestSequence_ = sequenceNumber;
            ackBits_ = 0;
            return;
         }

         const auto forwardDelta = static_cast<std::uint16_t>(sequenceNumber - latestSequence_);
         if (forwardDelta == 0) {
            return;
         }

         if (forwardDelta < 32768U) {
            if (forwardDelta <= 32U) {
               ackBits_ = (ackBits_ << forwardDelta) | (1U << (forwardDelta - 1U));
            } else {
               ackBits_ = 0;
            }
            latestSequence_ = sequenceNumber;
            return;
         }

         const auto backDelta = static_cast<std::uint16_t>(latestSequence_ - sequenceNumber);
         if (backDelta >= 1U && backDelta <= 32U) {
            ackBits_ |= 1U << (backDelta - 1U);
         }
      }

      [[nodiscard]] bool hasReceived() const noexcept {
         return hasReceived_;
      }
      [[nodiscard]] std::uint16_t latestSequence() const noexcept {
         return latestSequence_;
      }
      [[nodiscard]] std::uint32_t ackBits() const noexcept {
         return ackBits_;
      }

   private:
      bool hasReceived_ = false;
      std::uint16_t latestSequence_ = 0;
      std::uint32_t ackBits_ = 0;
   };

   inline bool isAcknowledged(std::uint16_t sequenceNumber, std::uint16_t ack, std::uint32_t ackBits) noexcept {
      if (sequenceNumber == ack) {
         return true;
      }

      const auto backDelta = static_cast<std::uint16_t>(ack - sequenceNumber);
      if (backDelta == 0 || backDelta > 32U || backDelta >= 32768U) {
         return false;
      }

      return (ackBits & (1U << (backDelta - 1U))) != 0;
   }

   template <typename Sock, typename Handler>
   void sendEncoded(Sock& sock, boost::asio::ip::udp::endpoint const& remoteEndpoint, std::vector<std::byte> encodedPacket,
                    Handler handler) {
      auto buffer = std::make_shared<std::vector<std::byte>>(std::move(encodedPacket));
      sock.asyncSendTo(boost::asio::buffer(*buffer), remoteEndpoint,
                       [buffer, handler = std::move(handler)](boost::system::error_code ec, std::size_t) mutable {
                          handler(ec);
                       });
   }
} // namespace rude::detail::channel

#endif // RUDE_DETAIL_CHANNEL_CHANNELCOMMON_HPP
