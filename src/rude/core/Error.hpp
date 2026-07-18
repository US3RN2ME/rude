
#ifndef RUDE_CORE_ERROR_HPP
#define RUDE_CORE_ERROR_HPP

#include <boost/system/error_category.hpp>
#include <boost/system/error_code.hpp>
#include <string>
#include <type_traits>

namespace rude {

   /**
    * @brief Library-specific error conditions.
    *
    * Values are convertible to std::error_code through
    * makeErrorCode() and the
    * std::is_error_code_enum specialization at the end of this header.
    */
   enum class Error {
      ConnectionReset = 1, ///< Remote peer sent DISCONNECT or vanished
      Timeout = 2,         ///< Keepalive or handshake deadline exceeded
      BadPacket = 3,       ///< CRC mismatch or malformed header
      SendWindowFull = 4,  ///< Reliable channel send window exhausted
      ChannelClosed = 5,   ///< Operation on a channel that has been shut down
      VersionMismatch = 6, ///< Peer's codec version != our version()
      MessageTooLarge = 7, ///< Payload + codec overhead exceeds the session MTU
   };

   /**
    * @brief Error category implementation for rude errors.
    *
    * Codes are boost::system::error_code so they slot natively into Boost.Asio
    * completion signatures (co_await unwrapping, exceptions); they convert
    * implicitly to std::error_code wherever the standard type is expected.
    * Users normally do not construct this type directly; call rudpCategory()
    * or makeErrorCode() instead.
    */
   struct RudpCategory : boost::system::error_category {
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
            case Error::MessageTooLarge:
               return "message exceeds session mtu";
            default:
               return "unknown rude error";
         }
      }
   };

   /**
    * @brief Returns the singleton error category used by rude.
    *
    * @return Stable process-local error category
    * instance.
    */
   inline const boost::system::error_category& rudpCategory() noexcept {
      static RudpCategory instance;
      return instance;
   }

   /**
    * @brief Converts a rude error enum to an error code.
    *
    * @param e
    * Library error condition.
    *
    * @return boost::system::error_code bound to the rude category.
    */
   inline boost::system::error_code makeErrorCode(Error e) noexcept {
      return {static_cast<int>(e), rudpCategory()};
   }

   /// Enables implicit conversion of rude::Error to boost::system::error_code.
   inline boost::system::error_code make_error_code(Error e) noexcept {
      return makeErrorCode(e);
   }

} // namespace rude

template <>
struct boost::system::is_error_code_enum<rude::Error> : std::true_type {};

#endif // RUDE_CORE_ERROR_HPP
