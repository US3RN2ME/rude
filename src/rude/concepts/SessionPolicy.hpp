
#ifndef RUDE_CONCEPTS_SESSIONPOLICY_HPP
#define RUDE_CONCEPTS_SESSIONPOLICY_HPP

#include <concepts>
#include <cstdint>
#include <system_error>

namespace rude {

   class SessionBase;

   template <typename T>
   concept SessionPolicy = requires(T policy, SessionBase& session, std::error_code ec, std::uint8_t channelId) {
      { policy.onConnected(session) } noexcept -> std::same_as<void>;
      { policy.onDisconnected(ec) } noexcept -> std::same_as<void>;
      { policy.onKeepaliveTimeout() } noexcept -> std::same_as<void>;
      { policy.onChannelStalled(channelId) } noexcept -> std::same_as<void>;
   };

   struct DefaultPolicy {
      void onConnected(SessionBase&) noexcept {}
      void onDisconnected(std::error_code) noexcept {}
      void onKeepaliveTimeout() noexcept {}
      void onChannelStalled(std::uint8_t) noexcept {}
   };

   static_assert(SessionPolicy<DefaultPolicy>);

} // namespace rude

#endif // RUDE_CONCEPTS_SESSIONPOLICY_HPP
