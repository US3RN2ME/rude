#ifndef RUDE_DETAIL_CHANNEL_RELIABLEORDEREDCHANNEL_HPP
#define RUDE_DETAIL_CHANNEL_RELIABLEORDEREDCHANNEL_HPP

#include <rude/detail/channel/ReliableChannel.hpp>

namespace rude {

   template <typename CongestionController>
   class ReliableOrderedChannel : public detail::channel::ReliableChannel<CongestionController, true> {
   public:
      using detail::channel::ReliableChannel<CongestionController, true>::ReliableChannel;
   };

} // namespace rude

#endif // RUDE_DETAIL_CHANNEL_RELIABLEORDEREDCHANNEL_HPP
