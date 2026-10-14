#pragma once

#include "drawers/pixel_drawing_interface.hpp"

namespace nestris_x86 {
inline constexpr int WIDESCREEN_WIDTH = 400;
inline constexpr int WIDESCREEN_HEIGHT = 225;
inline constexpr int WIDESCREEN_LEGACY_OFFSET_X = 72;

void drawWidescreenRails(const PixelDrawingInterface& drawer);

void drawNumber(const PixelDrawingInterface& drawer,                                          //
                const int x,                                                                  //
                const int y,                                                                  //
                const int num,                                                                //
                const int pad,                                                                //
                const PixelDrawingInterface::Color& color = PixelDrawingInterface::WHITE());  //

void drawNumber(const PixelDrawingInterface& drawer,                                          //
                const PixelDrawingInterface::Coords& coords,                                  //
                const int num,                                                                //
                const int pad,                                                                //
                const PixelDrawingInterface::Color& color = PixelDrawingInterface::WHITE());  //

void drawTriangleSelector(const PixelDrawingInterface& drawer,        //
                          const int x,                                //
                          const int y,                                //
                          const int size,                             //
                          const PixelDrawingInterface::Color& color,  //
                          const bool right);                          //

void drawUpArrow(const PixelDrawingInterface& drawer, const int x, const int y);

}  // namespace nestris_x86
