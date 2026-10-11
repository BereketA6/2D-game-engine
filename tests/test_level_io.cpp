#include "test_framework.hpp"

#include "ember/level_io.hpp"
#include "ember/prefabs.hpp"

#include <cstdio>

using namespace ember;

TEST(save_load_round_trip_is_exact) {
    const World w = make_demo_level();
    std::string err;
    auto loaded = world_from_json(world_to_json(w), err);
    CHECK(loaded.has_value());
    if (loaded) {
        CHECK(*loaded == w);                   // every entity, component, and the next id
        CHECK_EQ(loaded->size(), w.size());
    }
}

TEST(save_load_through_a_real_file) {
    const World w = make_demo_level();
    const std::string path = "ember_test_level.json";
    std::string err;
    CHECK(save_level(path, w, err));
    auto loaded = load_level(path, err);
    CHECK(loaded.has_value() && *loaded == w);
    std::remove(path.c_str());
}

TEST(next_id_survives_save_so_ids_never_repeat) {
    World w;
    EntityId a = w.create(make_prefab(Prefab::Coin, {0, 0}));
    EntityId b = w.create(make_prefab(Prefab::Coin, {0, 0}));
    w.remove(b);                               // highest id deleted...
    std::string err;
    auto loaded = world_from_json(world_to_json(w), err);
    EntityId c = loaded->create(make_prefab(Prefab::Coin, {0, 0}));
    CHECK(c != a && c != b);                   // ...but never reused after reload
}

TEST(rejects_bad_files_with_a_reason) {
    std::string err;
    CHECK(!world_from_json("not json at all", err));
    CHECK(err.find("invalid level") != std::string::npos);

    err.clear();
    CHECK(!world_from_json(R"({"format":"something-else","entities":[]})", err));
    CHECK(err == "not an Ember level file");

    err.clear();
    CHECK(!world_from_json(R"({"format":"ember-level","version":99,"entities":[]})", err));
    CHECK(err.find("newer") != std::string::npos);

    err.clear();
    CHECK(!world_from_json(R"({"format":"ember-level","version":1,"entities":[
        {"id":1,"transform":{"x":0,"y":0,"w":1,"h":1}},
        {"id":1,"transform":{"x":0,"y":0,"w":1,"h":1}}]})", err));
    CHECK(err.find("duplicate") != std::string::npos);

    err.clear();
    CHECK(!world_from_json(R"({"format":"ember-level","version":1,"entities":[
        {"id":1,"transform":{"x":0,"y":0,"w":1,"h":1},"sprite":{"shape":"Dragon"}}]})", err));
    CHECK(err.find("Dragon") != std::string::npos);
}

TEST(missing_optional_fields_use_defaults) {
    std::string err;
    auto w = world_from_json(R"({"format":"ember-level","version":1,"entities":[
        {"id":7,"transform":{"x":5,"y":6,"w":7,"h":8},"player":{},"body":{}}]})", err);
    CHECK(w.has_value());
    if (w) {
        const Entity* e = w->find(7);
        CHECK(e && e->player && e->player->move_speed == PlayerController{}.move_speed);
        CHECK(e && e->name == "Entity");
        CHECK_EQ(w->next_id(), 8u);            // derived from the highest id when not stored
    }
}
