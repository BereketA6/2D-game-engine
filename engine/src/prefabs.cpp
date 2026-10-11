#include "ember/prefabs.hpp"

namespace ember {

namespace {
constexpr Rgba kGrass{86, 140, 74, 255};
constexpr Rgba kWood{150, 108, 72, 255};
constexpr Rgba kHero{255, 138, 61, 255};
constexpr Rgba kGold{247, 201, 72, 255};
constexpr Rgba kEnemy{201, 79, 109, 255};
constexpr Rgba kSteel{184, 192, 204, 255};
constexpr Rgba kGoal{76, 195, 138, 255};
constexpr Rgba kCloud{255, 255, 255, 200};
}  // namespace

const char* prefab_name(Prefab p) {
    switch (p) {
        case Prefab::Platform:       return "Platform";
        case Prefab::Player:         return "Player";
        case Prefab::Coin:           return "Coin";
        case Prefab::Enemy:          return "Enemy";
        case Prefab::Spikes:         return "Spikes";
        case Prefab::MovingPlatform: return "Moving Platform";
        case Prefab::Goal:           return "Goal Flag";
        case Prefab::Cloud:          return "Cloud";
    }
    return "?";
}

const char* prefab_description(Prefab p) {
    switch (p) {
        case Prefab::Platform:       return "Solid ground. Transform + Sprite + Collider";
        case Prefab::Player:         return "You. Transform + Sprite + Body + PlayerController";
        case Prefab::Coin:           return "Collect for score. Transform + Sprite + Pickup";
        case Prefab::Enemy:          return "Walks back and forth; touching it kills you. Patrol + Hazard";
        case Prefab::Spikes:         return "Don't touch. Transform + Sprite + Hazard";
        case Prefab::MovingPlatform: return "Solid AND patrolling: carries the player. Collider + Patrol";
        case Prefab::Goal:           return "Reach it to win. Transform + Sprite + Goal";
        case Prefab::Cloud:          return "Decoration only: just Transform + Sprite";
    }
    return "";
}

Entity make_prefab(Prefab p, Vec2 pos) {
    Entity e;
    e.name = prefab_name(p);
    e.transform.pos = pos;
    switch (p) {
        case Prefab::Platform:
            e.transform.size = {192, 32};
            e.sprite = Sprite{Shape::Box, kGrass, 0};
            e.collider = Collider{true};
            break;
        case Prefab::Player:
            e.transform.size = {28, 40};
            e.sprite = Sprite{Shape::Hero, kHero, 5};
            e.body = Body{};
            e.player = PlayerController{};
            break;
        case Prefab::Coin:
            e.transform.size = {20, 20};
            e.sprite = Sprite{Shape::Circle, kGold, 3};
            e.pickup = Pickup{1};
            break;
        case Prefab::Enemy:
            e.transform.size = {34, 28};
            e.sprite = Sprite{Shape::Blob, kEnemy, 4};
            e.patrol = Patrol{pos.x - 96, pos.x + 96, 70.0f, 1};
            e.hazard = Hazard{};
            break;
        case Prefab::Spikes:
            e.transform.size = {64, 20};
            e.sprite = Sprite{Shape::Spikes, kSteel, 2};
            e.hazard = Hazard{};
            break;
        case Prefab::MovingPlatform:
            e.transform.size = {128, 20};
            e.sprite = Sprite{Shape::Box, kWood, 1};
            e.collider = Collider{true};
            e.patrol = Patrol{pos.x, pos.x + 192, 60.0f, 1};
            break;
        case Prefab::Goal:
            e.transform.size = {32, 96};
            e.sprite = Sprite{Shape::Flag, kGoal, 2};
            e.goal = Goal{};
            break;
        case Prefab::Cloud:
            e.transform.size = {120, 48};
            e.sprite = Sprite{Shape::Cloud, kCloud, -5};
            break;
    }
    return e;
}

World make_demo_level() {
    World w;
    auto add = [&](Prefab p, Vec2 pos, Vec2 size = {0, 0}, const char* name = nullptr) {
        Entity e = make_prefab(p, pos);
        if (size.x > 0) e.transform.size = size;
        if (name) e.name = name;
        return w.create(e);
    };

    add(Prefab::Cloud, {120, 90});
    add(Prefab::Cloud, {620, 60}, {150, 56});
    add(Prefab::Cloud, {1180, 110});
    add(Prefab::Cloud, {1760, 70}, {140, 52});

    add(Prefab::Platform, {0, 520}, {640, 80}, "Ground A");
    add(Prefab::Platform, {760, 520}, {640, 80}, "Ground B");
    add(Prefab::Platform, {1520, 520}, {800, 80}, "Ground C");
    add(Prefab::Platform, {880, 410}, {160, 24}, "Ledge");
    add(Prefab::Platform, {1700, 460}, {120, 60}, "Step");

    add(Prefab::Player, {64, 470});

    for (float x : {260.0f, 320.0f, 380.0f}) add(Prefab::Coin, {x, 470});
    add(Prefab::Coin, {690, 420}, {}, "Gap Coin");
    for (float x : {920.0f, 980.0f}) add(Prefab::Coin, {x, 370});
    add(Prefab::Coin, {1750, 410});
    add(Prefab::Coin, {2010, 430}, {}, "Spike Coin");   // grab it by jumping over the spikes

    // An enemy pacing Ground B, and spikes on Ground C.
    Entity enemy = make_prefab(Prefab::Enemy, {1150, 492});
    enemy.patrol = Patrol{1080, 1340, 70.0f, 1};
    w.create(enemy);
    add(Prefab::Spikes, {1990, 500});   // far enough past the Step to land safely

    // A bonus moving platform with a coin floating above it (optional route).
    Entity mover = make_prefab(Prefab::MovingPlatform, {1540, 380});
    mover.patrol = Patrol{1540, 1640, 50.0f, 1};
    w.create(mover);
    add(Prefab::Coin, {1600, 330}, {}, "Bonus Coin");

    add(Prefab::Goal, {2220, 424});
    return w;
}

}  // namespace ember
