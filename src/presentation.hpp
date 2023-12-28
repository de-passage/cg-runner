#ifndef HEADER_GUARD_DPSG_PRESENTATION_HPP
#define HEADER_GUARD_DPSG_PRESENTATION_HPP

#include "vt100.hpp"
#include <vector>

constexpr int digitnum(auto x) {
  int r = 0;
  if constexpr (std::is_signed_v<decltype(x)>) {
    r = x < 0 ? 1 : 0;
    x = x < 0 ? -x : x;
  }
  do {
    x /= 10;
    r++;
  } while (x > 0);
  return r;
}
static_assert(digitnum(-1) == 2);
static_assert(digitnum(-10) == 3);
static_assert(digitnum(-9) == 2);
static_assert(digitnum(0) == 1);
static_assert(digitnum(9) == 1);
static_assert(digitnum(10) == 2);

class presenter {
  std::ostream _out;

public:
  presenter(std::ostream &out) : _out(out.rdbuf()) {
    _out << dpsg::vt100::clear << dpsg::vt100::hide_cursor << std::endl;
  }
  ~presenter() { _out << dpsg::vt100::show_cursor << std::endl; }

private:
  constexpr static inline auto p1_color = dpsg::vt100::yellow;
  constexpr static inline auto p2_color = dpsg::vt100::cyan;
  constexpr static inline auto orange = dpsg::vt100::setf(255, 165, 0);
  constexpr static inline auto comment_color = dpsg::vt100::setf(165, 165, 165);

  constexpr static inline auto COL_MAX = 3;
  constexpr static inline auto LINE_NB = 20;
  constexpr static inline auto LINE_WIDTH = 120 / COL_MAX;
  template <std::size_t S> constexpr static int char_cnt(const char (&)[S]) {
    return S - 1;
  }
  constexpr static inline const char run[] = "Run ";
  constexpr static inline const char sep[] = ": ";

  constexpr static int header_size(int run_count) {
    return char_cnt(run) + digitnum(run_count+1) + char_cnt(sep);
  }
  constexpr int count_to_col(int run_count) {
    return (((run_count / LINE_NB) % COL_MAX) * LINE_WIDTH) + 1;
  }
  constexpr int count_to_row(int run_count) {
    return (run_count % LINE_NB) + 1;
  }

public:
  void update_header(int run_count);
  void update_result(int run_count, const struct run_result &result,
                    const struct statistics_t &stats);
  void print_summary(const struct statistics_t &stats, const std::vector<run_result> &results);
  void update_statistics(const struct statistics_t &stats);

private:
  void print_statistics(const struct statistics_t &stats);
  void print_result(const struct run_result &result);
};

#endif // HEADER_GUARD_DPSG_PRESENTATION_HPP
