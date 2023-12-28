#include "statistics.hpp"

void aggregate(const run_result &result, statistics_t &stats) {
  if (result.has_error()) {
    if (result.has_error(run_result::error::p1_error)) {
      stats.player1_errors++;
    }
    if (result.has_error(run_result::error::p2_error)) {
      stats.player2_errors++;
    }
    return;
  }

  if (result.winner() == run_result::winner::p1) {
    stats.p1_wins(result.p1_score, result.p2_score);
  } else if (result.winner() == run_result::winner::p2) {
    stats.p2_wins(result.p1_score, result.p2_score);
  } else {
    stats.draw(result.p1_score, result.p2_score);
  }
}
