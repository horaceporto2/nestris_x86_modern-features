
#include "frame_processors/game_processor.hpp"

#include <iso646.h>

#include <iterator>
#include <stdexcept>

#include "assets.hpp"
#include "drawers/pixel_drawing_interface.hpp"
#include "game_logic.hpp"
#include "gravity.hpp"
#include "key_defines.hpp"
#include "sound.hpp"
#include "utils/logging.hpp"

namespace nestris_x86 {

constexpr int GRAVITY_FIRST_FRAME = 100;

namespace {
// Modern (SRS) pieces spawn one row lower so the whole piece is visible on a 20-row field.
TetrominoState makeSpawnState(const Tetromino tetromino, const bool modern) {
  const int y = (modern && tetromino != Tetromino::Square) ? 1 : 0;
  return TetrominoState{tetromino, 5, y, 0, modern};
}

void applyModernMode(GameState<>& state, const bool modern) {
  state.modern_mode = modern;
  if (modern) {
    state.active_tetromino = makeSpawnState(state.active_tetromino.tetromino, true);
  }
}
}  // namespace

GameProcessor::GameProcessor(const GameOptions& options,
                             std::unique_ptr<PixelDrawingInterface>&& drawer,
                             const std::shared_ptr<sound::SoundPlayer>& sample_player,
                             const std::shared_ptr<SpriteProvider>& sprite_provider)
    : renderer_(std::move(drawer), sprite_provider, "./assets/images"),
      sample_player_(sample_player),
      state_{},
      statistics_{},
      tetromino_rng_{tetrominoRngFactory(RngType::Nes)},
      das_processor_{options.das_full_charge, options.das_min_charge},
      gravity_provider_{options.gravity_type},
      show_controls_{options.show_controls},
      show_das_bar_{options.show_das_bar},
      wall_kick_{options.wall_kick},
      hard_drop_{options.hard_drop},
      statistics_mode_{options.statistics_mode},
      line_clear_info_{},
      top_out_frame_counter_{} {
  state_ = getNewState(options.level);
  applyModernMode(state_, options.modern_mode);
  statistics_.update(state_.active_tetromino.tetromino);
}

void GameProcessor::reset(const GameOptions& options) {
  (state_.modern_mode ? high_scores_modern_ : high_scores_classic_) = state_.high_scores;
  das_processor_ = Das{options.das_full_charge, options.das_min_charge};
  gravity_provider_ = Gravity{options.gravity_type};
  show_controls_ = options.show_controls;
  show_das_bar_ = options.show_das_bar;
  wall_kick_ = options.wall_kick;
  hard_drop_ = options.hard_drop;
  statistics_mode_ = options.statistics_mode;
  line_clear_info_ = {};
  top_out_frame_counter_ = {};
  tetromino_rng_ = tetrominoRngFactory(options.rng_type);  // before drawing the first pieces
  state_ = getNewState(options.level);
  state_.high_scores = options.modern_mode ? high_scores_modern_ : high_scores_classic_;
  applyModernMode(state_, options.modern_mode);
  statistics_ = {};
  statistics_.update(state_.active_tetromino.tetromino);
  renderer_.startNewGame();
}

Tetromino GameProcessor::getRandomTetromino() {
  return tetromino_rng_->getRandomTetromino();
}

bool GameProcessor::spawnNewTetromino(GameState<>& state) {
  Tetromino to_spawn = state.next_tetromino;
  if (state.forced_spawn_tetromino.has_value()) {
    to_spawn = *state.forced_spawn_tetromino;  // piece coming out of Hold: queue is untouched
    state.forced_spawn_tetromino.reset();
  } else {
    state.next_tetromino = getRandomTetromino();
  }
  state.active_tetromino = makeSpawnState(to_spawn, state.modern_mode);
  state.spawn_new_tetromino = false;
  return not tetrominoCollision(state.grid, state.active_tetromino);
}

GameState<> GameProcessor::getNewState(const int level) {
  auto state = GameState<>{};
  state.level = level;
  state.high_scores = state_.high_scores;
  state.active_tetromino = {getRandomTetromino(), 5, 0, 0};
  state.next_tetromino = getRandomTetromino();
  state.gravity_counter = GRAVITY_FIRST_FRAME;
  state.lines_until_next_level = linesToClearFromStartingLevel(state.level);
  state.viz_wall_charge_frame_count = 0;
  return state;
}

void GameProcessor::doGravityStep(const KeyEvents& key_events) {
  auto spawn_if_needed = [this]() {
    if (not state_.spawn_new_tetromino) {
      return;
    }
    const bool from_hold = state_.forced_spawn_tetromino.has_value();
    const bool topped_out = not spawnNewTetromino(state_);
    if (not from_hold) {
      statistics_.update(state_.active_tetromino.tetromino);  // swapped-in pieces aren't new
    }
    if (topped_out) {
      // Lock the active tetromino and move it off the grid. This is to stop the active
      // tetromino interfering with the 'curtain' animation.
      state_.grid = addTetrominoToGrid(state_.grid, state_.active_tetromino);
      state_.active_tetromino.y = -10;
      state_.topped_out = true;
      sample_player_->playSample("top_out");
    }
  };

  spawn_if_needed();
  processKeyEvents(key_events, *sample_player_, das_processor_, wall_kick_, hard_drop_, state_);
  spawn_if_needed();  // Hold requested this frame: spawn the swapped piece immediately.
  if (not das_processor_.dasSoftlyCharged(state_.das_counter)) {
    statistics_.dasResetSignal();
  }

  const bool tetromino_locked = applyGravity(key_events, gravity_provider_, state_);
  if (tetromino_locked) {
    sample_player_->playSample("tetromino_lock");
    // Classic: Down must be released before it works on the next piece. Modern: keep soft-dropping.
    state_.press_down_lock = not state_.modern_mode;
    addPressDownScore(state_);
    auto lines_cleared = checkForLineClears(state_);
    if (state_.modern_mode) {
      scoreModernLock(state_, static_cast<int>(lines_cleared.size()));
    }
    if (not lines_cleared.empty()) {
      updateEntryDelayForLineClear(state_.entry_delay_counter, state_.modern_mode);
      line_clear_info_ = LineClearAnimationInfo{lines_cleared, state_.entry_delay_counter};
    }
  }
}

void GameProcessor::doEntryDelayStep(const KeyEvents& key_events) {
  if (key_events.at(KeyAction::Start).pressed) {
    state_.paused = true;
    sample_player_->playSample("pause");
  }

  animateLineClear(*sample_player_, state_, line_clear_info_);
  if (line_clear_info_.rows.size() == 4) {
    renderer_.doTetrisFlash(line_clear_info_.animation_frame);
  }
  // When the animation is almost over, update the score.
  if (line_clear_info_.animation_frame == 4) {
    const int level_before = state_.level;
    updateScoreAndLevel(static_cast<int>(line_clear_info_.rows.size()), *sample_player_, state_);
    statistics_.update(static_cast<int>(line_clear_info_.rows.size()), level_before,
                       state_.modern_mode);
  }
  --state_.entry_delay_counter;
}

bool checkForHighScore(GameState<>& state) {
  auto insertion = state.high_scores.insert({state.score, ""});
  return std::distance(insertion.first, state.high_scores.end()) < 3;
}

ProgramFlowSignal GameProcessor::processFrame(const KeyEvents& key_events) {
  if (key_events.at(KeyAction::Back).pressed) {
    return ProgramFlowSignal::LevelSelectorScreen;
  }
  if (state_.topped_out) {
    const bool end_game = updateTopOutState(key_events, top_out_frame_counter_, state_);
    if (end_game) {
      if (checkForHighScore(state_)) {
        return ProgramFlowSignal::NewHighScoreScreen;
      } else {
        return ProgramFlowSignal::LevelSelectorScreen;
      }
    }
  } else if (state_.paused) {
    if (key_events.at(KeyAction::Start).pressed) {
      state_.paused = false;
    }
  } else if (entryDelay(state_)) {
    doEntryDelayStep(key_events);
  } else {
    doGravityStep(key_events);
  }
  renderer_.renderGameState(state_, statistics_, show_controls_, show_das_bar_, statistics_mode_,
                            key_events, das_processor_);

  state_.viz_wall_charge_frame_count = std::max(state_.viz_wall_charge_frame_count - 1, 0);
  state_.clear_message_frames = std::max(state_.clear_message_frames - 1, 0);
  return ProgramFlowSignal::FrameSuccess;
}

};  // namespace nestris_x86
