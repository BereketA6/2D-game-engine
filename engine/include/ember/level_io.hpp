#pragma once
// Save / load levels as human-readable JSON.
//
//   {
//     "format": "ember-level", "version": 1, "next_id": 42,
//     "entities": [
//       { "id": 1, "name": "Player",
//         "transform": { "x": 64, "y": 300, "w": 28, "h": 40 },
//         "sprite": { "shape": "Hero", "color": [255, 196, 64, 255], "layer": 5 },
//         "body": {}, "player": { "move_speed": 230, "jump_speed": 560 } },
//       ...
//     ]
//   }
//
// Only components the entity actually has are written. Runtime-only state
// (velocity, grounded, coyote timer) is NOT saved: a level file describes
// how the level STARTS, not a frozen moment of gameplay.
//
// The "version" field matters more than it looks: when you add a feature
// that changes the format, bump it, and the loader can still read old files.

#include "ember/world.hpp"

#include <optional>
#include <string>

namespace ember {

std::string world_to_json(const World& world);

// On failure returns nullopt and writes a human-readable reason to `error`.
std::optional<World> world_from_json(const std::string& text, std::string& error);

bool save_level(const std::string& path, const World& world, std::string& error);
std::optional<World> load_level(const std::string& path, std::string& error);

}  // namespace ember
