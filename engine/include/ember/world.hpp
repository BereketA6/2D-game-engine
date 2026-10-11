#pragma once
// The World owns every entity in the level, in a stable order (the order
// the Outliner panel shows them in).
//
// It's a plain value type: `World copy = world;` duplicates the whole level.
// Play-In-Editor relies on that -- press Play, the editor keeps a copy of the
// world, the game mutates the live one, press Stop and the copy comes back.
//
// Lookup by id is a linear scan. For a few hundred entities that's faster
// than you'd think (cache-friendly vector); a big engine would add an
// id -> index hash map. Good exercise!

#include "ember/entity.hpp"

#include <cstddef>
#include <vector>

namespace ember {

class World {
public:
    // Adds an entity, assigning it a fresh unique id. Returns that id.
    EntityId create(Entity e);

    // Inserts an entity that ALREADY has an id at a specific position.
    // Used by undo/redo so a deleted entity comes back exactly where it was.
    void insert_at(std::size_t index, const Entity& e);

    // Removes the entity; returns false if no such id.
    bool remove(EntityId id);

    Entity* find(EntityId id);
    const Entity* find(EntityId id) const;
    std::size_t index_of(EntityId id) const;   // returns size() if not found

    std::vector<Entity>& entities() { return entities_; }
    const std::vector<Entity>& entities() const { return entities_; }
    std::size_t size() const { return entities_.size(); }

    // The id the next create() will hand out. Saved with the level so ids
    // never repeat, even across save/load.
    EntityId next_id() const { return next_id_; }
    void set_next_id(EntityId id) { next_id_ = id; }

    Entity* find_player();
    bool operator==(const World&) const = default;

private:
    std::vector<Entity> entities_;
    EntityId next_id_ = 1;
};

}  // namespace ember
