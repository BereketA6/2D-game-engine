#pragma once
// Systems are the "verbs" of the engine. Each one is a function that loops
// over every entity that has the components it cares about:
//
//   player_control  : PlayerController + Body  -> sets velocity from input
//   patrol          : Patrol                   -> walks back and forth
//   physics         : Body                     -> gravity + collide with solids
//   triggers        : player vs Pickup/Hazard/Goal overlaps -> game rules
//
// step() runs them in that order once per FIXED timestep (1/60 s). A fixed
// step means the game behaves identically on a 60 Hz laptop and a 144 Hz
// monitor -- the editor accumulates real frame time and calls step() as
// many times as needed. (Search "fix your timestep" -- a classic article.)

#include "ember/world.hpp"

namespace ember {

inline constexpr float kFixedDt = 1.0f / 60.0f;
inline constexpr float kGravity = 1500.0f;        // units / s^2, downwards
inline constexpr float kMaxFallSpeed = 900.0f;

// What the player is pressing this step. The editor fills this from the
// keyboard; tests fill it by hand -- the engine can't tell the difference.
struct Input {
    bool left = false;
    bool right = false;
    bool jump_pressed = false;   // true only on the step the key went down
};

enum class GameStatus { Playing, Won, Dead };

struct GameState {
    GameStatus status = GameStatus::Playing;
    int score = 0;
    int coins_total = 0;   // coins in the level when play started
    float time = 0.0f;     // seconds since play started
    float kill_y = 2000.0f; // falling below this line = death
};

// Call once when Play starts: counts coins, computes the fall-death line.
GameState begin_play(const World& world);

// Advance the whole simulation by exactly one fixed timestep.
void step(World& world, const Input& input, GameState& state, float dt = kFixedDt);

// Individual systems, exposed so tests can exercise them one at a time.
void player_control_system(World& world, const Input& input, float dt);
void patrol_system(World& world, float dt);
void physics_system(World& world, float dt);
void trigger_system(World& world, GameState& state);

}  // namespace ember
