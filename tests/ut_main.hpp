
#ifndef RUDE_TEST_UT_MAIN_HPP
#define RUDE_TEST_UT_MAIN_HPP

#include <boost/ut.hpp>

using namespace boost::ut;

int main() {
   return cfg<override>.run();
}

#endif // RUDE_TEST_UT_MAIN_HPP
