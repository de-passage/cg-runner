#include "cli.hpp"
#include "options.hpp"
#include "posix.hpp"
#include "presentation.hpp"
#include "runner.hpp"
#include "statistics.hpp"
#include "vt100.hpp"

#include <chrono>

template <class F>
dpsg::posix::poll_error
poll_one_each(const std::span<dpsg::posix::process_t> &ps,
              dpsg::posix::poll_event_t ev, F &&func) {

  using namespace dpsg;
  using namespace dpsg::posix;
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

  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::now().time_since_epoch())
                 .count();
  auto timestamp = std::to_string(now);
  auto output_file = [&](int x) {
    return "output-" + timestamp + '-' + std::to_string(x) + ".json";
  };

  int left_to_run = opts.process_count;
  auto current_offset = 0;

  std::vector<dpsg::posix::process_t> processes;
  processes.reserve(opts.parallel_processes);
  std::vector<run_result> results;
  results.reserve(opts.parallel_processes);
  presenter p{std::cout};

  while (left_to_run > 0) {
    int batch_size = std::min(left_to_run, opts.parallel_processes);

    for (int i = 0; i < batch_size; ++i) {
      auto run_count = current_offset + i;
      auto of = output_file(run_count);
      processes.emplace_back(runner(of));
      results.emplace_back(run_result{.output_file = of});
      p.update_header(run_count);
    }
    p.update_statistics(stats);

    poll_one_each(std::span{processes}, dpsg::posix::poll_event_t::read_ready,
                  [current_offset, &results, &stats,
                   &p](dpsg::posix::process_t &proc, size_t idx) {
                    auto run_count = idx + current_offset;
                    auto &result = results[run_count];

                    std::string seed;

                    dpsg::posix::fd_streambuf buf{proc.stdout};
                    std::istream out{&buf};
                    out >> result.p1_score >> result.p2_score >> seed;

                    int eq = seed.find('=');
                    result.seed = std::string_view{seed}.substr(eq + 1);

                    aggregate(result, stats);
                    p.update_result(run_count, result, stats);
                  });

    left_to_run -= batch_size;
    current_offset += batch_size;
    processes.clear();
  }

  p.print_summary(stats, results);


  return 0;
}
