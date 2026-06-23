
#ifndef RUDE_CORE_ERROR_HPP
#define RUDE_CORE_ERROR_HPP

#include <system_error>

namespace rude {

   enum class Error {
      ConnectionReset = 1, ///< Remote peer sent DISCONNECT or vanished
      Timeout = 2,         ///< Keepalive or handshake deadline exceeded
      BadPacket = 3,       ///< CRC mismatch or malformed header
      SendWindowFull = 4,  ///< Reliable channel send window exhausted
      ChannelClosed = 5,   ///< Operation on a channel that has been shut down
      VersionMismatch = 6, ///< Peer's codec version != our version()
   };

   struct RudpCategory : std::error_category {
      [[nodiscard]] const char* name() const noexcept override {
         return "rude";
      }

      [[nodiscard]] std::string message(int ev) const override {
         switch (static_cast<Error>(ev)) {
            case Error::ConnectionReset:
               return "connection reset by peer";
            case Error::Timeout:
               return "operation timed out";
            case Error::BadPacket:
               return "bad packet (crc or header)";
            case Error::SendWindowFull:
               return "send window full";
            case Error::ChannelClosed:
               return "channel closed";
            case Error::VersionMismatch:
               return "codec version mismatch";
            default:
               return "unknown rude error";
         }
      }
   };

   inline const std::error_category& rudpCategory() noexcept {
      static RudpCategory instance;
      return instance;
   }

   inline std::error_code makeErrorCode(Error e) noexcept {
      return {static_cast<int>(e), rudpCategory()};
   }

} // namespace rude

template <>
struct std::is_error_code_enum<rude::Error> : std::true_type {};

#endif // RUDE_CORE_ERROR_HPP
