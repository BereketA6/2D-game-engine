#pragma once
// Undo/redo with the COMMAND PATTERN -- how every real editor does it
// (Unreal calls them "transactions").
//
// Rule: the editor never changes the level directly. Every edit is wrapped
// in a Command object that knows how to apply() itself AND how to revert()
// itself. The History keeps two stacks:
//
//     undo stack: [create coin] [move player] [delete enemy]   <- most recent on top
//     redo stack: []
//
//   Ctrl+Z: pop the undo stack, revert() it, push it onto the redo stack.
//   Ctrl+Y: pop the redo stack, apply() it again, push it back onto undo.
//   Any NEW edit clears the redo stack (you've branched off history).
//
// Only three commands are needed for a whole editor, because ModifyEntity
// stores a full before/after copy of one entity -- so it covers moving,
// resizing, renaming, recolouring, adding or removing components... anything.

#include "ember/world.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace ember {

class Command {
public:
    virtual ~Command() = default;
    virtual void apply(World& world) = 0;
    virtual void revert(World& world) = 0;
    virtual std::string label() const = 0;   // shown in the History panel
};

class CreateEntityCommand final : public Command {
public:
    explicit CreateEntityCommand(Entity e, std::size_t index = SIZE_MAX) : entity_(std::move(e)), index_(index) {}
    void apply(World& world) override;
    void revert(World& world) override;
    std::string label() const override { return "Create " + entity_.name; }
    EntityId id() const { return entity_.id; }

private:
    Entity entity_;        // gets its id on first apply(), then keeps it forever
    std::size_t index_;
};

class DeleteEntityCommand final : public Command {
public:
    explicit DeleteEntityCommand(EntityId id) : id_(id) {}
    void apply(World& world) override;
    void revert(World& world) override;
    std::string label() const override { return "Delete " + saved_.name; }

private:
    EntityId id_;
    Entity saved_;         // remembered so revert() can bring it back
    std::size_t index_ = 0;
};

class ModifyEntityCommand final : public Command {
public:
    ModifyEntityCommand(Entity before, Entity after, std::string what)
        : before_(std::move(before)), after_(std::move(after)), what_(std::move(what)) {}
    void apply(World& world) override;
    void revert(World& world) override;
    std::string label() const override { return what_ + " " + after_.name; }

private:
    Entity before_, after_;
    std::string what_;
};

class History {
public:
    // Apply a command and record it.
    void execute(std::unique_ptr<Command> cmd, World& world);

    // Record a command whose effect is ALREADY visible in the world. Used for
    // continuous edits (dragging an object, scrubbing a slider): the editor
    // changes the world live every frame for instant feedback, then records
    // ONE command when the mouse is released -- so one Ctrl+Z undoes the
    // whole drag, not 60 tiny steps.
    void push_applied(std::unique_ptr<Command> cmd);

    bool undo(World& world);
    bool redo(World& world);
    bool can_undo() const { return !undo_.empty(); }
    bool can_redo() const { return !redo_.empty(); }
    void clear();

    std::size_t undo_count() const { return undo_.size(); }
    std::size_t redo_count() const { return redo_.size(); }
    std::string undo_label(std::size_t i) const { return undo_[i]->label(); }   // 0 = oldest

    // Increments on every change. The editor compares it with the value at
    // the last save to show "unsaved changes" (the * in the title).
    std::size_t version() const { return version_; }

    static constexpr std::size_t kMaxHistory = 256;

private:
    void record(std::unique_ptr<Command> cmd);
    std::vector<std::unique_ptr<Command>> undo_, redo_;
    std::size_t version_ = 0;
};

}  // namespace ember
