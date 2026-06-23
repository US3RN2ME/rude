#ifndef RUDE_DETAIL_CHANNEL_RELIABLEUNORDEREDCHANNEL_HPP
#define RUDE_DETAIL_CHANNEL_RELIABLEUNORDEREDCHANNEL_HPP

#include <rude/detail/channel/ReliableChannel.hpp>

namespace rude {

   template <typename CongestionController>
   class ReliableUnorderedChannel : public detail::channel::ReliableChannel<CongestionController, false> {
   public:
      using detail::channel::ReliableChannel<CongestionController, false>::ReliableChannel;
   };

} // namespace rude

#endif // RUDE_DETAIL_CHANNEL_RELIABLEUNORDEREDCHANNEL_HPP
