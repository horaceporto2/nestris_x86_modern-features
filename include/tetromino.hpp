#pragma once

#include <tuple>
#include <utility>
#include <vector>

namespace nestris_x86 {

enum class Tetromino { T, J, Z, Square, S, L, Line };

struct TetrominoState {
  Tetromino tetromino;
  int x;
  int y;
  int rotation;
  bool srs{false};  // modern mode: use SRS orientations (4 distinct states)
};

using TetrominoGrid = std::vector<std::vector<int>>;

// Tetrominos defines below. The matrices are column-major, resulting in access as matrix[x][y].
// The default rotation for nes tetris is defined first, followed by clockwise rotation order. This
// results in moving +1 through the vector results in a clockwise rotation, and likewise -1 results
// in a anticlockwise rotation.

const std::vector<TetrominoGrid> T_TETROMINO{{{{0, 1, 0}, {0, 1, 1}, {0, 1, 0}}},
                                             {{{0, 1, 0}, {1, 1, 1}, {0, 0, 0}}},
                                             {{{0, 1, 0}, {1, 1, 0}, {0, 1, 0}}},
                                             {{{0, 0, 0}, {1, 1, 1}, {0, 1, 0}}}};

const std::vector<TetrominoGrid> J_TETROMINO{{{{0, 1, 0}, {0, 1, 0}, {0, 1, 1}}},
                                             {{{0, 0, 1}, {1, 1, 1}, {0, 0, 0}}},
                                             {{{1, 1, 0}, {0, 1, 0}, {0, 1, 0}}},
                                             {{{0, 0, 0}, {1, 1, 1}, {1, 0, 0}}}};

const std::vector<TetrominoGrid> L_TETROMINO{{{{0, 1, 1}, {0, 1, 0}, {0, 1, 0}}},
                                             {{{1, 0, 0}, {1, 1, 1}, {0, 0, 0}}},
                                             {{{0, 1, 0}, {0, 1, 0}, {1, 1, 0}}},
                                             {{{0, 0, 0}, {1, 1, 1}, {0, 0, 1}}}};

const std::vector<TetrominoGrid> S_TETROMINO{{{{0, 0, 1}, {0, 1, 1}, {0, 1, 0}}},
                                             {{{0, 0, 0}, {1, 1, 0}, {0, 1, 1}}}};

const std::vector<TetrominoGrid> Z_TETROMINO{{{{0, 1, 0}, {0, 1, 1}, {0, 0, 1}}},
                                             {{{0, 0, 0}, {0, 1, 1}, {1, 1, 0}}}};

const std::vector<TetrominoGrid> I_TETROMINO{
    {{{0, 0, 1, 0}, {0, 0, 1, 0}, {0, 0, 1, 0}, {0, 0, 1, 0}}},
    {{{0, 0, 0, 0}, {0, 0, 0, 0}, {1, 1, 1, 1}, {0, 0, 0, 0}}}};

const TetrominoGrid SQUARE_TETROMINO{{{1, 1}, {1, 1}}};

inline TetrominoGrid getTetrominoGrid(const Tetromino &tetromino, const int rotation) {
  if (tetromino == Tetromino::T) {
    return T_TETROMINO[rotation];
  } else if (tetromino == Tetromino::J) {
    return J_TETROMINO[rotation];
  } else if (tetromino == Tetromino::Z) {
    return Z_TETROMINO[rotation % 2];
  } else if (tetromino == Tetromino::S) {
    return S_TETROMINO[rotation % 2];
  } else if (tetromino == Tetromino::L) {
    return L_TETROMINO[rotation];
  } else if (tetromino == Tetromino::Line) {
    return I_TETROMINO[rotation % 2];
  } else {
    return SQUARE_TETROMINO;
  }
}

// ---- SRS orientations (modern mode) ----
// States: 0 = spawn, 1 = clockwise ("R"), 2, 3 = counter-clockwise ("L").
// T/J/L reuse the NES tables: the NES spawn orientation is the SRS state 2, so srs[r] = nes[(r+2)%4].
// S/Z/I have only 2 NES states, so full 4-state tables are built here (matrices are [x][y]).
inline TetrominoGrid gridFromCells(const int size, const std::vector<std::pair<int, int>> &cells) {
  TetrominoGrid grid(size, std::vector<int>(size, 0));
  for (const auto &[x, y] : cells) {
    grid[x][y] = 1;
  }
  return grid;
}

inline TetrominoGrid getSrsTetrominoGrid(const Tetromino &tetromino, const int rotation) {
  static const std::vector<TetrominoGrid> S_SRS{
      gridFromCells(3, {{1, 0}, {2, 0}, {0, 1}, {1, 1}}), gridFromCells(3, {{1, 0}, {1, 1}, {2, 1}, {2, 2}}),
      gridFromCells(3, {{1, 1}, {2, 1}, {0, 2}, {1, 2}}), gridFromCells(3, {{0, 0}, {0, 1}, {1, 1}, {1, 2}})};
  static const std::vector<TetrominoGrid> Z_SRS{
      gridFromCells(3, {{0, 0}, {1, 0}, {1, 1}, {2, 1}}), gridFromCells(3, {{2, 0}, {1, 1}, {2, 1}, {1, 2}}),
      gridFromCells(3, {{0, 1}, {1, 1}, {1, 2}, {2, 2}}), gridFromCells(3, {{1, 0}, {0, 1}, {1, 1}, {0, 2}})};
  static const std::vector<TetrominoGrid> I_SRS{
      gridFromCells(4, {{0, 1}, {1, 1}, {2, 1}, {3, 1}}), gridFromCells(4, {{2, 0}, {2, 1}, {2, 2}, {2, 3}}),
      gridFromCells(4, {{0, 2}, {1, 2}, {2, 2}, {3, 2}}), gridFromCells(4, {{1, 0}, {1, 1}, {1, 2}, {1, 3}})};
  const int r = ((rotation % 4) + 4) % 4;
  switch (tetromino) {
    case Tetromino::T: return T_TETROMINO[(r + 2) % 4];
    case Tetromino::J: return J_TETROMINO[(r + 2) % 4];
    case Tetromino::L: return L_TETROMINO[(r + 2) % 4];
    case Tetromino::S: return S_SRS[r];
    case Tetromino::Z: return Z_SRS[r];
    case Tetromino::Line: return I_SRS[r];
    default: return SQUARE_TETROMINO;
  }
}

inline TetrominoGrid getTetrominoGrid(const TetrominoState &tetromino) {
  if (tetromino.srs) {
    return getSrsTetrominoGrid(tetromino.tetromino, tetromino.rotation);
  }
  return getTetrominoGrid(tetromino.tetromino, tetromino.rotation);
}

inline int getColor(const Tetromino &tetromino) {
  if (tetromino == Tetromino::T || tetromino == Tetromino::Square || tetromino == Tetromino::Line) {
    return 1;
  } else if (tetromino == Tetromino::J || tetromino == Tetromino::S) {
    return 2;
  } else {
    return 3;
  }
}

inline std::tuple<int, int> getStartOffsets(const Tetromino &tetromino) {
  if (tetromino == Tetromino::T || tetromino == Tetromino::J || tetromino == Tetromino::L ||
      tetromino == Tetromino::S || tetromino == Tetromino::Z) {
    return {1, 1};
  } else if (tetromino == Tetromino::Line) {
    return {2, 2};
  } else {
    return {1, 0};
  }
}

}  // namespace nestris_x86
