#include "test_framework.hpp"

#include "ember/prefabs.hpp"
#include "ember/systems.hpp"

using namespace ember;

namespace {

bool solid_at(const World& w, Vec2 p) {
    for (const auto& e : w.entities())
        if (e.collider && e.collider->solid && e.transform.rect().contains(p)) return true;
    return false;
}

// A very simple "AI player": hold right, and jump when there's a gap,
// a wall, or a hazard just ahead. If this bot can finish the demo level,
// the level is beatable and the physics + rules hang together.
Input bot_input(const World& w, int& cooldown) {
    Input in;
    in.right = true;
    const Entity* p = nullptr;
    for (const auto& e : w.entities()) if (e.player) p = &e;
    if (!p || !p->body->grounded) return in;
    if (cooldown > 0) { --cooldown; return in; }

    const Rect r = p->transform.rect();
    // A "gap" is a pit with no floor below. A step down with ground under it
    // is just a drop -- walking off is safer than jumping off.
    bool floor_below_ahead = false;
    for (float dy = 4; dy <= 140 && !floor_below_ahead; dy += 8)
        floor_below_ahead = solid_at(w, {r.right() + 20, r.bottom() + dy});
    const bool gap_ahead  = !floor_below_ahead;
    const bool wall_ahead = solid_at(w, {r.right() + 24, r.bottom() - 4});
    bool hazard_ahead = false;
    for (const auto& e : w.entities()) {
        if (!e.hazard) continue;
        const Rect h = e.transform.rect();
        const float dx = h.left() - r.right();
        if (dx > -4 && dx < 70 && h.top() < r.bottom() + 8 && h.bottom() > r.top()) hazard_ahead = true;
    }
    if (gap_ahead || wall_ahead || hazard_ahead) { in.jump_pressed = true; cooldown = 10; }
    return in;
}

}  // namespace

TEST(bot_can_beat_the_demo_level) {
    World w = make_demo_level();
    GameState s = begin_play(w);
    int cooldown = 0;
    for (int i = 0; i < 60 * 60 && s.status == GameStatus::Playing; ++i) step(w, bot_input(w, cooldown), s);
    std::printf("    bot result: %s in %.1fs with %d/%d coins\n",
                s.status == GameStatus::Won ? "WON" : (s.status == GameStatus::Dead ? "DIED" : "TIMED OUT"),
                static_cast<double>(s.time), s.score, s.coins_total);
    CHECK(s.status == GameStatus::Won);
    CHECK(s.score > 0);
}

// Play-In-Editor in miniature: copy the world, play on the copy, throw it
// away. The level being edited must come back exactly as it was.
TEST(play_in_editor_restores_the_level) {
    World editing = make_demo_level();
    const World snapshot = editing;

    World playing = editing;                   // what the editor does on "Play"
    GameState s = begin_play(playing);
    Input right; right.right = true;
    for (int i = 0; i < 300; ++i) step(playing, right, s);

    CHECK(!(playing == snapshot));             // the game really changed things
    CHECK(editing == snapshot);                // ...but never the level being edited
}

TEST(simulation_is_deterministic) {
    auto run = [] {
        World w = make_demo_level();
        GameState s = begin_play(w);
        int cooldown = 0;
        for (int i = 0; i < 1200; ++i) step(w, bot_input(w, cooldown), s);
        return w;
    };
    CHECK(run() == run());                     // same inputs -> bit-identical world
}
