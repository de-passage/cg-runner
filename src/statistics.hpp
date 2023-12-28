#ifndef HEADER_GUARD_DPSG_STATISTICS_HPP
#define HEADER_GUARD_DPSG_STATISTICS_HPP

#include <cstddef>
#include <string>

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

#endif // HEADER_GUARD_DPSG_STATISTICS_HPP
