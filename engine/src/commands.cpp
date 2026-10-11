#include "ember/commands.hpp"

namespace ember {

// --- CreateEntity -------------------------------------------------------------

void CreateEntityCommand::apply(World& world) {
    if (entity_.id == kNoEntity) {
        // First time: let the world assign a new id, then remember it so a
        // redo recreates the entity with the SAME id (other commands in the
        // history may refer to it).
        entity_.id = world.create(entity_);
        if (index_ != SIZE_MAX && index_ < world.size() - 1) {
            Entity copy = *world.find(entity_.id);
            world.remove(entity_.id);
            world.insert_at(index_, copy);
        }
    } else {
        world.insert_at(index_ == SIZE_MAX ? world.size() : index_, entity_);
    }
}

void CreateEntityCommand::revert(World& world) {
    if (index_ == SIZE_MAX) index_ = world.index_of(entity_.id);   // remember slot for redo
    world.remove(entity_.id);
}

// --- DeleteEntity -------------------------------------------------------------

void DeleteEntityCommand::apply(World& world) {
    if (const Entity* e = world.find(id_)) {
        saved_ = *e;
        index_ = world.index_of(id_);
        world.remove(id_);
    }
}

void DeleteEntityCommand::revert(World& world) {
    world.insert_at(index_, saved_);   // same id, same position in the Outliner
}

// --- ModifyEntity -------------------------------------------------------------

void ModifyEntityCommand::apply(World& world) {
    if (Entity* e = world.find(after_.id)) *e = after_;
}

void ModifyEntityCommand::revert(World& world) {
    if (Entity* e = world.find(before_.id)) *e = before_;
}

// --- History ------------------------------------------------------------------

void History::execute(std::unique_ptr<Command> cmd, World& world) {
    cmd->apply(world);
    record(std::move(cmd));
}

void History::push_applied(std::unique_ptr<Command> cmd) { record(std::move(cmd)); }

void History::record(std::unique_ptr<Command> cmd) {
    undo_.push_back(std::move(cmd));
    redo_.clear();   // a new edit makes the old "future" unreachable
    if (undo_.size() > kMaxHistory) undo_.erase(undo_.begin());
    ++version_;
}

bool History::undo(World& world) {
    if (undo_.empty()) return false;
    std::unique_ptr<Command> cmd = std::move(undo_.back());
    undo_.pop_back();
    cmd->revert(world);
    redo_.push_back(std::move(cmd));
    ++version_;
    return true;
}

bool History::redo(World& world) {
    if (redo_.empty()) return false;
    std::unique_ptr<Command> cmd = std::move(redo_.back());
    redo_.pop_back();
    cmd->apply(world);
    undo_.push_back(std::move(cmd));
    ++version_;
    return true;
}

void History::clear() {
    undo_.clear();
    redo_.clear();
    ++version_;
}

}  // namespace ember
