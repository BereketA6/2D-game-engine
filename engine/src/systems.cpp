#include "ember/systems.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ember {

GameState begin_play(const World& world) {
    GameState s;
    float lowest = 0.0f;
    for (const auto& e : world.entities()) {
        if (e.pickup) ++s.coins_total;
        lowest = std::max(lowest, e.transform.rect().bottom());
    }
    s.kill_y = lowest + 400.0f;   // fall well below the lowest thing in the level = dead
    return s;
}

void step(World& world, const Input& input, GameState& state, float dt) {
    if (state.status != GameStatus::Playing) return;   // frozen after win / death
    state.time += dt;
    player_control_system(world, input, dt);
    patrol_system(world, dt);
    physics_system(world, dt);
    trigger_system(world, state);
}

// ---------------------------------------------------------------------------

void player_control_system(World& world, const Input& input, float dt) {
    for (auto& e : world.entities()) {
        if (!e.player || !e.body) continue;
        PlayerController& pc = *e.player;
        Body& body = *e.body;

        const float dir = (input.right ? 1.0f : 0.0f) - (input.left ? 1.0f : 0.0f);
        body.velocity.x = dir * pc.move_speed;

        // "Coyote time": for a split second after running off a ledge you can
        // still jump. Players never notice it -- but they notice its absence.
        if (body.grounded) pc.coyote_timer = pc.coyote_time;
        else               pc.coyote_timer = std::max(0.0f, pc.coyote_timer - dt);

        if (input.jump_pressed && pc.coyote_timer > 0.0f) {
            body.velocity.y = -pc.jump_speed;   // negative = up
            pc.coyote_timer = 0.0f;             // no double jumps
            body.grounded = false;
        }
    }
}

void patrol_system(World& world, float dt) {
    auto& ents = world.entities();
    for (auto& e : ents) {
        if (!e.patrol) continue;
        Patrol& p = *e.patrol;
        const float old_x = e.transform.pos.x;

        e.transform.pos.x += static_cast<float>(p.direction) * p.speed * dt;
        if (e.transform.pos.x >= p.max_x) { e.transform.pos.x = p.max_x; p.direction = -1; }
        if (e.transform.pos.x <= p.min_x) { e.transform.pos.x = p.min_x; p.direction = 1; }

        // A solid patrolling entity is a moving platform: carry anything
        // standing on top of it, otherwise the player slides off.
        const float dx = e.transform.pos.x - old_x;
        if (!e.collider || !e.collider->solid || dx == 0.0f) continue;
        const Rect plat = e.transform.rect();
        for (auto& rider : ents) {
            if (!rider.body || !rider.body->grounded || &rider == &e) continue;
            const Rect r = rider.transform.rect();
            const bool on_top = std::abs(r.bottom() - plat.top()) < 1.5f;
            const bool horizontally_over = r.right() > plat.left() - dx && r.left() < plat.right() - dx;
            if (on_top && horizontally_over) rider.transform.pos.x += dx;
        }
    }
}

// Move along ONE axis, then push out of anything solid we ended up inside.
// Doing x and y separately is the classic trick that makes corners and
// sliding along walls "just work" for axis-aligned boxes.
static void move_axis(World& world, Entity& self, bool horizontal, float amount) {
    if (amount == 0.0f) return;
    Transform& t = self.transform;
    if (horizontal) t.pos.x += amount; else t.pos.y += amount;

    for (const auto& other : world.entities()) {
        if (&other == &self || !other.collider || !other.collider->solid) continue;
        const Rect a = t.rect();
        const Rect b = other.transform.rect();
        if (!a.overlaps(b)) continue;

        Body& body = *self.body;
        if (horizontal) {
            t.pos.x = amount > 0 ? b.left() - t.size.x : b.right();   // stop at the wall
            body.velocity.x = 0.0f;
        } else if (amount > 0) {
            t.pos.y = b.top() - t.size.y;    // landed on top of something
            body.velocity.y = 0.0f;
            body.grounded = true;
        } else {
            t.pos.y = b.bottom();            // bonked our head
            body.velocity.y = 0.0f;
        }
    }
}

void physics_system(World& world, float dt) {
    for (auto& e : world.entities()) {
        if (!e.body) continue;
        Body& body = *e.body;

        body.velocity.y = std::min(body.velocity.y + kGravity * body.gravity_scale * dt, kMaxFallSpeed);
        body.grounded = false;   // re-detected below if we land on something

        move_axis(world, e, true, body.velocity.x * dt);
        move_axis(world, e, false, body.velocity.y * dt);
    }
}

void trigger_system(World& world, GameState& state) {
    Entity* player = world.find_player();
    if (!player) return;
    const Rect pr = player->transform.rect();

    std::vector<EntityId> collected;   // never erase from a vector while looping over it
    for (const auto& e : world.entities()) {
        if (&e == player || !pr.overlaps(e.transform.rect())) continue;
        if (e.pickup) { state.score += e.pickup->value; collected.push_back(e.id); }
        if (e.hazard) state.status = GameStatus::Dead;
        if (e.goal && state.status == GameStatus::Playing) state.status = GameStatus::Won;
    }
    for (EntityId id : collected) world.remove(id);

    if (state.status == GameStatus::Playing && pr.top() > state.kill_y) state.status = GameStatus::Dead;
}

}  // namespace ember
