#pragma once

#include <vector>

#include "das.hpp"
#include "game_states.hpp"
#include "gravity.hpp"
#include "key_defines.hpp"
#include "sound.hpp"
#include "statistics.hpp"

namespace nestris_x86 {

int getGravity(const int level);

bool entryDelay(const GameState<> &state);

template <typename Container>
bool outOfBounds(const Container &container, int x, int y) {
  if (x < 0 || y < 0 || x >= container.size() || y >= container[x].size()) {
    return true;
  } else
    return false;
}

int getScoreForLineClear(const int lines_cleared, const int level);

// ---- Modern mode ----
int getModernScoreForLineClear(const int lines_cleared, const int level);
int modernGravityFrames(const int level);
bool rotateTetrominoSrs(const GameState<>::Grid &grid, const int rotation_offset,
                        TetrominoState &tetromino, int *used_kick = nullptr);
TSpin classifyTSpin(const GameState<> &state);
void scoreModernLock(GameState<> &state, const int lines_cleared);
TetrominoState getGhostTetromino(const GameState<>::Grid &grid, const TetrominoState &tetromino);
void holdTetromino(GameState<> &state);

GameState<>::Grid addTetrominoToGrid(const GameState<>::Grid &grid,
                                     const TetrominoState &tetromino);

bool tetrominoCollision(const GameState<>::Grid &grid, const TetrominoState &tetromino);

bool updateStateOnNoCollision(const GameState<>::Grid &grid, const int tetromino_x_offset,
                              const int tetromino_y_offset, const int tetromino_rotation_offset,
                              TetrominoState &tetromino);

void processKeyEvents(const KeyEvents &key_events, const sound::SoundPlayer &sample_player,
                      const Das &das_processor, const bool wall_kick, const bool hard_drop_enable,
                      GameState<> &state);

void clearLine(const int row, GameState<> &state);

std::vector<int> checkForLineClears(const GameState<> &state);

void updateEntryDelayForLineClear(int &delay_counter, const bool modern = false);

void updateScoreAndLevel(const int line_clears, const sound::SoundPlayer &sound_player,
                         GameState<> &state);

int getEntryDelayFromLockHeight(const int height);

bool applyGravity(const KeyEvents &key_events, const Gravity &gravity_provider, GameState<> &state);

bool updateTopOutState(const KeyEvents &key_events, int &top_out_frame_counter, GameState<> &state);

int linesToClearFromStartingLevel(const int level);

void animateLineClear(const sound::SoundPlayer &sample_player, GameState<> &state,
                      LineClearAnimationInfo &line_clear_info);

void addPressDownScore(GameState<> &state);

}  // namespace nestris_x86
