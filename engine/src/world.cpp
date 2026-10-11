#include "ember/world.hpp"

#include <algorithm>

namespace ember {

EntityId World::create(Entity e) {
    e.id = next_id_++;
    entities_.push_back(std::move(e));
    return entities_.back().id;
}

void World::insert_at(std::size_t index, const Entity& e) {
    index = std::min(index, entities_.size());
    entities_.insert(entities_.begin() + static_cast<std::ptrdiff_t>(index), e);
    next_id_ = std::max(next_id_, e.id + 1);   // never hand this id out again
}

bool World::remove(EntityId id) {
    auto it = std::find_if(entities_.begin(), entities_.end(), [id](const Entity& e) { return e.id == id; });
    if (it == entities_.end()) return false;
    entities_.erase(it);
    return true;
}

Entity* World::find(EntityId id) {
    for (auto& e : entities_) if (e.id == id) return &e;
    return nullptr;
}

const Entity* World::find(EntityId id) const {
    for (const auto& e : entities_) if (e.id == id) return &e;
    return nullptr;
}

std::size_t World::index_of(EntityId id) const {
    for (std::size_t i = 0; i < entities_.size(); ++i) if (entities_[i].id == id) return i;
    return entities_.size();
}

Entity* World::find_player() {
    for (auto& e : entities_) if (e.player) return &e;
    return nullptr;
}

}  // namespace ember
