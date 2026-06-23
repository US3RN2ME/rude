
#ifndef RUDE_DETAIL_UDPSOCKET_HPP
#define RUDE_DETAIL_UDPSOCKET_HPP

#include <array>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/udp.hpp>
#include <cstddef>
#include <rude/core/Executor.hpp>
#include <system_error>

namespace rude {

   /// Concrete Socket implementation wrapping asio::ip::udp::socket.
   ///
   /// All async operations are dispatched through strand_ so the socket
   /// is safe to use from multiple threads. A pre-allocated rxBuf_ avoids
   /// per-receive heap allocation on the hot path.
   class UdpSocket {
   public:
      static constexpr std::size_t kRecvBufSize = 65'536;

      UdpSocket() = default;

      explicit UdpSocket(Executor executor, boost::asio::ip::udp::endpoint localEp)
          : strand_{std::move(executor)}
          , sock_{strand_.get_inner_executor(), localEp} {}

      template <typename Token>
      auto asyncSendTo(boost::asio::const_buffer buf, boost::asio::ip::udp::endpoint remote, Token&& token) {
         return sock_.async_send_to(buf, remote, boost::asio::bind_executor(strand_, std::forward<Token>(token)));
      }

      template <typename Token>
      auto asyncRecvFrom(boost::asio::mutable_buffer buf, boost::asio::ip::udp::endpoint& sender, Token&& token) {
         return sock_.async_receive_from(buf, sender, boost::asio::bind_executor(strand_, std::forward<Token>(token)));
      }

      /// Convenience: receive into the internal pre-allocated buffer.
      template <typename Token>
      auto asyncRecv(boost::asio::ip::udp::endpoint& sender, Token&& token) {
         return asyncRecvFrom(boost::asio::buffer(rxBuf_), sender, std::forward<Token>(token));
      }

      /// View of the internal receive buffer (valid until next asyncRecv call).
      [[nodiscard]] boost::asio::const_buffer rxBuffer(std::size_t n) const noexcept {
         return boost::asio::buffer(rxBuf_.data(), n);
      }

      [[nodiscard]] boost::asio::ip::udp::endpoint localEndpoint() const {
         return sock_.local_endpoint();
      }

      void cancel() {
         sock_.cancel();
      }
      void close() {
         sock_.close();
      }

      [[nodiscard]] Strand& strand() noexcept {
         return strand_;
      }

   private:
      Strand strand_;
      boost::asio::ip::udp::socket sock_;
      std::array<std::byte, kRecvBufSize> rxBuf_{};
   };

} // namespace rude

#endif // RUDE_DETAIL_UDPSOCKET_HPP
