#pragma once

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "key_defines.hpp"
#include "tetromino.hpp"

namespace nestris_x86 {

enum class TSpin { NoTSpin, Mini, Full };

template <int W = 10, int H = 20>
// clang-format off
struct GameState {
  GameState()
      : grid{},
        das_counter{},
        gravity_counter{},
        entry_delay_counter{},
        spawn_new_tetromino{},
        level{},
        lines{},
        lines_until_next_level{},
        active_tetromino{},
        next_tetromino{},
        score{},
        topped_out{},
        paused{} ,
        press_down_lock{},
        press_down_counter{},
        viz_wall_charge_frame_count{},
        high_scores{{1000, "HOWARD"}},
        modern_mode{},
        hold_tetromino{},
        hold_used{},
        forced_spawn_tetromino{},
        lock_delay_counter{},
        lock_resets{},
        lowest_y{-100},
        force_lock{},
        last_move_rotation{},
        last_kick_index{},
        pending_tspin{TSpin::NoTSpin},
        combo{-1},
        back_to_back{},
        pending_clear_score{},
        clear_message{},
        clear_message_frames{} {}
  // clang-format on

  using Grid = std::array<std::array<int, H>, W>;
  Grid grid;
  int gravity_counter;
  int das_counter;
  int entry_delay_counter;
  bool spawn_new_tetromino;
  int level;
  int lines;
  int lines_until_next_level;
  TetrominoState active_tetromino;
  Tetromino next_tetromino;
  int score;
  bool topped_out;
  bool paused;
  bool press_down_lock;
  int press_down_counter;
  int viz_wall_charge_frame_count;
  std::map<int, std::string> high_scores;

  // ---- Modern mode ----
  bool modern_mode;
  std::optional<Tetromino> hold_tetromino;
  bool hold_used;
  std::optional<Tetromino> forced_spawn_tetromino;  // set by Hold, consumed on spawn
  int lock_delay_counter;
  int lock_resets;
  int lowest_y;
  bool force_lock;
  bool last_move_rotation;
  int last_kick_index;
  TSpin pending_tspin;
  int combo;
  bool back_to_back;
  int pending_clear_score;
  std::string clear_message;
  int clear_message_frames;
};

inline bool entryDelay(const GameState<>& state) {
  return state.entry_delay_counter > 0;
}

struct LineClearAnimationInfo {
  std::vector<int> rows;
  int animation_frame{};
};

enum class StatisticsMode { Classic, TreyVision };

inline StatisticsMode statisticsModeFromString(const std::string& statistics_mode) {
  if (statistics_mode == "NES") {
    return StatisticsMode::Classic;
  } else if (statistics_mode == "TREY V") {
    return StatisticsMode::TreyVision;
  } else {
    throw std::runtime_error("Invalid statistics mode `" + statistics_mode + "`.");
  }
  return StatisticsMode::Classic;
}

}  // namespace nestris_x86
