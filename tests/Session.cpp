#include <array>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/system/system_error.hpp>
#include <chrono>
#include <cstddef>
#include <rude/rude.hpp>
#include <span>
#include <string>

#include "ut_main.hpp"

namespace {
   namespace asio = boost::asio;
   using asio::use_awaitable;

   constexpr int kMessageCount = 5;

   std::span<std::byte const> asBytes(std::string const& text) {
      return {reinterpret_cast<std::byte const*>(text.data()), text.size()};
   }

   std::string asText(std::span<std::byte const> payload) {
      return {reinterpret_cast<char const*>(payload.data()), payload.size()};
   }

   suite<"[Session]"> _ = [] {
      "[LoopbackEchoAndDisconnect]"_test = [] {
         asio::io_context io;

         rude::Acceptor acceptor{io.get_executor(), {asio::ip::make_address("127.0.0.1"), 0}};
         rude::Connector connector{io.get_executor()};

         bool serverSawDisconnect = false;
         bool clientFinished = false;
         int echoes = 0;

         // Server: accept one client on channel 0 and echo everything back.
         asio::co_spawn(
             io,
             [&]() -> asio::awaitable<void> {
                auto session = co_await acceptor.asyncAccept(use_awaitable);
                std::array<std::byte, 2048> buf{};
                try {
                   for (;;) {
                      const auto n = co_await session->asyncRecv(0, asio::buffer(buf), use_awaitable);
                      co_await session->asyncSend(0, std::span<std::byte const>{buf.data(), n}, use_awaitable);
                   }
                } catch (boost::system::system_error const& error) {
                   serverSawDisconnect = error.code() == rude::makeErrorCode(rude::Error::ConnectionReset);
                }
             },
             asio::detached);

         // Client: connect, exchange echoes in order, then disconnect.
         asio::co_spawn(
             io,
             [&]() -> asio::awaitable<void> {
                auto session = co_await connector.asyncConnect(acceptor.localEndpoint(), use_awaitable);
                std::array<std::byte, 2048> buf{};

                for (int i = 0; i < kMessageCount; ++i) {
                   const auto message = "message-" + std::to_string(i);
                   co_await session->asyncSend(0, asBytes(message), use_awaitable);
                   const auto n = co_await session->asyncRecv(0, asio::buffer(buf), use_awaitable);
                   expect(eq(asText({buf.data(), n}), message));
                   ++echoes;
                }

                // Payloads above the MTU are rejected up front.
                const std::string oversized(4096, 'x');
                boost::system::error_code sendError;
                try {
                   co_await session->asyncSend(0, asBytes(oversized), use_awaitable);
                } catch (boost::system::system_error const& error) {
                   sendError = error.code();
                }
                expect(eq(sendError, rude::makeErrorCode(rude::Error::MessageTooLarge)));

                const auto snapshot = session->stats();
                expect(snapshot.packetsSent >= static_cast<std::uint64_t>(kMessageCount));
                expect(snapshot.packetsRecv >= static_cast<std::uint64_t>(kMessageCount));

                co_await session->asyncDisconnect(use_awaitable);
                clientFinished = true;
             },
             asio::detached);

         io.run_for(std::chrono::seconds{10});

         expect(eq(echoes, kMessageCount));
         expect(clientFinished);
         expect(serverSawDisconnect);
      };

      "[ConnectTimesOutWithoutServer]"_test = [] {
         asio::io_context io;

         rude::SessionConfig config;
         config.maxHandshakeMs_ = 300;

         rude::Connector connector{io.get_executor()};
         connector.withConfig(config);

         boost::system::error_code connectError;
         bool completed = false;

         asio::co_spawn(
             io,
             [&]() -> asio::awaitable<void> {
                try {
                   // Nobody listens on this port; the handshake must fail
                   // instead of hanging forever.
                   auto session = co_await connector.asyncConnect({asio::ip::make_address("127.0.0.1"), 1}, use_awaitable);
                   static_cast<void>(session);
                } catch (boost::system::system_error const& error) {
                   connectError = error.code();
                }
                completed = true;
             },
             asio::detached);

         io.run_for(std::chrono::seconds{10});

         expect(completed);
         expect(static_cast<bool>(connectError));
      };
   };
} // namespace
