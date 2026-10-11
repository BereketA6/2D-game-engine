// The editor's panels. Each draw_* function is one dockable window.
//
// The Details panel shows the most important ImGui + undo pattern in the
// project (see track_edit below): edit the entity live while a widget is
// being dragged/typed in, and record ONE undo command when the user lets go.

#include "editor_app.hpp"
#include "renderer.hpp"

#include "imgui_stdlib.h"   // ImGui::InputText for std::string

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>

using namespace ember;

namespace editor {

static constexpr const char* kPrefabPayload = "EMBER_PREFAB";

// ---------------------------------------------------------------------------
// Toolbar
// ---------------------------------------------------------------------------

void EditorApp::draw_toolbar() {
    ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    const bool playing = mode_ == Mode::Play;

    // Big Play / Stop button, coloured like Unreal's.
    ImGui::PushStyleColor(ImGuiCol_Button, playing ? ImVec4{0.70f, 0.22f, 0.20f, 1} : ImVec4{0.20f, 0.55f, 0.28f, 1});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, playing ? ImVec4{0.85f, 0.30f, 0.26f, 1} : ImVec4{0.26f, 0.68f, 0.35f, 1});
    if (ImGui::Button(playing ? "  Stop (Esc)  " : "  Play (F5)  ")) playing ? stop() : play();
    ImGui::PopStyleColor(2);

    ImGui::SameLine();
    ImGui::BeginDisabled(playing);
    if (ImGui::Button("Save")) save();
    ImGui::SameLine();
    if (ImGui::Button("Open...")) open_load_popup_ = true;
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::BeginDisabled(!history_.can_undo());
    if (ImGui::Button("Undo")) undo();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!history_.can_redo());
    if (ImGui::Button("Redo")) redo();
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::Checkbox("Snap", &snap_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70);
    static constexpr std::array<float, 4> kGrids = {8, 16, 32, 64};
    if (ImGui::BeginCombo("Grid", TextFormat("%d", static_cast<int>(grid_)))) {
        for (float g : kGrids)
            if (ImGui::Selectable(TextFormat("%d", static_cast<int>(g)), g == grid_)) grid_ = g;
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button(TextFormat("Zoom %d%%", static_cast<int>(cam_zoom_ * 100)))) cam_zoom_ = 1.0f;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to reset zoom to 100%%");

    if (playing) {
        ImGui::SameLine();
        ImGui::TextColored({0.45f, 0.9f, 0.55f, 1}, "   PLAYING  -  coins %d/%d  -  %.1fs", game_.score, game_.coins_total,
                           static_cast<double>(game_.time));
    } else if (dirty()) {
        ImGui::SameLine();
        ImGui::TextColored({0.96f, 0.75f, 0.35f, 1}, "   unsaved changes");
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------
// Content Browser: the prefab palette
// ---------------------------------------------------------------------------

void EditorApp::draw_content_browser() {
    ImGui::Begin("Content Browser");
    ImGui::TextWrapped("Click a prefab, then click in the Viewport -- or drag it in.");
    ImGui::Spacing();

    for (std::size_t i = 0; i < kAllPrefabs.size(); ++i) {
        const Prefab p = kAllPrefabs[i];
        const Entity sample = make_prefab(p, {0, 0});
        ImGui::PushID(static_cast<int>(i));

        const bool active = placing_ && *placing_ == p;
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        constexpr float kSwatch = 34.0f;   // space reserved on the left for the colour swatch
        ImGui::Indent(kSwatch);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
        if (ImGui::Selectable(prefab_name(p), active, 0, ImVec2(0, 30)))
            placing_ = active ? std::nullopt : std::optional<Prefab>(p);
        ImGui::PopStyleVar();
        ImGui::Unindent(kSwatch);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", prefab_description(p));

        // Drag-and-drop source: the payload is just the prefab's index.
        if (ImGui::BeginDragDropSource()) {
            const int index = static_cast<int>(i);
            ImGui::SetDragDropPayload(kPrefabPayload, &index, sizeof(index));
            ImGui::Text("Place %s", prefab_name(p));
            ImGui::EndDragDropSource();
        }

        // Colour swatch at the left of the row.
        if (sample.sprite) {
            const Color c = to_color(sample.sprite->color);
            ImGui::GetWindowDrawList()->AddRectFilled({pos.x + 2, pos.y + 5}, {pos.x + 26, pos.y + 25},
                                                      IM_COL32(c.r, c.g, c.b, 255), 4.0f);
        }
        ImGui::PopID();
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------
// Outliner: every entity in the level
// ---------------------------------------------------------------------------

static const char* entity_tag(const Entity& e, ImVec4& color) {
    if (e.player)              { color = {1.0f, 0.6f, 0.3f, 1}; return "PLAYER"; }
    if (e.goal)                { color = {0.4f, 0.85f, 0.6f, 1}; return "GOAL  "; }
    if (e.hazard && e.patrol)  { color = {0.9f, 0.4f, 0.5f, 1}; return "ENEMY "; }
    if (e.hazard)              { color = {0.9f, 0.4f, 0.5f, 1}; return "HAZARD"; }
    if (e.pickup)              { color = {1.0f, 0.85f, 0.3f, 1}; return "COIN  "; }
    if (e.collider)            { color = {0.6f, 0.75f, 0.5f, 1}; return "SOLID "; }
    color = {0.6f, 0.6f, 0.65f, 1};
    return "DECOR ";
}

void EditorApp::draw_outliner() {
    ImGui::Begin("Outliner");
    static std::string filter;
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##filter", "Search...", &filter);
    if (mode_ == Mode::Play) ImGui::TextColored({0.45f, 0.9f, 0.55f, 1}, "Showing the LIVE game world");
    else ImGui::TextDisabled("%zu entities", world_.size());

    ImGui::BeginChild("##list");
    for (const auto& e : world_.entities()) {
        if (!filter.empty()) {   // case-insensitive "contains"
            std::string a = e.name, b = filter;
            for (auto& ch : a) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            for (auto& ch : b) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            if (a.find(b) == std::string::npos) continue;
        }
        ImGui::PushID(static_cast<int>(e.id));
        ImVec4 tag_color;
        const char* tag = entity_tag(e, tag_color);
        ImGui::TextColored(tag_color, "%s", tag);
        ImGui::SameLine();
        if (ImGui::Selectable(e.name.c_str(), e.id == selected_, ImGuiSelectableFlags_AllowDoubleClick)) {
            selected_ = e.id;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) frame_selection();
        }
        if (ImGui::BeginPopupContextItem()) {
            selected_ = e.id;
            if (ImGui::MenuItem("Focus", "F")) frame_selection();
            if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, mode_ == Mode::Edit)) duplicate_selected();
            if (ImGui::MenuItem("Delete", "Del", false, mode_ == Mode::Edit)) delete_selected();
            ImGui::EndPopup();
            ImGui::PopID();
            break;   // the list may have changed -- stop iterating this frame
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::End();
}

// ---------------------------------------------------------------------------
// Details: edit every component of the selected entity
// ---------------------------------------------------------------------------

void EditorApp::draw_details() {
    ImGui::Begin("Details");
    Entity* e = world_.find(selected_);
    if (!e) {
        ImGui::TextDisabled("Select something in the Viewport or Outliner.");
        ImGui::End();
        return;
    }
    const bool playing = mode_ == Mode::Play;
    if (playing) ImGui::TextColored({0.45f, 0.9f, 0.55f, 1}, "Live values (read-only while playing)");
    ImGui::BeginDisabled(playing);

    // THE UNDO PATTERN FOR WIDGETS:
    //  - remember the entity as it was at the start of this frame
    //  - when a widget becomes active (mouse down / click into a text box), keep that copy
    //  - while it's active, the widget edits the entity directly (live preview)
    //  - when it deactivates after a change, record ONE ModifyEntityCommand(before, now)
    const Entity frame_start = *e;
    auto track_edit = [&](const char* what) {
        if (ImGui::IsItemActivated()) details_before_ = frame_start;
        if (ImGui::IsItemDeactivatedAfterEdit() && details_before_) {
            history_.push_applied(std::make_unique<ModifyEntityCommand>(*details_before_, *e, what));
            details_before_.reset();
        }
    };
    std::optional<Entity> structural_change;   // add/remove component: applied after drawing
    ImGui::PushItemWidth(-115.0f);              // widgets fill the width minus a label column

    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##name", &e->name);
    track_edit("Rename");
    ImGui::TextDisabled("Entity id %u", e->id);

    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat2("Position", &e->transform.pos.x, 1.0f, 0.0f, 0.0f, "%.0f");
        track_edit("Move");
        ImGui::DragFloat2("Size", &e->transform.size.x, 1.0f, 4.0f, 4000.0f, "%.0f");
        track_edit("Resize");
    }

    // Shows a collapsible header with an [x] button; clicking [x] queues
    // removal of that component.
    auto component = [&](const char* label, bool present, auto remove) -> bool {
        if (!present) return false;
        bool keep = true;
        const bool open = ImGui::CollapsingHeader(label, &keep, ImGuiTreeNodeFlags_DefaultOpen);
        if (!keep && !playing) { Entity after = *e; remove(after); structural_change = after; }
        return open;
    };

    if (component("Sprite", e->sprite.has_value(), [](Entity& a) { a.sprite.reset(); })) {
        static constexpr const char* kShapes[] = {"Box", "Circle", "Spikes", "Hero", "Flag", "Cloud", "Blob"};
        int shape = static_cast<int>(e->sprite->shape);
        if (ImGui::Combo("Shape", &shape, kShapes, IM_ARRAYSIZE(kShapes))) e->sprite->shape = static_cast<Shape>(shape);
        track_edit("Change shape of");
        Rgba& c = e->sprite->color;
        float col[4] = {c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f};
        if (ImGui::ColorEdit4("Color", col, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayHex)) {
            auto to8 = [](float f) { return static_cast<std::uint8_t>(std::clamp(f, 0.0f, 1.0f) * 255.0f + 0.5f); };
            c = {to8(col[0]), to8(col[1]), to8(col[2]), to8(col[3])};
        }
        track_edit("Recolor");
        ImGui::DragInt("Layer", &e->sprite->layer, 0.1f, -10, 10);
        track_edit("Change layer of");
    }
    if (component("Collider", e->collider.has_value(), [](Entity& a) { a.collider.reset(); })) {
        ImGui::Checkbox("Solid", &e->collider->solid);
        track_edit("Toggle solid on");
    }
    if (component("Body", e->body.has_value(), [](Entity& a) { a.body.reset(); })) {
        ImGui::DragFloat("Gravity scale", &e->body->gravity_scale, 0.01f, -2.0f, 4.0f);
        track_edit("Change gravity of");
        if (playing) ImGui::Text("velocity (%.0f, %.0f)  grounded: %s", static_cast<double>(e->body->velocity.x),
                                 static_cast<double>(e->body->velocity.y), e->body->grounded ? "yes" : "no");
    }
    if (component("Player Controller", e->player.has_value(), [](Entity& a) { a.player.reset(); })) {
        ImGui::DragFloat("Move speed", &e->player->move_speed, 1.0f, 0.0f, 1000.0f, "%.0f");
        track_edit("Tune");
        ImGui::DragFloat("Jump speed", &e->player->jump_speed, 1.0f, 0.0f, 1500.0f, "%.0f");
        track_edit("Tune");
        ImGui::DragFloat("Coyote time", &e->player->coyote_time, 0.005f, 0.0f, 0.5f, "%.3f s");
        track_edit("Tune");
        ImGui::TextDisabled("Jump height ~ %.0f units", static_cast<double>(e->player->jump_speed * e->player->jump_speed / (2 * kGravity)));
    }
    if (component("Patrol", e->patrol.has_value(), [](Entity& a) { a.patrol.reset(); })) {
        ImGui::DragFloat("Min X", &e->patrol->min_x, 1.0f, 0.0f, 0.0f, "%.0f");
        track_edit("Edit patrol of");
        ImGui::DragFloat("Max X", &e->patrol->max_x, 1.0f, 0.0f, 0.0f, "%.0f");
        track_edit("Edit patrol of");
        ImGui::DragFloat("Speed", &e->patrol->speed, 1.0f, 0.0f, 600.0f, "%.0f");
        track_edit("Edit patrol of");
    }
    if (component("Pickup", e->pickup.has_value(), [](Entity& a) { a.pickup.reset(); })) {
        ImGui::DragInt("Value", &e->pickup->value, 0.1f, 1, 100);
        track_edit("Edit value of");
    }
    if (component("Hazard", e->hazard.has_value(), [](Entity& a) { a.hazard.reset(); }))
        ImGui::TextDisabled("Touching this kills the player.");
    if (component("Goal", e->goal.has_value(), [](Entity& a) { a.goal.reset(); }))
        ImGui::TextDisabled("Touching this wins the level.");

    // Add Component menu: lists only the components this entity lacks.
    ImGui::Spacing();
    if (ImGui::Button("+ Add Component", ImVec2(-1, 0))) ImGui::OpenPopup("add_component");
    if (ImGui::BeginPopup("add_component")) {
        Entity after = *e;
        bool added = false;
        if (!e->sprite && ImGui::MenuItem("Sprite"))              { after.sprite = Sprite{}; added = true; }
        if (!e->collider && ImGui::MenuItem("Collider"))          { after.collider = Collider{}; added = true; }
        if (!e->body && ImGui::MenuItem("Body"))                  { after.body = Body{}; added = true; }
        if (!e->player && ImGui::MenuItem("Player Controller"))   { after.player = PlayerController{}; added = true; }
        if (!e->patrol && ImGui::MenuItem("Patrol")) {
            after.patrol = Patrol{e->transform.pos.x - 96, e->transform.pos.x + 96, 70.0f, 1};
            added = true;
        }
        if (!e->pickup && ImGui::MenuItem("Pickup"))              { after.pickup = Pickup{}; added = true; }
        if (!e->hazard && ImGui::MenuItem("Hazard"))              { after.hazard = Hazard{}; added = true; }
        if (!e->goal && ImGui::MenuItem("Goal"))                  { after.goal = Goal{}; added = true; }
        if (added) structural_change = after;
        ImGui::EndPopup();
    }
    ImGui::PopItemWidth();
    ImGui::EndDisabled();

    if (structural_change && !playing)
        execute(std::make_unique<ModifyEntityCommand>(*e, *structural_change, "Edit components of"));
    ImGui::End();
}

// ---------------------------------------------------------------------------
// History: the undo stack, visible
// ---------------------------------------------------------------------------

void EditorApp::draw_history() {
    ImGui::Begin("History");
    ImGui::BeginDisabled(mode_ == Mode::Play || !history_.can_undo());
    if (ImGui::Button("Undo")) undo();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(mode_ == Mode::Play || !history_.can_redo());
    if (ImGui::Button("Redo")) redo();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("%zu undo steps, %zu redo steps", history_.undo_count(), history_.redo_count());

    ImGui::BeginChild("##steps");
    static std::size_t last_count = 0;
    for (std::size_t i = 0; i < history_.undo_count(); ++i) {
        const bool latest = i + 1 == history_.undo_count();
        if (latest) ImGui::TextColored({0.96f, 0.6f, 0.3f, 1}, "%3zu. %s   <- you are here", i + 1, history_.undo_label(i).c_str());
        else        ImGui::Text("%3zu. %s", i + 1, history_.undo_label(i).c_str());
    }
    if (history_.undo_count() == 0) ImGui::TextDisabled("No edits yet. Every change you make shows up here.");
    if (history_.undo_count() != last_count) { ImGui::SetScrollHereY(1.0f); last_count = history_.undo_count(); }
    ImGui::EndChild();
    ImGui::End();
}

// ---------------------------------------------------------------------------
// Open / Save As dialogs (a path text box -- a native file picker is a nice exercise)
// ---------------------------------------------------------------------------

void EditorApp::draw_file_popups() {
    if (open_load_popup_ || open_save_as_popup_) {
        std::strncpy(path_buf_, level_path_.c_str(), sizeof(path_buf_) - 1);
        ImGui::OpenPopup(open_load_popup_ ? "Open Level" : "Save Level As");
        open_load_popup_ = open_save_as_popup_ = false;
    }
    for (const char* name : {"Open Level", "Save Level As"}) {
        if (ImGui::BeginPopupModal(name, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            const bool is_open = std::strcmp(name, "Open Level") == 0;
            ImGui::Text("Path to a .json level file:");
            ImGui::SetNextItemWidth(420);
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            const bool enter = ImGui::InputText("##path", path_buf_, sizeof(path_buf_), ImGuiInputTextFlags_EnterReturnsTrue);
            if (ImGui::Button(is_open ? "Open" : "Save") || enter) {
                if (is_open) load(path_buf_);
                else { level_path_ = path_buf_; save(); }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }
}

}  // namespace editor
