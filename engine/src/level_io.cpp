#include "ember/level_io.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <fstream>
#include <sstream>

namespace ember {

using json = nlohmann::ordered_json;   // keeps keys in the order we write them -> readable files

namespace {

constexpr int kFormatVersion = 1;

constexpr std::array<const char*, 7> kShapeNames = {"Box", "Circle", "Spikes", "Hero", "Flag", "Cloud", "Blob"};

std::string shape_name(Shape s) { return kShapeNames[static_cast<std::size_t>(s)]; }

Shape shape_from_name(const std::string& n) {
    for (std::size_t i = 0; i < kShapeNames.size(); ++i)
        if (n == kShapeNames[i]) return static_cast<Shape>(i);
    throw std::runtime_error("unknown shape '" + n + "'");
}

json entity_to_json(const Entity& e) {
    json j;
    j["id"] = e.id;
    j["name"] = e.name;
    j["transform"] = {{"x", e.transform.pos.x}, {"y", e.transform.pos.y},
                      {"w", e.transform.size.x}, {"h", e.transform.size.y}};
    if (e.sprite) {
        const Rgba& c = e.sprite->color;
        j["sprite"] = {{"shape", shape_name(e.sprite->shape)}, {"color", {c.r, c.g, c.b, c.a}},
                       {"layer", e.sprite->layer}};
    }
    if (e.collider) j["collider"] = {{"solid", e.collider->solid}};
    if (e.body)     j["body"] = {{"gravity_scale", e.body->gravity_scale}};
    if (e.player)   j["player"] = {{"move_speed", e.player->move_speed}, {"jump_speed", e.player->jump_speed},
                                   {"coyote_time", e.player->coyote_time}};
    if (e.patrol)   j["patrol"] = {{"min_x", e.patrol->min_x}, {"max_x", e.patrol->max_x}, {"speed", e.patrol->speed}};
    if (e.pickup)   j["pickup"] = {{"value", e.pickup->value}};
    if (e.hazard)   j["hazard"] = json::object();
    if (e.goal)     j["goal"] = json::object();
    return j;
}

Entity entity_from_json(const json& j) {
    Entity e;
    e.id = j.at("id").get<EntityId>();
    e.name = j.value("name", "Entity");
    const json& t = j.at("transform");
    e.transform.pos = {t.at("x").get<float>(), t.at("y").get<float>()};
    e.transform.size = {t.at("w").get<float>(), t.at("h").get<float>()};

    // j.value(key, default) means old files missing a newer field still load.
    if (j.contains("sprite")) {
        const json& s = j["sprite"];
        Sprite sp;
        sp.shape = shape_from_name(s.value("shape", "Box"));
        const auto c = s.value("color", std::vector<int>{255, 255, 255, 255});
        if (c.size() != 4) throw std::runtime_error("sprite color must have 4 numbers");
        sp.color = {static_cast<std::uint8_t>(c[0]), static_cast<std::uint8_t>(c[1]),
                    static_cast<std::uint8_t>(c[2]), static_cast<std::uint8_t>(c[3])};
        sp.layer = s.value("layer", 0);
        e.sprite = sp;
    }
    if (j.contains("collider")) e.collider = Collider{j["collider"].value("solid", true)};
    if (j.contains("body")) { Body b; b.gravity_scale = j["body"].value("gravity_scale", 1.0f); e.body = b; }
    if (j.contains("player")) {
        PlayerController p;
        p.move_speed = j["player"].value("move_speed", p.move_speed);
        p.jump_speed = j["player"].value("jump_speed", p.jump_speed);
        p.coyote_time = j["player"].value("coyote_time", p.coyote_time);
        e.player = p;
    }
    if (j.contains("patrol")) {
        Patrol p;
        p.min_x = j["patrol"].value("min_x", p.min_x);
        p.max_x = j["patrol"].value("max_x", p.max_x);
        p.speed = j["patrol"].value("speed", p.speed);
        e.patrol = p;
    }
    if (j.contains("pickup")) e.pickup = Pickup{j["pickup"].value("value", 1)};
    if (j.contains("hazard")) e.hazard = Hazard{};
    if (j.contains("goal")) e.goal = Goal{};
    return e;
}

}  // namespace

std::string world_to_json(const World& world) {
    json root;
    root["format"] = "ember-level";
    root["version"] = kFormatVersion;
    root["next_id"] = world.next_id();
    root["entities"] = json::array();
    for (const auto& e : world.entities()) root["entities"].push_back(entity_to_json(e));
    return root.dump(2);
}

std::optional<World> world_from_json(const std::string& text, std::string& error) {
    try {
        const json root = json::parse(text);
        if (root.value("format", "") != "ember-level") { error = "not an Ember level file"; return std::nullopt; }
        const int version = root.value("version", 0);
        if (version > kFormatVersion) {
            error = "level was saved by a newer Ember (version " + std::to_string(version) + ")";
            return std::nullopt;
        }
        World world;
        for (const json& je : root.at("entities")) {
            Entity e = entity_from_json(je);
            if (e.id == kNoEntity || world.find(e.id)) {
                error = "duplicate or missing entity id " + std::to_string(e.id);
                return std::nullopt;
            }
            world.insert_at(world.size(), e);
        }
        world.set_next_id(std::max(world.next_id(), root.value("next_id", EntityId{1})));
        return world;
    } catch (const std::exception& ex) {
        error = std::string("invalid level: ") + ex.what();
        return std::nullopt;
    }
}

bool save_level(const std::string& path, const World& world, std::string& error) {
    std::ofstream out(path);
    if (!out) { error = "cannot open '" + path + "' for writing"; return false; }
    out << world_to_json(world) << '\n';
    if (!out) { error = "failed while writing '" + path + "'"; return false; }
    return true;
}

std::optional<World> load_level(const std::string& path, std::string& error) {
    std::ifstream in(path);
    if (!in) { error = "cannot open '" + path + "'"; return std::nullopt; }
    std::stringstream ss;
    ss << in.rdbuf();
    return world_from_json(ss.str(), error);
}

}  // namespace ember
