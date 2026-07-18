// Full connect / send / receive / disconnect round trip in one process.
//
// A server accepts one client and echoes everything it receives on the
// reliable channel. The client sends a few messages, waits for the echoes,
// prints session statistics, and disconnects. This is the complete public
// API surface most applications need:
//
//   Acceptor::asyncAccept  -> shared_ptr<Session>
//   Connector::asyncConnect -> shared_ptr<Session>
//   Session::asyncSend / asyncRecv / asyncDisconnect

#include <array>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/system/system_error.hpp>
#include <cstddef>
#include <iostream>
#include <rude/rude.hpp>
#include <span>
#include <string>

namespace {
   namespace asio = boost::asio;
   using asio::use_awaitable;

   constexpr std::uint8_t kReliableChannel = 0;
   constexpr std::uint8_t kUnreliableChannel = 1;

   rude::SessionConfig makeConfig() {
      rude::SessionConfig config;
      config.channels_ = {
          rude::ChannelConfig::reliableOrdered(kReliableChannel),
          rude::ChannelConfig::unreliableSequenced(kUnreliableChannel),
      };
      return config;
   }

   std::span<std::byte const> asBytes(std::string const& text) {
      return {reinterpret_cast<std::byte const*>(text.data()), text.size()};
   }

   asio::awaitable<void> runServer(rude::Acceptor& acceptor) {
      auto session = co_await acceptor.asyncAccept(use_awaitable);
      std::cout << "[server] accepted " << session->remoteEndpoint() << '\n';

      std::array<std::byte, 2048> buf{};
      try {
         for (;;) {
            const auto n = co_await session->asyncRecv(kReliableChannel, asio::buffer(buf), use_awaitable);
            co_await session->asyncSend(kReliableChannel, std::span<std::byte const>{buf.data(), n}, use_awaitable);
         }
      } catch (boost::system::system_error const& error) {
         std::cout << "[server] session ended: " << error.code().message() << '\n';
      }
   }

   asio::awaitable<void> runClient(rude::Connector& connector, boost::asio::ip::udp::endpoint server) {
      auto session = co_await connector.asyncConnect(server, use_awaitable);
      std::cout << "[client] connected to " << session->remoteEndpoint() << '\n';

      std::array<std::byte, 2048> buf{};
      for (int i = 0; i < 3; ++i) {
         const auto message = "hello #" + std::to_string(i);
         co_await session->asyncSend(kReliableChannel, asBytes(message), use_awaitable);
         const auto n = co_await session->asyncRecv(kReliableChannel, asio::buffer(buf), use_awaitable);
         std::cout << "[client] echo: " << std::string{reinterpret_cast<char const*>(buf.data()), n} << '\n';
      }

      // Fire-and-forget telemetry on the unreliable channel: no retransmits,
      // no ordering guarantees, no waiting.
      const std::string telemetry = "position update";
      co_await session->asyncSend(kUnreliableChannel, asBytes(telemetry), use_awaitable);

      const auto stats = session->stats();
      std::cout << "[client] sent " << stats.packetsSent << " packets, received " << stats.packetsRecv << ", avg rtt "
                << stats.avgRttUs << " us\n";

      co_await session->asyncDisconnect(use_awaitable);
      std::cout << "[client] disconnected\n";
   }
} // namespace

int main() {
   asio::io_context io;

   rude::Acceptor acceptor{io.get_executor(), {asio::ip::make_address("127.0.0.1"), 0}};
   acceptor.withConfig(makeConfig());

   rude::Connector connector{io.get_executor()};
   connector.withConfig(makeConfig());

   asio::co_spawn(io, runServer(acceptor), asio::detached);
   asio::co_spawn(io, runClient(connector, acceptor.localEndpoint()), asio::detached);

   io.run();
   return 0;
}
