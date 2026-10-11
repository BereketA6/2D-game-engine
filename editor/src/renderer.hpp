#pragma once
// Turns entity DATA into PICTURES. The engine only knows "this entity has a
// Sprite with shape Hero and colour orange"; this file decides what a Hero
// looks like. Everything is drawn from simple shapes, so there are no image
// files to load -- swap in textures later as an exercise.

#include "ember/entity.hpp"
#include "ember/world.hpp"

#include "raylib.h"

namespace editor {

Color to_color(ember::Rgba c);

// Sky gradient, drawn in screen space before the camera is applied.
void draw_sky(int width, int height);

// Editor grid lines covering the visible part of the world.
void draw_grid(ember::Rect visible, float spacing);

// Every entity, sorted by sprite layer (background first). `time` drives
// small animations like spinning coins.
void draw_world(const ember::World& world, float time);

}  // namespace editor
