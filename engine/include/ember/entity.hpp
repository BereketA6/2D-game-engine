#pragma once
// An Entity is a "thing in the level" -- the same idea as an Actor in Unreal.
// Its behaviour comes from optional COMPONENTS attached to it:
//
//     Player   = Transform + Sprite + Body + PlayerController
//     Platform = Transform + Sprite + Collider(solid)
//     Coin     = Transform + Sprite + Pickup
//     Enemy    = Transform + Sprite + Patrol + Hazard
//
// Want a moving platform? Give a platform a Patrol component. Want a coin
// that kills you? Give it a Hazard. New behaviour comes from combining
// components, not from writing a new class -- that's the core idea behind
// Unreal's components and every Entity-Component System.
//
// Components are std::optional members: present or absent. That keeps the
// whole entity a plain value type, so copying a World is a real deep copy --
// which is exactly what Play-In-Editor and undo/redo need.

#include "ember/math.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace ember {

using EntityId = std::uint32_t;
inline constexpr EntityId kNoEntity = 0;

struct Rgba {
    std::uint8_t r = 255, g = 255, b = 255, a = 255;
    constexpr bool operator==(const Rgba&) const = default;
};

// --- Components -------------------------------------------------------------

// Where the entity is and how big it is. pos = top-left corner.
struct Transform {
    Vec2 pos;
    Vec2 size{32.0f, 32.0f};
    Rect rect() const { return {pos.x, pos.y, size.x, size.y}; }
    bool operator==(const Transform&) const = default;
};

// How to draw it. The engine never draws anything itself -- it only stores
// this data; the editor's renderer reads it (like a mesh/sprite component).
enum class Shape : std::uint8_t { Box, Circle, Spikes, Hero, Flag, Cloud, Blob };
struct Sprite {
    Shape shape = Shape::Box;
    Rgba color;
    int layer = 0;  // lower layers are drawn first (background)
    bool operator==(const Sprite&) const = default;
};

// Solid colliders block movement (floors, walls). Entities with Pickup,
// Hazard or Goal use their Transform rect as a trigger area instead.
struct Collider {
    bool solid = true;
    bool operator==(const Collider&) const = default;
};

// Makes an entity dynamic: it has velocity and falls under gravity.
struct Body {
    Vec2 velocity;
    float gravity_scale = 1.0f;
    bool grounded = false;   // runtime state, set by physics each step
    bool operator==(const Body&) const = default;
};

// Reads input and drives a Body. Only the player has one.
struct PlayerController {
    float move_speed = 230.0f;   // units per second
    float jump_speed = 560.0f;   // initial upward velocity
    float coyote_time = 0.10f;   // seconds you can still jump after walking off a ledge
    float coyote_timer = 0.0f;   // runtime state
    bool operator==(const PlayerController&) const = default;
};

// Walks back and forth between two x positions. Kinematic: it ignores
// gravity and collisions, like a moving platform or a simple enemy.
struct Patrol {
    float min_x = 0.0f;
    float max_x = 200.0f;   // the entity's LEFT edge stays within [min_x, max_x]
    float speed = 80.0f;
    int direction = 1;      // +1 right, -1 left (runtime state)
    bool operator==(const Patrol&) const = default;
};

struct Pickup { int value = 1; bool operator==(const Pickup&) const = default; };
struct Hazard { bool operator==(const Hazard&) const = default; };
struct Goal   { bool operator==(const Goal&) const = default; };

// --- The entity -------------------------------------------------------------

struct Entity {
    EntityId id = kNoEntity;
    std::string name = "Entity";
    Transform transform;

    std::optional<Sprite> sprite;
    std::optional<Collider> collider;
    std::optional<Body> body;
    std::optional<PlayerController> player;
    std::optional<Patrol> patrol;
    std::optional<Pickup> pickup;
    std::optional<Hazard> hazard;
    std::optional<Goal> goal;

    bool operator==(const Entity&) const = default;
};

}  // namespace ember
