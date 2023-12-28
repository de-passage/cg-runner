#include "options.hpp"

option_t parse_options(int argc, const char **argv) {

  enum { expect_option, expect_value } expectation = expect_option;

  enum class curopt : char {
    none = 0,
    count = 'c',
    generate_output = 'G',
    parallel_processes = 'p',
    player_1 = '1',
    player_2 = '2',
    referee = 'r',
    debug = 'd'

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
            expectation = expect_value;
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
            expectation = expect_value;
          }
          goto done_with_short_options;
        }
        case curopt::player_1: {
          if (c < arg.size() - 1) {
            options.p1 = arg.substr(c + 1);
          } else {
            current_option = curopt::player_1;
            expectation = expect_value;
          }
          goto done_with_short_options;
        }
        case curopt::player_2: {
          if (c < arg.size() - 1) {
            options.p2 = arg.substr(c + 1);
          } else {
            current_option = curopt::player_2;
            expectation = expect_value;
          }
          goto done_with_short_options;
        }
        case curopt::referee: {
          if (c < arg.size() - 1) {
            options.referee = arg.substr(c + 1);
          } else {
            current_option = curopt::referee;
            expectation = expect_value;
          }
          goto done_with_short_options;
        }
        case curopt::debug: {
          options.debug = true;
          break;
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
        break;
      }
      case curopt::parallel_processes: {
        options.parallel_processes =
            unwrap(dpsg::cli::parse_unsigned_int(arg),
                   "Invalid parallel process count ", arg);
        break;
      }
      case curopt::player_1: {
        options.p1 = arg;
        break;
      }
      case curopt::player_2: {
        options.p2 = arg;
        break;
      }
      case curopt::referee: {
        options.referee = arg;
        break;
      }
      default:
        std::cerr << "This isn't right, fix the code: at " << i << '(' << arg
                  << ')' << (int)current_option << std::endl;
        exit(1);
      }
      current_option = curopt::none;
      expectation = expect_option;
      break;
    }
  }

  if (current_option != curopt::none) {
    std::cerr << "Option needs a value: " << argv[argc - 1] << std::endl;
  }
  return options;
}
