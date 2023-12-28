#ifndef HEADER_GUARD_DPSG_STATISTICS_HPP
#define HEADER_GUARD_DPSG_STATISTICS_HPP

#include <cstddef>
#include <string>

struct statistics_t {
  enum class player: int {
    p1 = 0,
    p2 = 1,
  };
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
  union {
    int player_errors[2] = {0, 0};
    struct {
      int player1_errors;
      int player2_errors;
    };
  };

  int total_games = 0;
  int draws = 0;

  int left_to_run() const { return total_games - run_games(); }

  int significant_games() const {
    return player1_victory + player2_victory + draws;
  }

  int errors() const { return player1_errors + player2_errors; }
  int errors(player x) const { return player_errors[(int)x]; }

  int run_games() const { return significant_games() + errors(); }

  double win_ratio(player x) const {
    return (double)(player_victory[(int)x]) / (double)run_games();
  }

  double p1_win_ratio() const { return win_ratio(player::p1); }

  double p2_win_ratio() const { return win_ratio(player::p2); }

  void draw(int p1_score, int p2_score) {
    _add_points(p1_score, p2_score);
    draws++;
  }

  void p1_wins(int p1_score, int p2_score) {
    _add_points(p1_score, p2_score);
    _add_player_victory(0);
  }
  void p2_wins(int p1_score, int p2_score) {
    _add_points(p1_score, p2_score);
    _add_player_victory(1);
  }

private:
  void _add_player_victory(int x) { player_victory[x]++; }

  void _add_player_points(int x, int points) {
    double c = significant_games();
    player_point_avg[x] =
        ((player_point_avg[x] * c) + (double)points) / (c + 1);
  }

  void _add_points(int p1_score, int p2_score) {
    _add_player_points(0, p1_score);
    _add_player_points(1, p2_score);
  }
};

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

  enum class error {
    none = 0,
    p1_error = 1,
    p2_error = 2,
    both_error = 1|2,
  };

  enum class winner {
    draw = 0,
    p1 = 1,
    p2 = 2,
    error = -1,
  };

  constexpr error get_error() const {
    if (p1_score < 0 && p2_score < 0) {
      return error::both_error;
    } else if (p1_score < 0) {
      return error::p1_error;
    } else if (p2_score < 0) {
      return error::p2_error;
    } else {
      return error::none;
    }
  }
  constexpr bool has_error(error e) const {
    return ((int)get_error() & (int)e) == (int)e;
  }
  bool has_error() const { return get_error() != error::none; }

  winner winner() const {
    if (p1_score > p2_score) {
      return winner::p1;
    } else if (p2_score > p1_score) {
      return winner::p2;
    } else if (p1_score == p2_score) {
      return winner::draw;
    } else {
      return winner::error;
    }
  }
};

void aggregate(const run_result &result, statistics_t &stats);

#endif // HEADER_GUARD_DPSG_STATISTICS_HPP
