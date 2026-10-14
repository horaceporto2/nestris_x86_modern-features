#include "game_logic.hpp"

#include <iso646.h>

#include <algorithm>
#include <array>
#include <cmath>

#include "game_states.hpp"
#include "gravity.hpp"

namespace nestris_x86 {

constexpr int LINES_PER_LEVEL = 10;
const std::vector<int> line_scores{0, 40, 100, 300, 1200};

// ---- Modern rules constants ----
constexpr int MODERN_LOCK_DELAY_FRAMES = 30;  // 0.5 s @ 60 Hz
constexpr int MODERN_MAX_LOCK_RESETS = 15;
const std::vector<int> modern_line_scores{0, 100, 300, 500, 800};

int getScoreForLineClear(const int lines_cleared, const int level) {
  if (lines_cleared > 4) {
    throw std::runtime_error("Too many simultaneous lines cleared to get a score.");
  }
  return level * line_scores[lines_cleared];
}

int getModernScoreForLineClear(const int lines_cleared, const int level) {
  if (lines_cleared > 4) {
    throw std::runtime_error("Too many simultaneous lines cleared to get a score.");
  }
  return std::max(1, level) * modern_line_scores[lines_cleared];
}

// Guideline gravity: seconds/row = (0.8 - (L-1)*0.007)^(L-1). Returned in frames/row (min 1).
// Above ~level 20 the guideline wants >1 row per frame; we clamp at 1 row/frame.
int modernGravityFrames(const int level) {
  const int l = std::max(1, level);
  const double base = std::max(0.0, 0.8 - (l - 1) * 0.007);
  const double seconds = std::pow(base, l - 1);
  return std::max(1, static_cast<int>(std::lround(seconds * 60.0)));
}

GameState<>::Grid addTetrominoToGrid(const GameState<>::Grid &grid,
                                     const TetrominoState &tetromino) {
  GameState<>::Grid grid_copy(grid);
  const auto &tetromino_grid = getTetrominoGrid(tetromino);
  const auto [x_offset, y_offset] = getStartOffsets(tetromino.tetromino);
  const int x_start = tetromino.x - x_offset;
  const int y_start = tetromino.y - y_offset;
  const int color = getColor(tetromino.tetromino);
  for (int i = 0; i < tetromino_grid.size(); ++i) {
    for (int j = 0; j < tetromino_grid[i].size(); ++j) {
      if (outOfBounds(grid_copy, x_start + i, y_start + j) || not tetromino_grid[i][j]) {
        continue;
      }
      grid_copy[x_start + i][y_start + j] = tetromino_grid[i][j] * color;
    }
  }
  return grid_copy;
}

bool tetrominoCollision(const GameState<>::Grid &grid, const TetrominoState &tetromino) {
  const auto &tetromino_grid = getTetrominoGrid(tetromino);
  const auto [x_offset, y_offset] = getStartOffsets(tetromino.tetromino);
  const int x_start = tetromino.x - x_offset;
  const int y_start = tetromino.y - y_offset;
  for (int i = 0; i < tetromino_grid.size(); ++i) {
    for (int j = 0; j < tetromino_grid[i].size(); ++j) {
      if (not tetromino_grid[i][j]) {
        continue;
      }
      // Special case: If out of bounds above the play field, this is not a
      // collision (providing it is still within x bounds).
      if ((y_start + j < 0) && (x_start + i >= 0) && (x_start + i < grid.size())) {
        continue;
      }
      if (outOfBounds(grid, x_start + i, y_start + j) || grid[x_start + i][y_start + j]) {
        return true;
      }
    }
  }
  return false;
}

bool updateStateOnNoCollision(const GameState<>::Grid &grid, const int tetromino_x_offset,
                              const int tetromino_y_offset, const int tetromino_rotation_offset,
                              TetrominoState &tetromino) {
  TetrominoState test_tetromino{tetromino};
  test_tetromino.x += tetromino_x_offset;
  test_tetromino.y += tetromino_y_offset;
  auto &r = test_tetromino.rotation;
  r += tetromino_rotation_offset;
  r = r < 0 ? 3 : r;
  r = r > 3 ? 0 : r;
  if (not tetrominoCollision(grid, test_tetromino)) {
    tetromino = test_tetromino;
    return true;
  } else {
    return false;
  }
}

bool attemptWallKickRotate(const GameState<>::Grid &grid, const int tetromino_rotation_offset,
                           TetrominoState &tetromino) {
  if (updateStateOnNoCollision(grid, 0, 0, tetromino_rotation_offset, tetromino)) {
    return true;
  }
  for (int i = 1; i <= 2; ++i) {
    if (updateStateOnNoCollision(grid, i, 0, tetromino_rotation_offset, tetromino) ||
        updateStateOnNoCollision(grid, -i, 0, tetromino_rotation_offset, tetromino)) {
      return true;
    }
  }
  return false;
}

bool rotateTetromino(const GameState<>::Grid &grid, const int rotation, const bool wall_kick,
                     TetrominoState &tetromino) {
  if (wall_kick) {
    return attemptWallKickRotate(grid, rotation, tetromino);
  } else {
    return updateStateOnNoCollision(grid, 0, 0, rotation, tetromino);
  }
}

void hardDrop(const GameState<>::Grid &grid, TetrominoState &tetromino) {
  while (updateStateOnNoCollision(grid, 0, 1, 0, tetromino))
    ;
}

// ===================== MODERN MODE =====================

namespace {
struct Kick {
  int x, y;  // SRS convention: y points UP (inverted when applied).
};
using KickTable = std::array<std::array<Kick, 5>, 8>;

// Index = from_rotation * 2 + (counter-clockwise ? 1 : 0).
// Assumes rotation 0 = spawn, 1 = clockwise ("R"), 2 = "2", 3 = "L".
const KickTable KICKS_JLSTZ{{
    {{{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}}},  // 0>R
    {{{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}}},     // 0>L
    {{{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}}},      // R>2
    {{{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}}},      // R>0
    {{{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}}},     // 2>L
    {{{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}}},  // 2>R
    {{{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}}},   // L>0
    {{{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}}},   // L>2
}};
const KickTable KICKS_I{{
    {{{0, 0}, {-2, 0}, {1, 0}, {-2, -1}, {1, 2}}},  // 0>R
    {{{0, 0}, {-1, 0}, {2, 0}, {-1, 2}, {2, -1}}},  // 0>L
    {{{0, 0}, {-1, 0}, {2, 0}, {-1, 2}, {2, -1}}},  // R>2
    {{{0, 0}, {2, 0}, {-1, 0}, {2, 1}, {-1, -2}}},  // R>0
    {{{0, 0}, {2, 0}, {-1, 0}, {2, 1}, {-1, -2}}},  // 2>L
    {{{0, 0}, {1, 0}, {-2, 0}, {1, -2}, {-2, 1}}},  // 2>R
    {{{0, 0}, {1, 0}, {-2, 0}, {1, -2}, {-2, 1}}},  // L>0
    {{{0, 0}, {-2, 0}, {1, 0}, {-2, -1}, {1, 2}}},  // L>2
}};
}  // namespace

bool rotateTetrominoSrs(const GameState<>::Grid &grid, const int rotation_offset,
                        TetrominoState &tetromino, int *used_kick) {
  if (tetromino.tetromino == Tetromino::Square) {
    return false;  // O never changes shape: no sound and no lock-delay reset.
  }
  const bool ccw = rotation_offset < 0;
  const auto &table = (tetromino.tetromino == Tetromino::Line) ? KICKS_I : KICKS_JLSTZ;
  const int idx = tetromino.rotation * 2 + (ccw ? 1 : 0);
  int kick_number = 0;
  for (const auto &kick : table[idx]) {
    if (updateStateOnNoCollision(grid, kick.x, -kick.y, rotation_offset, tetromino)) {
      if (used_kick != nullptr) {
        *used_kick = kick_number;
      }
      return true;
    }
    ++kick_number;
  }
  return false;
}

TSpin classifyTSpin(const GameState<> &state) {
  const auto &t = state.active_tetromino;
  if (t.tetromino != Tetromino::T || not t.srs || not state.last_move_rotation) {
    return TSpin::NoTSpin;
  }
  const auto occupied = [&state](const int x, const int y) {
    if (x < 0 || x >= static_cast<int>(state.grid.size()) ||
        y >= static_cast<int>(state.grid[0].size())) {
      return true;
    }
    if (y < 0) {
      return false;
    }
    return state.grid[x][y] != 0;
  };
  const bool tl = occupied(t.x - 1, t.y - 1);
  const bool tr = occupied(t.x + 1, t.y - 1);
  const bool bl = occupied(t.x - 1, t.y + 1);
  const bool br = occupied(t.x + 1, t.y + 1);
  if (int{tl} + int{tr} + int{bl} + int{br} < 3) {
    return TSpin::NoTSpin;
  }
  bool front_corners_filled = false;
  switch (t.rotation) {
    case 0: front_corners_filled = tl && tr; break;
    case 1: front_corners_filled = tr && br; break;
    case 2: front_corners_filled = bl && br; break;
    default: front_corners_filled = tl && bl; break;
  }
  return (front_corners_filled || state.last_kick_index == 4) ? TSpin::Full : TSpin::Mini;
}

void scoreModernLock(GameState<> &state, const int lines_cleared) {
  static const int normal[5] = {0, 100, 300, 500, 800};
  static const int tspin_full[5] = {400, 800, 1200, 1600, 0};
  static const int tspin_mini[5] = {100, 200, 400, 0, 0};
  static const int perfect_clear[5] = {0, 800, 1200, 1800, 2000};
  static const char *names[5] = {"", "SINGLE", "DOUBLE", "TRIPLE", "TETRIS"};
  if (lines_cleared < 0 || lines_cleared > 4) {
    throw std::runtime_error("Invalid number of lines cleared.");
  }
  const int level = std::max(1, state.level);
  const TSpin tspin = state.pending_tspin;
  state.pending_tspin = TSpin::NoTSpin;
  int base = normal[lines_cleared];
  if (tspin == TSpin::Full) {
    base = tspin_full[lines_cleared];
  } else if (tspin == TSpin::Mini) {
    base = tspin_mini[lines_cleared];
  }
  int points = base * level;
  const bool difficult = lines_cleared > 0 && (lines_cleared == 4 || tspin != TSpin::NoTSpin);
  const bool b2b_applied = difficult && state.back_to_back;
  if (b2b_applied) {
    points = points * 3 / 2;
  }
  bool perfect = false;
  if (lines_cleared > 0) {
    ++state.combo;
    if (state.combo > 0) {
      points += 50 * state.combo * level;
    }
    state.back_to_back = difficult;
    int filled = 0;
    for (const auto &column : state.grid) {
      for (const int cell : column) {
        filled += cell != 0 ? 1 : 0;
      }
    }
    perfect = filled - lines_cleared * static_cast<int>(state.grid.size()) == 0;
    if (perfect) {
      points += perfect_clear[lines_cleared] * level;
    }
  } else {
    state.combo = -1;
  }
  if (lines_cleared == 0) {
    state.score += points;
  } else {
    state.pending_clear_score = points;
  }
  std::string message;
  if (b2b_applied) message += "B2B ";
  if (tspin == TSpin::Mini) message += "MINI ";
  if (tspin != TSpin::NoTSpin) message += "T-SPIN ";
  message += names[lines_cleared];
  if (perfect) message = "PERFECT CLEAR";
  while (not message.empty() && message.back() == ' ') message.pop_back();
  state.clear_message = message;
  state.clear_message_frames = message.empty() ? 0 : 120;
}

TetrominoState getGhostTetromino(const GameState<>::Grid &grid, const TetrominoState &tetromino) {
  TetrominoState ghost{tetromino};
  hardDrop(grid, ghost);
  return ghost;
}

void resetLockState(GameState<> &state) {
  state.lock_delay_counter = 0;
  state.lock_resets = 0;
  state.lowest_y = -100;
  state.force_lock = false;
}

// Called after a successful move/rotation: "move reset" lock delay (max 15 resets).
void registerLockReset(GameState<> &state) {
  if (state.lock_delay_counter > 0 && state.lock_resets < MODERN_MAX_LOCK_RESETS) {
    state.lock_delay_counter = 0;
    ++state.lock_resets;
  }
}

// Hold: reuses the game's normal spawn path. The processor must spawn
// `forced_spawn_tetromino` (if set) instead of `next_tetromino`.
void holdTetromino(GameState<> &state) {
  const Tetromino current = state.active_tetromino.tetromino;
  state.forced_spawn_tetromino = state.hold_tetromino;  // nullopt -> spawn next piece
  state.hold_tetromino = current;
  state.hold_used = true;
  state.entry_delay_counter = 0;
  state.spawn_new_tetromino = true;
  state.active_tetromino.y = -10;
  state.gravity_counter = modernGravityFrames(state.level);  // fresh piece: full gravity interval
  resetLockState(state);
}

void processKeyEvents(const KeyEvents &key_events, const sound::SoundPlayer &sample_player,
                      const Das &das_processor, const bool wall_kick, const bool hard_drop_enable,
                      GameState<> &state) {
  const bool modern = state.modern_mode;

  if (modern && key_events.at(KeyAction::Hold).pressed && not state.hold_used &&
      not state.spawn_new_tetromino) {
    holdTetromino(state);
    sample_player.playSample("tetromino_rotate");
    return;
  }

  // Preserve the classic frame order: NES hard drop happened before lateral moves/rotation.
  if (not modern && key_events.at(KeyAction::Up).pressed && hard_drop_enable) {
    hardDrop(state.grid, state.active_tetromino);
    state.gravity_counter = 0;
  }

  auto move_check_wall_charge = [&sample_player, &das_processor, modern](GameState<> &state,
                                                                         const int direction) {
    if (updateStateOnNoCollision(state.grid, direction, 0, 0, state.active_tetromino)) {
      sample_player.playSample("tetromino_move");
      if (modern) {
        state.last_move_rotation = false;
        registerLockReset(state);
      }
    } else {
      das_processor.fullyChargeDas(state.das_counter);
      state.viz_wall_charge_frame_count = 30;
    }
  };

  if (key_events.at(KeyAction::Left).pressed) {
    das_processor.hardResetDas(state.das_counter);
    move_check_wall_charge(state, -1);
  }
  if (key_events.at(KeyAction::Right).pressed) {
    das_processor.hardResetDas(state.das_counter);
    move_check_wall_charge(state, +1);
  }
  auto das_trigger = [&das_processor](int &das_counter) {
    ++das_counter;
    if (das_processor.dasFullyCharged(das_counter)) {
      das_processor.softResetDas(das_counter);
      return true;
    }
    return false;
  };
  if (key_events.at(KeyAction::Left).held) {
    if (das_trigger(state.das_counter)) {
      move_check_wall_charge(state, -1);
    }
  }
  if (key_events.at(KeyAction::Right).held) {
    if (das_trigger(state.das_counter)) {
      move_check_wall_charge(state, +1);
    }
  }

  if (key_events.at(KeyAction::Down).held) {
    const int soft_drop_frames = modern ? 1 : 2;
    auto &gr = state.gravity_counter;
    gr = (gr > soft_drop_frames) && not state.press_down_lock ? soft_drop_frames : gr;
  } else {
    state.press_down_lock = false;
    state.press_down_counter = 0;
  }

  int rotation = 0;
  rotation = key_events.at(KeyAction::RotateClockwise).pressed ? 1 : rotation;
  rotation = key_events.at(KeyAction::RotateAntiClockwise).pressed ? -1 : rotation;
  if (rotation != 0) {
    int used_kick = 0;
    const bool rotated = modern
                             ? rotateTetrominoSrs(state.grid, rotation, state.active_tetromino,
                                                  &used_kick)
                             : rotateTetromino(state.grid, rotation, wall_kick,
                                               state.active_tetromino);
    if (rotated) {
      sample_player.playSample("tetromino_rotate");
      if (modern) {
        state.last_move_rotation = true;
        state.last_kick_index = used_kick;
        registerLockReset(state);
      }
    }
  }

  // Hard drop is processed last so that a move/rotation pressed on the same frame is applied
  // first (otherwise the piece could end up away from the ghost's position).
  if (modern && key_events.at(KeyAction::Up).pressed) {
    const int y_before = state.active_tetromino.y;
    hardDrop(state.grid, state.active_tetromino);
    state.gravity_counter = 0;
    if (modern) {
      state.score += 2 * (state.active_tetromino.y - y_before);  // 2 pts / cell
      if (state.active_tetromino.y != y_before) {
        state.last_move_rotation = false;
      }
      state.force_lock = true;
    }
  }

  if (key_events.at(KeyAction::Start).pressed) {
    state.paused = true;
    sample_player.playSample("pause");
  }
}

void clearLine(const int row, GameState<> &state) {
  for (int y = row; y > 0; --y) {
    for (int x = 0; x < state.grid.size(); ++x) {
      state.grid[x][y] = state.grid[x][y - 1];
    }
  }
  for (int x = 0; x < state.grid.size(); ++x) {
    state.grid[x][0] = 0;
  }
}

std::vector<int> checkForLineClears(const GameState<> &state) {
  std::vector<int> complete_lines{};
  auto is_line_complete = [](const GameState<>::Grid &grid, const int row) {
    for (int x = 0; x < grid.size(); ++x) {
      if (grid[x][row] == 0) {
        return false;
      }
    }
    return true;
  };
  for (int y = 0; y < state.grid[0].size(); ++y) {
    if (is_line_complete(state.grid, y)) {
      complete_lines.push_back(y);
    }
  }
  return complete_lines;
}

void updateEntryDelayForLineClear(int &delay_counter, const bool modern) {
  // Emulate a quirk in the nes implementation: The animation would only
  // start on certain frames, randomly increasing/decreasing the ARE.
  // TODO:: -use modern c++ random and hold the random seed in class state
  delay_counter += (17 + (rand() % 5));
  // In modern mode there is no lock-height ARE, so the delay would be < 24 frames and
  // animateLineClear() would never reach frame 23 (where the clear sound is played).
  if (modern) {
    delay_counter = std::max(delay_counter, 24 + (rand() % 5));
  }
}

void updateScoreAndLevel(const int line_clears, const sound::SoundPlayer &sound_player,
                         GameState<> &state) {
  if (state.modern_mode) {
    state.score += state.pending_clear_score;
    state.pending_clear_score = 0;
  } else {
    state.score += getScoreForLineClear(line_clears, state.level);
  }
  state.lines += line_clears;
  state.lines_until_next_level -= line_clears;
  if (state.lines_until_next_level <= 0) {
    ++state.level;
    sound_player.playSample("level_up");
    state.lines_until_next_level += LINES_PER_LEVEL;
  }
}

int getEntryDelayFromLockHeight(const int height) {
  return static_cast<int>(height * -0.5 + 19);
}

void lockActiveTetromino(GameState<> &state) {
  if (state.modern_mode) {
    state.pending_tspin = classifyTSpin(state);
  }
  state.last_move_rotation = false;
  state.last_kick_index = 0;
  state.entry_delay_counter =
      state.modern_mode ? 0 : getEntryDelayFromLockHeight(state.active_tetromino.y);
  state.spawn_new_tetromino = true;
  state.grid = addTetrominoToGrid(state.grid, state.active_tetromino);
  state.active_tetromino.y = -10;
  state.hold_used = false;  // hold is available again for the next piece
  if (state.modern_mode) {
    // Otherwise a hard drop (gravity_counter = 0) makes the next piece fall a row on spawn.
    state.gravity_counter = modernGravityFrames(state.level);
  }
  resetLockState(state);
}

bool applyModernGravity(const KeyEvents &key_events, GameState<> &state) {
  TetrominoState below{state.active_tetromino};
  ++below.y;

  if (tetrominoCollision(state.grid, below)) {
    // Grounded: run the lock delay (hard drop forces an immediate lock).
    if (state.force_lock || ++state.lock_delay_counter >= MODERN_LOCK_DELAY_FRAMES) {
      lockActiveTetromino(state);
      return true;
    }
    return false;
  }

  if (--state.gravity_counter <= 0) {
    if (key_events.at(KeyAction::Down).held) {
      ++state.score;  // 1 pt per soft-dropped cell, banked right away (not lost if Down is released)
    }
    state.gravity_counter = modernGravityFrames(state.level);
    state.active_tetromino = below;
    state.last_move_rotation = false;
    if (below.y > state.lowest_y) {  // fell to a new lowest row: refresh the delay
      state.lowest_y = below.y;
      state.lock_resets = 0;
      state.lock_delay_counter = 0;
    }
  }
  return false;
}

bool applyGravity(const KeyEvents &key_events, const Gravity &gravity_provider,
                  GameState<> &state) {
  if (state.modern_mode) {
    return applyModernGravity(key_events, state);
  }
  if (--state.gravity_counter <= 0) {
    // Apply press down scoring.
    if (key_events.at(KeyAction::Down).held) {
      auto &down = state.press_down_counter;
      down = ++down > 15 ? 10 : down;
    }
    state.gravity_counter = gravity_provider.getGravity(state.level);
    if (not updateStateOnNoCollision(state.grid, 0, 1, 0, state.active_tetromino)) {
      lockActiveTetromino(state);
      return true;
    }
  }
  return false;
}

bool updateTopOutState(const KeyEvents &key_events, int &top_out_frame_counter,
                       GameState<> &state) {
  top_out_frame_counter++;
  constexpr int start_frame = 60;
  constexpr int animation_step = 4;
  constexpr int end_frame = start_frame + (20 * animation_step);
  if (key_events.at(KeyAction::Start).pressed) {
    if (top_out_frame_counter >= end_frame) {
      return true;
    } else {
      top_out_frame_counter = end_frame;
    }
  }
  if (top_out_frame_counter < start_frame || top_out_frame_counter > end_frame ||
      (top_out_frame_counter - start_frame) % animation_step) {
    return false;
  }
  const int top_out_step = (top_out_frame_counter - start_frame) / animation_step;
  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < top_out_step; ++j) {
      state.grid[i][j] = 4;
    }
  }
  return false;
}

int linesToClearFromStartingLevel(const int level) {
  if (level < 10) {
    return level * 10 + 10;
  } else {
    return std::max(100, level * 10 - 50);
  }
}

void animateLineClear(const sound::SoundPlayer &sample_player, GameState<> &state,
                      LineClearAnimationInfo &line_clear_info) {
  auto &frame = line_clear_info.animation_frame;
  if (frame == 0) {
    return;
  }
  --frame;

  if (frame == 23) {
    if (line_clear_info.rows.size() == 4) {
      sample_player.playSample("tetris");
    } else if (line_clear_info.rows.size() > 0) {
      sample_player.playSample("line_clear");
    }
  } else if (frame > 21) {
    // Still waiting for entry delay to end.
    return;
  } else if (frame >= 5) {
    // Line clear animation.
    const int blocks_to_remove = 6 - ((frame - 1) / 4);
    for (const auto &row : line_clear_info.rows) {
      for (int i = 4; i > 4 - blocks_to_remove; --i) {
        state.grid[i][row] = 0;
        state.grid[9 - i][row] = 0;
      }
    }
  } else if (frame == 3) {
    // Move lines down.
    for (const auto &row : line_clear_info.rows) {
      clearLine(row, state);
    }
  }
}

void addPressDownScore(GameState<> &state) {
  state.score += state.press_down_counter;
  state.press_down_counter = 0;
}

}  // namespace nestris_x86
