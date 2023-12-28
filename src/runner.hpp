#ifndef HEADER_GUARD_DPSG_RUNNER_HPP
#define HEADER_GUARD_DPSG_RUNNER_HPP

#include "options.hpp"
#include <string_view>

struct runner {

  enum POSITIONS {
    Referee = 2,
    Player1 = 4,
    Player2 = 6,
    GenerateFlag = 7,
    OutputName = 8,
  };

  const char *cmd_args[12] = {
      "java",  // 0
      "-jar",  // 1
      nullptr, // 2
      "-p1",   // 3
      nullptr, // 4
      "-p2",   // 5
      nullptr, // 6
      nullptr, // 7
      nullptr, // 8
      nullptr, // 9
      nullptr, // 10
      nullptr, // 11
  };

  dpsg::posix::process_t operator()(std::string_view output_file);
};

runner make_runner(const option_t &opts);

#endif // HEADER_GUARD_DPSG_RUNNER_HPP
