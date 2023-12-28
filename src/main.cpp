#include "cli.hpp"
#include "posix.hpp"
#include "vt100.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <span>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace dpsg {
template <class T>
struct std::basic_ostream<T> &operator<<(struct std::basic_ostream<T> &out,
                                         pid_t p) {
  return out << (int)p;
}

template <class F, class R = int64_t, class P = std::milli>
poll_error poll_one_each(const std::span<process_t> &ps, poll_event_t ev,
                         F &&func) {
  std::vector<pollfd> pollfds;
  pollfds.reserve(ps.size());
  for (auto proc : ps) {
    pollfds.emplace_back(proc.stdout, ev);
  }
  auto s = ps.size();
  size_t n = 0;
  while (n < s) {
    auto r = poll(std::span{pollfds});
    if (r.is_error()) {
      auto e = r.error();
      if (e == poll_error::interrupted || e == poll_error::again) {
        continue;
      }
      return e;
    }

    for (size_t idx = 0; idx < pollfds.size(); ++idx) {
      if (pollfds[idx].revents == 0)
        continue;
      n++;
      pollfds[idx].invalidate();
      if constexpr (std::is_invocable_v<F, process_t &, size_t>) {
        func(ps[idx], idx);
      } else {
        func(ps[idx]);
      }
    }
  }

  return poll_error::success;
}
} // namespace dpsg

struct process_pool {
  std::vector<pid_t> pids;
  std::vector<dpsg::fd_t> ins;
  std::vector<dpsg::fd_t> outs;
};

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

struct run_result {
  std::string output_file;
  union {
    struct {
      int p1_score;
      int p2_score;
    };
    int scores[2];
  };
  std::string seed;
};

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

  dpsg::process_t operator()(std::string_view output_file) {
    if (cmd_args[GenerateFlag] != nullptr) {
      cmd_args[OutputName] = output_file.data();
    }
    return dpsg::run_external("java", cmd_args);
  }
};

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

struct statistics_t {
  union {
    size_t player_victory[2] = {0, 0};
    struct {
      size_t player1_victory;
      size_t player2_victory;
    };
  };
  union {
    double player_point_avg[2] = {0, 0};
    struct {
      double player1_point_avg;
      double player2_point_avg;
    };
  };
  int total_games = 0;
  int errors = 0;
  int draws = 0;

  int left_to_run() const { return total_games - run_games(); }

  int significant_games() const {
    return player1_victory + player2_victory + draws;
  }

  int run_games() const { return significant_games() + errors; }

  double win_ratio(int x) const {
    return (double)(player_victory[x]) / (double)run_games();
  }

  double p1_win_ratio() const { return win_ratio(0); }

  double p2_win_ratio() const { return win_ratio(1); }

  void add_player_victory(int x) { player_victory[x]++; }

  void add_player_points(int x, int points) {
    double c = significant_games();
    player_point_avg[x] =
        ((player_point_avg[x] * c) + (double)points) / (c + 1);
  }

  void add_points(int p1_score, int p2_score) {
    add_player_points(0, p1_score);
    add_player_points(1, p2_score);
  }

  void draw(int p1_score, int p2_score) {
    add_points(p1_score, p2_score);
    draws++;
  }

  void p1_wins(int p1_score, int p2_score) {
    add_points(p1_score, p2_score);
    add_player_victory(0);
  }
  void p2_wins(int p1_score, int p2_score) {
    add_points(p1_score, p2_score);
    add_player_victory(1);
  }
};

constexpr int digitnum(auto x) {
  auto r = 0;
  do {
    x /= 10;
    r++;
  } while (x > 0);
  return r;
}
static_assert(digitnum(0) == 1);
static_assert(digitnum(9) == 1);
static_assert(digitnum(10) == 2);
constexpr auto p1_color = dpsg::vt100::yellow;
constexpr auto p2_color = dpsg::vt100::cyan;
constexpr auto orange = dpsg::vt100::setf(255, 165, 0);
constexpr static inline auto COL_MAX = 3;
constexpr static inline auto LINE_NB = 20;
constexpr static inline auto LINE_WIDTH = 120 / COL_MAX;
template <std::size_t S> constexpr int char_cnt(const char (&)[S]) {
  return S - 1;
}
constexpr const char run[] = "Run ";
constexpr const char sep[] = ": ";
constexpr const char elipsis[] = "...";
constexpr int header_size(int run_count) {
  return char_cnt(run) + digitnum(run_count) + char_cnt(sep);
}
constexpr int count_to_col(int run_count) {
  return (((run_count / LINE_NB) % COL_MAX) * LINE_WIDTH) + 1;
}
constexpr int count_to_row(int run_count) { return (run_count % LINE_NB) + 1; }
constexpr void print_header(int run_count) {
  using namespace dpsg::vt100;
  const auto full_size = header_size(run_count) + char_cnt(elipsis);
  std::cout << set_cursor(count_to_row(run_count), count_to_col(run_count))
            << "Run " << run_count << ':' << red << " ..." << reset;
  for (int i = full_size; i < LINE_WIDTH; ++i) {
    std::cout.put(' ');
  }
  std::cout.flush();
}
void print_statistics(const statistics_t &stats) {
  using namespace dpsg::vt100;
  constexpr auto comment_color = setf(165, 165, 165);
  constexpr int score_size = 4;
  const auto s4 = std::setw(4);

  std::cout << set_cursor(LINE_NB + 2, 2);
  std::cout << comment_color << "Games played:" << s4 << stats.run_games()
            << " / " << s4 << stats.total_games << " (remaining:" << s4
            << stats.left_to_run() << ") ";

  if (stats.errors > 0) {
    std::cout << (bold | red) << "Errors:" << s4 << stats.errors << ' ';
  }
  if (stats.draws > 0) {
    std::cout << (bold | orange) << "Draws:" << s4 << stats.draws << ' ';
  }
  std::cout << reset << std::endl;

  constexpr char p1_txt[] = " Player 1 wins: ";
  constexpr char p2_txt[] = " :Player 2 wins";
  constexpr char sep[] =  " | ";
  constexpr char op_paren[] = " (pts avg ";
  constexpr char cl_paren[] = ")";

  const auto format_double = [](double d) {
    if (std::isnan(d)) {
      return std::string("-");
    }
    const auto s = std::to_string(d);
    return s.substr(0, s.size() - 4); // no good
  };

  const std::string p1_avg = format_double(stats.player1_point_avg);
  const std::string p2_avg = format_double(stats.player2_point_avg);

  std::cout << comment_color << p1_txt << p1_color << std::setw(score_size)
            << stats.player_victory[0] << op_paren << p1_avg << cl_paren
            << comment_color << sep << p2_color << std::setw(score_size)
            << stats.player_victory[1] << op_paren << p2_avg << cl_paren
            << comment_color << p2_txt << reset << std::endl;

  const auto p1_win_ratio = format_double(stats.p1_win_ratio() * 100);
  const auto p2_win_ratio = format_double(stats.p2_win_ratio() * 100);

  const size_t first_offset = char_cnt(p1_txt) + 4 + char_cnt(op_paren) +
                              p1_avg.size() + char_cnt(cl_paren) -
                              p1_win_ratio.size();

  for (size_t i = 0; i < first_offset; ++i) {
    std::cout.put(' ');
  }
  std::cout << set_cursor(LINE_NB + 4, first_offset) << p1_color << p1_win_ratio
            << '%' << comment_color << " | " << p2_color << p2_win_ratio << '%'
            << reset << std::endl;
}

void print_result(int run_count, const run_result &result,
                  statistics_t &stats) {
  using namespace dpsg::vt100;
  std::cout << set_cursor(count_to_row(run_count),
                          count_to_col(run_count) + header_size(run_count));
  if (result.p1_score < 0 && result.p2_score < 0) {
    std::cout << (red | bold) << "Errors in both players!";
    stats.errors++;
  } else if (result.p1_score < 0) {
    std::cout << (red | bold) << "Error in player 1!";
    stats.errors++;
  } else if (result.p2_score < 0) {
    std::cout << (red | bold) << "Error in player 2!";
    stats.errors++;
  } else if (result.p2_score > result.p1_score) {
    std::cout << (bold | p2_color) << "Player 2 wins " << white << '('
              << p1_color << result.p1_score << white << '/' << p2_color
              << result.p2_score << white << ')';
    stats.p2_wins(result.p1_score, result.p2_score);
  } else if (result.p1_score > result.p2_score) {
    std::cout << (bold | p1_color) << "Player 1 wins " << white << '('
              << p1_color << result.p1_score << white << '/' << p2_color
              << result.p2_score << white << ')';
    stats.p1_wins(result.p1_score, result.p2_score);
  } else {
    std::cout << (bold | orange) << "Draw!";
    stats.draw(result.p1_score, result.p2_score);
  }
  std::cout << reset << std::flush;
  print_statistics(stats);
}

int main(int argc, const char **argv) {
  using namespace dpsg::vt100;
  using namespace dpsg;
  auto opts = parse_options(argc, argv);

  if (opts.process_count <= 0) {
    std::cerr << "-c must be > 0" << std::endl;
    exit(1);
  }
  if (opts.process_count >= 1000) {
    std::cerr << "Keep the process count (-c) < 1000 please" << std::endl;
    exit(1);
  }
  if (opts.parallel_processes <= 0) {
    std::cerr << "-p must be > 0" << std::endl;
    exit(1);
  }
  if (opts.p1.empty() || opts.p2.empty() || opts.referee.empty()) {
    std::cerr
        << "You must specify commands for player 1, player 2 and the referee!"
        << std::endl;
    exit(1);
  }

  statistics_t stats{.total_games = opts.process_count};
  auto runner = make_runner(opts);

  std::vector<dpsg::process_t> processes;
  processes.reserve(opts.parallel_processes);
  std::vector<run_result> results;
  results.reserve(opts.parallel_processes);

  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::now().time_since_epoch())
                 .count();
  auto timestamp = std::to_string(now);
  auto output_file = [&](int x) {
    return "output-" + timestamp + '-' + std::to_string(x) + ".json";
  };

  int left_to_run = opts.process_count;
  auto current_offset = 0;

  std::cout << dpsg::vt100::clear;

  while (left_to_run > 0) {
    int batch_size = std::min(left_to_run, opts.parallel_processes);

    for (int i = 0; i < batch_size; ++i) {
      auto run_count = current_offset + i;
      auto of = output_file(run_count);
      processes.emplace_back(runner(of));
      results.emplace_back(run_result{.output_file = of});
      print_header(run_count);
    }
    print_statistics(stats);

    dpsg::poll_one_each(
        std::span{processes}, dpsg::poll_event_t::read_ready,
        [current_offset, &results, &stats](dpsg::process_t &proc, size_t idx) {
          auto run_count = idx + current_offset;
          auto &result = results[run_count];
          std::string seed;
          dpsg::fd_streambuf buf{proc.stdout};
          std::istream out{&buf};
          out >> result.p1_score >> result.p2_score >> seed;
          int eq = seed.find('=');
          result.seed = std::string_view{seed}.substr(eq + 1);
          print_result(run_count, result, stats);
        });

    left_to_run -= batch_size;
    current_offset += batch_size;
    processes.clear();
  }

  return 0;
}
