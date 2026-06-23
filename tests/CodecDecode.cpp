#include <boost/asio/buffer.hpp>
#include <cstddef>
#include <cstdint>
#include <rude/detail/DefaultCodec.hpp>
#include <vector>

#include "ut_main.hpp"

namespace {
   suite<"[Fuzz.DefaultCodecDecode]"> _ = [] {
      "[RejectsArbitraryShortInputs]"_test = [] {
         rude::DefaultCodec codec;

         for (std::size_t size = 0; size < rude::DefaultCodec::kOverhead; ++size) {
            std::vector<std::byte> input(size, static_cast<std::byte>(0xA5));
            auto decoded = codec.decode(boost::asio::buffer(input));
            expect(!decoded.has_value());
         }
      };

      "[HandlesDeterministicBytePatterns]"_test = [] {
         rude::DefaultCodec codec;

         for (std::uint8_t seed = 0; seed < 32; ++seed) {
            std::vector<std::byte> input(64);
            for (std::size_t i = 0; i < input.size(); ++i) {
               input[i] = static_cast<std::byte>(seed + static_cast<std::uint8_t>(i * 17));
            }

            auto decoded = codec.decode(boost::asio::buffer(input));
            expect(!decoded.has_value());
         }
      };
   };
} // namespace
