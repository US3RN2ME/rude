#include <boost/asio.hpp>
#include <iostream>
#include <rude/rude.hpp>

int main() {
   boost::asio::io_context io_context;

   boost::asio::post(io_context, []() {
      std::cout << "Hello, World!" << std::endl;
   });
   io_context.run();

   return 0;
}
