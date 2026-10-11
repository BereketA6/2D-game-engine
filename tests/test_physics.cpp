#include "test_framework.hpp"

#include "ember/prefabs.hpp"
#include "ember/systems.hpp"

using namespace ember;

namespace {
EntityId add_floor(World& w, float x, float y, float width, float height = 40) {
    Entity f = make_prefab(Prefab::Platform, {x, y});
    f.transform.size = {width, height};
    return w.create(f);
}
EntityId add_player(World& w, float x, float y) { return w.create(make_prefab(Prefab::Player, {x, y})); }

void run(World& w, GameState& s, const Input& in, int steps) {
    for (int i = 0; i < steps; ++i) step(w, in, s);
}
}  // namespace

TEST(player_falls_and_lands_on_platform) {
    World w;
    add_floor(w, 0, 300, 400);
    EntityId p = add_player(w, 100, 100);
    GameState s = begin_play(w);
    run(w, s, {}, 120);
    const Entity& pl = *w.find(p);
    CHECK_NEAR(pl.transform.rect().bottom(), 300.0, 0.001);   // resting exactly on top
    CHECK(pl.body->grounded);
    CHECK_NEAR(pl.body->velocity.y, 0.0, 0.001);
}

TEST(walls_block_horizontal_movement) {
    World w;
    add_floor(w, 0, 300, 600);
    add_floor(w, 300, 200, 40, 100);          // a wall standing on the floor
    EntityId p = add_player(w, 100, 260);
    GameState s = begin_play(w);
    Input right; right.right = true;
    run(w, s, right, 180);                    // 3 seconds of pushing into the wall
    CHECK_NEAR(w.find(p)->transform.rect().right(), 300.0, 0.001);
}

TEST(jump_height_matches_physics) {
    World w;
    add_floor(w, 0, 300, 400);
    EntityId p = add_player(w, 100, 260);
    GameState s = begin_play(w);
    run(w, s, {}, 10);                        // settle on the floor
    const float start = w.find(p)->transform.pos.y;

    Input jump; jump.jump_pressed = true;
    step(w, jump, s);
    float highest = start;
    for (int i = 0; i < 90; ++i) {
        step(w, {}, s);
        highest = std::min(highest, w.find(p)->transform.pos.y);
    }
    // Ideal: v^2 / 2g = 560^2 / (2*1500) = 104.5. A discrete timestep loses a little.
    const double height = start - highest;
    std::printf("    jump height: %.1f units (ideal 104.5)\n", height);
    CHECK(height > 95.0 && height < 106.0);
}

TEST(no_jumping_in_mid_air) {
    World w;
    EntityId p = add_player(w, 100, 100);    // no floor: falling forever
    GameState s = begin_play(w);
    run(w, s, {}, 30);
    Input jump; jump.jump_pressed = true;
    step(w, jump, s);
    CHECK(w.find(p)->body->velocity.y > 0);   // still falling, the jump was refused
}

TEST(coyote_time_allows_late_jump) {
    // Walk off a ledge, then press jump a few frames later.
    auto try_jump_after = [](int frames_off_ledge) {
        World w;
        add_floor(w, 0, 300, 200);
        EntityId p = add_player(w, 150, 260);
        GameState s = begin_play(w);
        run(w, s, {}, 5);
        Input right; right.right = true;
        while (w.find(p)->transform.pos.x < 200) step(w, right, s);   // until off the edge
        run(w, s, right, frames_off_ledge);
        Input jump; jump.right = true; jump.jump_pressed = true;
        step(w, jump, s);
        return w.find(p)->body->velocity.y < 0;   // moving up = jump accepted
    };
    CHECK(try_jump_after(3));    // 3/60 s = 0.05 s, inside the 0.10 s window
    CHECK(!try_jump_after(12));  // 0.20 s: too late
}

TEST(patrol_stays_in_bounds_and_turns) {
    World w;
    Entity e = make_prefab(Prefab::Enemy, {100, 0});
    e.patrol = Patrol{50, 150, 100.0f, 1};
    EntityId id = w.create(e);
    GameState s = begin_play(w);
    bool went_left = false;
    for (int i = 0; i < 600; ++i) {
        step(w, {}, s);
        const Entity& en = *w.find(id);
        CHECK(en.transform.pos.x >= 50.0f && en.transform.pos.x <= 150.0f);
        if (en.patrol->direction < 0) went_left = true;
    }
    CHECK(went_left);
}

TEST(moving_platform_carries_player) {
    World w;
    Entity mover = make_prefab(Prefab::MovingPlatform, {100, 300});
    mover.patrol = Patrol{100, 400, 60.0f, 1};
    EntityId m = w.create(mover);
    EntityId p = add_player(w, 140, 250);
    GameState s = begin_play(w);
    run(w, s, {}, 30);                        // land on it
    const float offset_before = w.find(p)->transform.pos.x - w.find(m)->transform.pos.x;
    run(w, s, {}, 120);                       // ride for 2 seconds
    const float offset_after = w.find(p)->transform.pos.x - w.find(m)->transform.pos.x;
    CHECK(w.find(m)->transform.pos.x > 200);  // the platform really moved...
    CHECK_NEAR(offset_after, offset_before, 0.5);   // ...and the player moved with it
    CHECK(w.find(p)->body->grounded);
}

TEST(coin_pickup_scores_and_disappears) {
    World w;
    add_floor(w, 0, 300, 400);
    add_player(w, 100, 260);
    EntityId coin = w.create(make_prefab(Prefab::Coin, {150, 270}));
    GameState s = begin_play(w);
    CHECK_EQ(s.coins_total, 1);
    Input right; right.right = true;
    run(w, s, right, 30);
    CHECK_EQ(s.score, 1);
    CHECK(w.find(coin) == nullptr);
}

TEST(hazard_kills_and_game_freezes) {
    World w;
    add_floor(w, 0, 300, 400);
    EntityId p = add_player(w, 100, 260);
    w.create(make_prefab(Prefab::Spikes, {160, 280}));
    GameState s = begin_play(w);
    Input right; right.right = true;
    run(w, s, right, 60);
    CHECK(s.status == GameStatus::Dead);
    const Vec2 frozen = w.find(p)->transform.pos;
    run(w, s, right, 60);                     // nothing moves after death
    CHECK(w.find(p)->transform.pos == frozen);
}

TEST(reaching_goal_wins) {
    World w;
    add_floor(w, 0, 300, 400);
    add_player(w, 100, 260);
    w.create(make_prefab(Prefab::Goal, {200, 204}));
    GameState s = begin_play(w);
    Input right; right.right = true;
    run(w, s, right, 60);
    CHECK(s.status == GameStatus::Won);
}

TEST(falling_out_of_the_world_kills) {
    World w;
    add_floor(w, 0, 300, 100);
    add_player(w, 20, 260);
    GameState s = begin_play(w);
    Input right; right.right = true;
    run(w, s, right, 300);
    CHECK(s.status == GameStatus::Dead);
}
