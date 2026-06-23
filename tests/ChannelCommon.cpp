#include <array>
#include <boost/asio/buffer.hpp>
#include <cstddef>
#include <deque>
#include <optional>
#include <rude/detail/channel/ChannelCommon.hpp>
#include <string>

#include "Helpers.hpp"
#include "ut_main.hpp"

namespace {
   suite<"[ChannelCommon]"> _ = [] {
      "[TracksAckBits]"_test = [] {
         rude::detail::channel::ReceiveAckTracker tracker;
         tracker.markReceived(10);
         tracker.markReceived(8);
         tracker.markReceived(11);

         expect(tracker.hasReceived());
         expect(eq(tracker.latestSequence(), std::uint16_t{11}));
         expect(rude::detail::channel::isAcknowledged(11, tracker.latestSequence(), tracker.ackBits()));
         expect(rude::detail::channel::isAcknowledged(10, tracker.latestSequence(), tracker.ackBits()));
         expect(rude::detail::channel::isAcknowledged(8, tracker.latestSequence(), tracker.ackBits()));
         expect(!rude::detail::channel::isAcknowledged(7, tracker.latestSequence(), tracker.ackBits()));
      };

      "[CompletesPendingReceive]"_test = [] {
         std::deque<rude::detail::channel::ReceivedMessage> queued;
         std::optional<rude::detail::channel::PendingRecv> pending;
         std::array<std::byte, 8> buffer{};
         std::size_t receivedSize = 0;

         rude::detail::channel::asyncRecv(queued, pending, boost::asio::buffer(buffer),
                                          [&](std::error_code ec, std::size_t size) {
                                             expect(!ec);
                                             receivedSize = size;
                                          });
         auto payload = rude::test::bytes("abc");
         rude::detail::channel::deliverOrQueue(queued, pending, payload);

         expect(eq(receivedSize, std::size_t{3}));
         expect(eq(std::string{reinterpret_cast<char const*>(buffer.data()), receivedSize}, std::string{"abc"}));
         expect(!pending.has_value());
         expect(queued.empty());
      };
   };
} // namespace
