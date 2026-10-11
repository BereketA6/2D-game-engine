#pragma once
// Prefabs ("Blueprints", in Unreal terms): ready-made combinations of
// components. The editor's Content Browser lists these; clicking one and
// then clicking in the viewport stamps a copy into the level.

#include "ember/world.hpp"

#include <array>

namespace ember {

enum class Prefab { Platform, Player, Coin, Enemy, Spikes, MovingPlatform, Goal, Cloud };

inline constexpr std::array<Prefab, 8> kAllPrefabs = {
    Prefab::Platform, Prefab::Player, Prefab::Coin, Prefab::Enemy,
    Prefab::Spikes, Prefab::MovingPlatform, Prefab::Goal, Prefab::Cloud};

const char* prefab_name(Prefab p);
const char* prefab_description(Prefab p);

// Builds an entity of that kind with its top-left corner at `pos`.
// The returned entity has no id yet -- the World assigns one on create().
Entity make_prefab(Prefab p, Vec2 pos);

// A small hand-built level so the editor never opens to an empty screen.
World make_demo_level();

}  // namespace ember
