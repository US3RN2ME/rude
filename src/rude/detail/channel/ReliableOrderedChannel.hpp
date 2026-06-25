#ifndef RUDE_DETAIL_CHANNEL_RELIABLEORDEREDCHANNEL_HPP
#define RUDE_DETAIL_CHANNEL_RELIABLEORDEREDCHANNEL_HPP

#include <rude/detail/channel/ReliableChannel.hpp>

namespace rude {

   /**
    * @brief Reliable channel that preserves receive order.
    *
    * Incoming packets may arrive out of order. The channel buffers newer
    * sequence numbers and only completes receives when all preceding reliable
    * packets have been delivered.
    *
    * @tparam CongestionController
    * Congestion controller used to size the reliable send window.
    */
   template <typename CongestionController>
   class ReliableOrderedChannel : public detail::channel::ReliableChannel<CongestionController, true> {
   public:
      using detail::channel::ReliableChannel<CongestionController, true>::ReliableChannel;
   };

} // namespace rude

#endif // RUDE_DETAIL_CHANNEL_RELIABLEORDEREDCHANNEL_HPP
