#ifndef HEADER_GUARD_DPSG_OPTIONS_HPP
#define HEADER_GUARD_DPSG_OPTIONS_HPP

#include "cli.hpp"
#include "integer_result.hpp"

#include <string_view>
#include <iostream>

struct option_t {
  int process_count = 20;
  int parallel_processes = 4;
  bool generate_output = true;
  std::string_view p1 = "";
  std::string_view p2 = "";
  std::string_view referee = "";
  bool debug = false;
};

template <class T, class E, class... Args>
T unwrap(dpsg::integer_result<T, E> r, Args &&...msg) {
  if (r.is_error()) {
    if constexpr (sizeof...(msg) > 0) {
      ((std::cerr << msg), ...);
      std::cerr << std::endl;
    }
    exit(1);
  }
  return r.value();
}

option_t parse_options(int argc, const char **argv);

#endif // HEADER_GUARD_DPSG_OPTIONS_HPP
