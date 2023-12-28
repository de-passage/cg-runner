#include "runner.hpp"

runner make_runner(const option_t &opts) {
  runner r{};
  r.cmd_args[runner::Player1] = opts.p1.data();
  r.cmd_args[runner::Player2] = opts.p2.data();
  r.cmd_args[runner::Referee] = opts.referee.data();
  if (opts.generate_output) {
    r.cmd_args[runner::GenerateFlag] = "-l";
  }

  return r;
}

dpsg::posix::process_t runner::operator()(std::string_view output_file) {
  if (cmd_args[GenerateFlag] != nullptr) {
    cmd_args[OutputName] = output_file.data();
  }
  return dpsg::posix::run_external("java", cmd_args);
}
