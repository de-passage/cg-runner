#include "presentation.hpp"
#include "statistics.hpp"
#include <cmath>
#include <iomanip>

void presenter::print_header(int run_count) {
  using namespace dpsg::vt100;
  const auto full_size = header_size(run_count) + char_cnt(elipsis);
  _out << set_cursor(count_to_row(run_count), count_to_col(run_count))
            << "Run " << run_count << ':' << red << " ..." << reset;
  for (int i = full_size; i < LINE_WIDTH; ++i) {
    _out.put(' ');
  }
  _out.flush();
}
void presenter::print_statistics(const statistics_t &stats) {
  using namespace dpsg::vt100;
  constexpr auto comment_color = setf(165, 165, 165);
  constexpr int score_size = 4;
  const auto s4 = std::setw(4);

  _out << set_cursor(LINE_NB + 2, 2);
  _out << comment_color << "Games played:" << s4 << stats.run_games()
            << " / " << s4 << stats.total_games << " (remaining:" << s4
            << stats.left_to_run() << ") ";

  if (stats.errors > 0) {
    _out << (bold | red) << "Errors:" << s4 << stats.errors << ' ';
  }
  if (stats.draws > 0) {
    _out << (bold | orange) << "Draws:" << s4 << stats.draws << ' ';
  }
  _out << reset << std::endl;

  constexpr char p1_txt[] = " Player 1 wins: ";
  constexpr char p2_txt[] = " :Player 2 wins";
  constexpr char sep[] = " | ";
  constexpr char op_paren[] = " (pts avg ";
  constexpr char cl_paren[] = ")";

  const auto format_double = [](double d) {
    if (std::isnan(d)) {
      return std::string("-");
    }
    const auto s = std::to_string(d);
    return s.substr(0, s.size() - 4); // TODO: no good
  };

  const std::string p1_avg = format_double(stats.player1_point_avg);
  const std::string p2_avg = format_double(stats.player2_point_avg);

  _out << comment_color << p1_txt << p1_color << std::setw(score_size)
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
    _out.put(' ');
  }
  _out << set_cursor(LINE_NB + 4, first_offset) << p1_color << p1_win_ratio
            << '%' << comment_color << " | " << p2_color << p2_win_ratio << '%'
            << reset << std::endl;
}

void presenter::print_result(int run_count, const run_result &result,
                  statistics_t &stats) {
  using namespace dpsg::vt100;
  _out << set_cursor(count_to_row(run_count),
                          count_to_col(run_count) + header_size(run_count));
  if (result.p1_score < 0 && result.p2_score < 0) {
    _out << (red | bold) << "Errors in both players!";
    stats.errors++;
  } else if (result.p1_score < 0) {
    _out << (red | bold) << "Error in player 1!";
    stats.errors++;
  } else if (result.p2_score < 0) {
    _out << (red | bold) << "Error in player 2!";
    stats.errors++;
  } else if (result.p2_score > result.p1_score) {
    _out << (bold | p2_color) << "Player 2 wins " << white << '('
              << p1_color << result.p1_score << white << '/' << p2_color
              << result.p2_score << white << ')';
    stats.p2_wins(result.p1_score, result.p2_score);
  } else if (result.p1_score > result.p2_score) {
    _out << (bold | p1_color) << "Player 1 wins " << white << '('
              << p1_color << result.p1_score << white << '/' << p2_color
              << result.p2_score << white << ')';
    stats.p1_wins(result.p1_score, result.p2_score);
  } else {
    _out << (bold | orange) << "Draw!";
    stats.draw(result.p1_score, result.p2_score);
  }
  _out << reset << std::flush;
  print_statistics(stats);
}
