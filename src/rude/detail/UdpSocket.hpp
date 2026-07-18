
#ifndef RUDE_DETAIL_UDPSOCKET_HPP
#define RUDE_DETAIL_UDPSOCKET_HPP

#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/system/error_code.hpp>
#include <rude/core/Executor.hpp>
#include <utility>

namespace rude {

   /// Concrete Socket implementation wrapping asio::ip::udp::socket.
   ///
   /// The socket runs on whatever executor it is constructed with. BasicSession
   /// passes its strand, so every completion handler is serialized with the rest
   /// of the session state without any extra synchronization here.
   class UdpSocket {
   public:
      explicit UdpSocket(Executor executor, boost::asio::ip::udp::endpoint localEp)
          : sock_{std::move(executor), localEp} {}

      template <typename Token>
      auto asyncSendTo(boost::asio::const_buffer buf, boost::asio::ip::udp::endpoint remote, Token&& token) {
         return sock_.async_send_to(buf, remote, std::forward<Token>(token));
      }

      template <typename Token>
      auto asyncRecvFrom(boost::asio::mutable_buffer buf, boost::asio::ip::udp::endpoint& sender, Token&& token) {
         return sock_.async_receive_from(buf, sender, std::forward<Token>(token));
      }

      [[nodiscard]] boost::asio::ip::udp::endpoint localEndpoint() const {
         boost::system::error_code ec;
         return sock_.local_endpoint(ec);
      }

      void cancel() {
         boost::system::error_code ec;
         sock_.cancel(ec);
      }

      void close() {
         boost::system::error_code ec;
         sock_.close(ec);
      }

   private:
      boost::asio::ip::udp::socket sock_;
   };

} // namespace rude

#endif // RUDE_DETAIL_UDPSOCKET_HPP
