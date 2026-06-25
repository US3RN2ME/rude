#ifndef RUDE_DETAIL_CHANNEL_RELIABLEUNORDEREDCHANNEL_HPP
#define RUDE_DETAIL_CHANNEL_RELIABLEUNORDEREDCHANNEL_HPP

#include <rude/detail/channel/ReliableChannel.hpp>

namespace rude {

   /**
    * @brief Reliable channel that suppresses duplicates without ordering.
    *
    * Packets are retransmitted until acknowledged, but the receive side
    * delivers each sequence number as soon as it arrives instead of waiting for
    * lower sequence numbers.
    *
    * @tparam CongestionController
    * Congestion controller used to size the reliable send window.
    */
   template <typename CongestionController>
   class ReliableUnorderedChannel : public detail::channel::ReliableChannel<CongestionController, false> {
   public:
      using detail::channel::ReliableChannel<CongestionController, false>::ReliableChannel;
   };

} // namespace rude

#endif // RUDE_DETAIL_CHANNEL_RELIABLEUNORDEREDCHANNEL_HPP
