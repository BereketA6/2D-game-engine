#include "test_framework.hpp"

#include "ember/commands.hpp"
#include "ember/prefabs.hpp"

#include <random>

using namespace ember;

TEST(create_undo_redo_keeps_same_id) {
    World w; History h;
    auto cmd = std::make_unique<CreateEntityCommand>(make_prefab(Prefab::Coin, {10, 10}));
    CreateEntityCommand* raw = cmd.get();
    h.execute(std::move(cmd), w);
    const EntityId id = raw->id();
    CHECK(w.find(id) != nullptr);
    CHECK(h.undo(w));
    CHECK(w.find(id) == nullptr);
    CHECK(h.redo(w));
    CHECK(w.find(id) != nullptr);              // same id after redo
    CHECK_EQ(w.size(), 1u);
}

TEST(delete_undo_restores_exact_position_in_list) {
    World w = make_demo_level();
    const World original = w;
    History h;
    const EntityId victim = w.entities()[5].id;
    h.execute(std::make_unique<DeleteEntityCommand>(victim), w);
    CHECK(w.find(victim) == nullptr);
    h.undo(w);
    CHECK(w == original);                      // identical, including order
}

TEST(modify_undo_redo) {
    World w; History h;
    EntityId id = w.create(make_prefab(Prefab::Platform, {0, 0}));
    Entity before = *w.find(id);
    Entity after = before;
    after.transform.pos = {300, 200};
    after.name = "Moved";
    h.execute(std::make_unique<ModifyEntityCommand>(before, after, "Move"), w);
    CHECK(w.find(id)->transform.pos == (Vec2{300, 200}));
    h.undo(w);
    CHECK(*w.find(id) == before);
    h.redo(w);
    CHECK(*w.find(id) == after);
}

TEST(new_edit_clears_redo) {
    World w; History h;
    h.execute(std::make_unique<CreateEntityCommand>(make_prefab(Prefab::Coin, {0, 0})), w);
    h.undo(w);
    CHECK(h.can_redo());
    h.execute(std::make_unique<CreateEntityCommand>(make_prefab(Prefab::Enemy, {0, 0})), w);
    CHECK(!h.can_redo());
}

TEST(push_applied_records_a_drag_as_one_step) {
    World w; History h;
    EntityId id = w.create(make_prefab(Prefab::Coin, {0, 0}));
    const Entity before = *w.find(id);
    for (int frame = 0; frame < 60; ++frame) w.find(id)->transform.pos.x += 5;   // live drag
    h.push_applied(std::make_unique<ModifyEntityCommand>(before, *w.find(id), "Move"));
    CHECK_EQ(h.undo_count(), 1u);
    h.undo(w);
    CHECK(*w.find(id) == before);              // one undo reverts the whole drag
}

// The strongest test: hundreds of random edits, then undo ALL of them and
// check we're back at the exact original level; redo all and check we
// reach the exact final level. Any command with a subtle bug breaks this.
TEST(random_edit_sequence_fully_undoes_and_redoes) {
    World w = make_demo_level();
    const World original = w;
    History h;
    std::mt19937 rng(2024);
    std::uniform_int_distribution<int> op(0, 2), coord(0, 2000);

    const int kEdits = 200;   // below History::kMaxHistory so nothing is dropped
    for (int i = 0; i < kEdits; ++i) {
        const int o = op(rng);
        if (o == 0 || w.size() == 0) {
            const Prefab p = kAllPrefabs[static_cast<std::size_t>(coord(rng)) % kAllPrefabs.size()];
            h.execute(std::make_unique<CreateEntityCommand>(make_prefab(p, {float(coord(rng)), float(coord(rng))})), w);
        } else {
            const Entity& target = w.entities()[static_cast<std::size_t>(coord(rng)) % w.size()];
            if (o == 1) {
                h.execute(std::make_unique<DeleteEntityCommand>(target.id), w);
            } else {
                Entity after = target;
                after.transform.pos.x = float(coord(rng));
                after.transform.size.y += 4;
                h.execute(std::make_unique<ModifyEntityCommand>(target, after, "Edit"), w);
            }
        }
    }
    const World final_world = w;
    while (h.undo(w)) {}
    CHECK(w.entities() == original.entities());
    while (h.redo(w)) {}
    CHECK(w.entities() == final_world.entities());
}

TEST(history_is_capped) {
    World w; History h;
    for (std::size_t i = 0; i < History::kMaxHistory + 50; ++i)
        h.execute(std::make_unique<CreateEntityCommand>(make_prefab(Prefab::Coin, {0, 0})), w);
    CHECK_EQ(h.undo_count(), History::kMaxHistory);
}
