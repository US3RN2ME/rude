
#ifndef RUDE_CORE_EXECUTOR_HPP
#define RUDE_CORE_EXECUTOR_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/strand.hpp>

namespace rude {
   using Executor = boost::asio::any_io_executor;
   using Strand = boost::asio::strand<Executor>;
} // namespace rude

#endif // RUDE_CORE_EXECUTOR_HPP
