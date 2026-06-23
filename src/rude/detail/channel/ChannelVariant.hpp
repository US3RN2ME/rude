#ifndef RUDE_DETAIL_CHANNEL_CHANNELVARIANT_HPP
#define RUDE_DETAIL_CHANNEL_CHANNELVARIANT_HPP

#include <rude/detail/channel/ReliableOrderedChannel.hpp>
#include <rude/detail/channel/ReliableUnorderedChannel.hpp>
#include <rude/detail/channel/UnreliableChannel.hpp>
#include <variant>

namespace rude {

   template <typename OrderedCongestionController, typename UnorderedCongestionController>
   using ChannelVariant = std::variant<UnreliableChannel, ReliableOrderedChannel<OrderedCongestionController>,
                                       ReliableUnorderedChannel<UnorderedCongestionController>>;

} // namespace rude

#endif // RUDE_DETAIL_CHANNEL_CHANNELVARIANT_HPP
