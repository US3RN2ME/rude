#include <iostream>
#include <rude/core/Error.hpp>
#include <rude/session/SocketStats.hpp>

int main() {
   auto ec = rude::makeErrorCode(rude::Error::SendWindowFull);
   std::cout << "error category: " << ec.category().name() << '\n';
   std::cout << "error message: " << ec.message() << '\n';

   rude::SocketStats stats;
   stats.packetsSent_.store(12);
   stats.packetsRecv_.store(10);
   stats.bytesSent_.store(4096);
   stats.bytesRecv_.store(2048);
   stats.sendWindowUsed_.store(3);

   auto snapshot = stats.snapshot();
   std::cout << "sent packets: " << snapshot.packetsSent << '\n';
   std::cout << "received packets: " << snapshot.packetsRecv << '\n';
   std::cout << "send window used: " << snapshot.sendWindowUsed << '\n';
}
