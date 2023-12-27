#include "posix.hpp"
#include "vt100.hpp"
#include "cli.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <span>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <vector>

namespace dpsg {
template <class T>
struct std::basic_ostream<T> &operator<<(struct std::basic_ostream<T> &out,
                                         pid_t p) {
  return out << (int)p;
}
} // namespace dpsg

auto run() {
  const char *params[] = {
      "java",
      "-jar",
      "/home/depassage/workspace/codingame-fall2023/ais/referee.jar",
      "-p1",
      "/home/depassage/workspace/codingame-fall2023/ais/basic",
      "-p2",
      "/home/depassage/workspace/codingame-fall2023/ais/basic-hunter",
      "-l",
      "output.json",
      (char *)NULL};
  return dpsg::run_external(params[0], params);
}

struct process_pool {
  std::vector<pid_t> pids;
  std::vector<dpsg::fd_t> ins;
  std::vector<dpsg::fd_t> outs;
};

struct option_t {
  int process_count = 1;
  int parallel_processes = 1;
  bool generate_output = true;
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

option_t parse_options(int argc, const char **argv) {

  enum { expect_option, expect_value } expectation = expect_option;

  enum class curopt : char {
    none = 0,
    count = 'c',
    generate_output = 'G',
    parallel_processes = 'p'
  } current_option = curopt::none;

  option_t options;

  for (int i = 1; i < argc; ++i) {
    std::string_view arg{argv[i]};
    switch (expectation) {
    case expect_option: {
      if (arg.size() < 2 || arg[0] != '-') {
        std::cerr << "Expected option, got '" << arg << "'" << std::endl;
        exit(1);
      }

      for (size_t c = 1; c < arg.size(); ++c) {
        char cur = arg[c];
        switch ((curopt)cur) {
        case curopt::count: {
          if (c < arg.size() - 1) {
            options.process_count =
                unwrap(dpsg::cli::parse_unsigned_int(arg.substr(c + 1)),
                       "Invalid run count ", arg.substr(c + 1));
          } else {
            current_option = curopt::count;
          }
          goto done_with_short_options;
        }
        case curopt::generate_output: {
          options.generate_output = false;
          break;
        }
        case curopt::parallel_processes: {
          if (c < arg.size() - 1) {
            options.parallel_processes =
                unwrap(dpsg::cli::parse_unsigned_int(arg.substr(c + 1)),
                       "Invalid parallel process count ", arg.substr(c + 1));
          } else {
            current_option = curopt::parallel_processes;
          }
          goto done_with_short_options;
        }
        default: {
          std::cerr << "Unexpected option " << cur << std::endl;
          exit(1);
        }
        }
      }
    done_with_short_options:
      break;
    }
    case expect_value:
      switch (current_option) {
      case curopt::count: {
        options.process_count = unwrap(dpsg::cli::parse_unsigned_int(arg),
                                       "Invalid run count ", arg);
      }
      case curopt::parallel_processes: {
        options.parallel_processes =
            unwrap(dpsg::cli::parse_unsigned_int(arg),
                   "Invalid parallel process count ", arg);
      }
      default:
        std::cerr << "This isn't right, fix the code" << std::endl;
        exit(1);
      }
      current_option = curopt::none;
      break;
    }
  }

  if (current_option != curopt::none) {
    std::cerr << "Option needs a value: " << argv[argc - 1] << std::endl;
  }
  return options;
}

struct run_result {
  std::string output_file;
  union {
    struct {
      int p1_score;
      int p2_score;
    };
    int scores[2];
  };
  size_t seed;
};

int main(int argc, const char **argv) {
  auto opts = parse_options(argc, argv);
  std::cout << (dpsg::vt100::bold | dpsg::vt100::setf(145, 145, 145));
  std::cout << "Process count: " << opts.process_count << std::endl;
  std::cout << "Generate output: " << std::boolalpha << opts.generate_output
            << std::endl;
  std::cout << "Parallel processes: " << opts.parallel_processes << std::endl;
  std::cout << dpsg::vt100::reset;

  if (opts.process_count <= 0) {
    std::cerr << "-c must be > 0" << std::endl;
    exit(1);
  }
  if (opts.parallel_processes <= 0) {
    std::cerr << "-p must be > 0" << std::endl;
    exit(1);
  }

  int left_to_run = opts.process_count;
  std::vector<dpsg::process_t> processes;
  processes.reserve(opts.parallel_processes);
  std::vector<run_result> results;
  results.reserve(opts.parallel_processes);
  while (left_to_run > 0) {
    int batch_size = std::min(left_to_run, opts.parallel_processes);
    std::cerr << "Running batch: " << std::endl;

    for (int i = 0; i < batch_size; ++i) {
      processes.emplace_back(run());
      std::cerr << "Process launched: " << (int)processes.back().pid << '\n';
    }

    int color = -1;
    dpsg::poll_one_each(
        std::span{processes}, dpsg::poll_event_t::read_ready,
        [&color](dpsg::process_t &proc) {
          std::cerr << "Got result for " << (int)proc.pid << "\t fd is "
                    << (int)proc.stdout << '\n';
          color = ((color + 1) % 6) + 1;
          int p1_score, p2_score;
          std::string seed;
          dpsg::fd_streambuf buf{proc.stdout};
          std::istream out{&buf};
          out >> p1_score >> p2_score >> seed;
          int eq = seed.find('=');
          std::string_view sid = std::string_view{seed}.substr(eq + 1);
          std::cout << dpsg::vt100::fg((dpsg::vt100::color)color)
                    << "Player 1: " << p1_score << '\n'
                    << "Player 2: " << p2_score << '\n'
                    << "Seed: " << sid << dpsg::vt100::reset << std::endl;
        });

    left_to_run -= batch_size;
    processes.clear();
  }

  return 0;
}
