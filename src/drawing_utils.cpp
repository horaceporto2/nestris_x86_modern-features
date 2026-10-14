#include "drawing_utils.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace nestris_x86 {
using pdi = PixelDrawingInterface;

void drawWidescreenRails(const PixelDrawingInterface& drawer) {
  const auto frame = pdi::Color{40, 174, 166, 255};
  const auto dim = pdi::Color{17, 64, 67, 255};
  drawer.fillRect(0, 0, WIDESCREEN_WIDTH, WIDESCREEN_HEIGHT, pdi::BLACK());
  drawer.fillRect(4, 4, 1, 217, frame);
  drawer.fillRect(67, 4, 1, 217, dim);
  drawer.fillRect(331, 4, 1, 217, dim);
  drawer.fillRect(395, 4, 1, 217, frame);
  drawer.fillRect(4, 4, 64, 1, dim);
  drawer.fillRect(332, 4, 64, 1, dim);
  drawer.fillRect(4, 220, 64, 1, dim);
  drawer.fillRect(332, 220, 64, 1, dim);
  drawer.drawString(10, 12, "NESTRIS", frame);
  drawer.drawString(340, 12, "16:9", frame);
}

void drawNumber(const PixelDrawingInterface& drawer, const int x, const int y, const int num,
                const int pad, const pdi::Color& color) {
  const auto clipped_num = std::min(num, static_cast<int>(std::pow(10, pad) - 1));
  constexpr int TEXT_WIDTH_PX = 8;
  drawer.fillRect(x, y, TEXT_WIDTH_PX * pad, TEXT_WIDTH_PX, pdi::BLACK());
  std::stringstream ss;
  ss << std::setw(pad) << std::setfill('0') << clipped_num;
  drawer.drawString(x, y, ss.str(), color);
}

void drawNumber(const PixelDrawingInterface& drawer, const pdi::Coords& coords, const int num,
                const int pad, const pdi::Color& color) {
  drawNumber(drawer, coords.x, coords.y, num, pad, color);
}

void drawTriangleSelector(const PixelDrawingInterface& drawer, const int x, const int y,
                          const int size, const pdi::Color& color, const bool right) {
  for (int j = 0; j < size; ++j) {
    const int j_max = j < size / 2 ? j + 1 : size - j;
    if (right) {
      for (int i = 0; i > -j_max; --i) {
        drawer.drawPixel(x + i, y + j, color);
      }
    } else {
      for (int i = 0; i < j_max; ++i) {
        drawer.drawPixel(x + i, y + j, color);
      }
    }
  }
}

void drawUpArrow(const PixelDrawingInterface& drawer, const int x, const int y) {
  drawer.drawLine(x, y, x, y + 6);
  drawer.drawLine(x - 1, y, x - 1, y + 6);
  drawer.drawLine(x - 1, y, x - 4, y + 3);
  drawer.drawLine(x, y, x + 3, y + 3);
  drawer.drawLine(x - 1, y + 1, x - 3, y + 3);
  drawer.drawLine(x, y + 1, x + 2, y + 3);
}
}  // namespace nestris_x86
