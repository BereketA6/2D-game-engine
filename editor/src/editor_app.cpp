#include "editor_app.hpp"

#include "ember/level_io.hpp"

#include "imgui_internal.h"   // DockBuilder (default layout) and the status bar
#include "rlImGui.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>

using namespace ember;

namespace editor {

EditorApp::EditorApp(Options opt) : opt_(std::move(opt)), level_path_(opt_.level_path) {
    std::string err;
    if (auto w = load_level(level_path_, err)) {
        world_ = std::move(*w);
    } else {
        world_ = make_demo_level();   // first run: start from the built-in demo
        status_ = "Loaded the built-in demo level (" + err + ")";
    }
    if (!opt_.select.empty())
        for (const auto& e : world_.entities()) if (e.name == opt_.select) selected_ = e.id;
}

// ---------------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------------

// ImGui ships a few fonts in its repo; CMake passes us the path to Roboto.
// If it's missing for any reason we quietly fall back to the built-in font.
static void load_fonts() {
    ImGuiIO& io = ImGui::GetIO();
#ifdef EMBER_FONT_PATH
    if (FileExists(EMBER_FONT_PATH)) {
        io.Fonts->AddFontFromFileTTF(EMBER_FONT_PATH, 16.0f);
        return;
    }
#endif
    io.Fonts->AddFontDefault();
}

static void apply_style() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 4; s.FrameRounding = 3; s.TabRounding = 3; s.GrabRounding = 3;
    s.WindowPadding = {8, 8}; s.FramePadding = {6, 4}; s.ItemSpacing = {8, 6};
    ImVec4* c = s.Colors;
    const ImVec4 ember_orange{0.96f, 0.55f, 0.22f, 1.0f};
    c[ImGuiCol_WindowBg]           = {0.11f, 0.11f, 0.12f, 1.0f};
    c[ImGuiCol_TitleBg]            = {0.08f, 0.08f, 0.09f, 1.0f};
    c[ImGuiCol_TitleBgActive]      = {0.13f, 0.13f, 0.15f, 1.0f};
    c[ImGuiCol_Header]             = {0.96f, 0.55f, 0.22f, 0.30f};
    c[ImGuiCol_HeaderHovered]      = {0.96f, 0.55f, 0.22f, 0.45f};
    c[ImGuiCol_HeaderActive]       = {0.96f, 0.55f, 0.22f, 0.60f};
    c[ImGuiCol_Button]             = {0.20f, 0.20f, 0.23f, 1.0f};
    c[ImGuiCol_ButtonHovered]      = {0.96f, 0.55f, 0.22f, 0.55f};
    c[ImGuiCol_ButtonActive]       = ember_orange;
    c[ImGuiCol_FrameBg]            = {0.17f, 0.17f, 0.19f, 1.0f};
    c[ImGuiCol_CheckMark]          = ember_orange;
    c[ImGuiCol_SliderGrab]         = ember_orange;
    c[ImGuiCol_TabSelected]        = {0.22f, 0.22f, 0.25f, 1.0f};
    c[ImGuiCol_TabSelectedOverline] = ember_orange;
    c[ImGuiCol_DockingPreview]     = {0.96f, 0.55f, 0.22f, 0.5f};
}

int EditorApp::run() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(opt_.width, opt_.height, "Ember Editor");
    SetExitKey(KEY_NULL);   // Esc is ours (stop playing / cancel), not "quit"
    SetTargetFPS(60);

    rlImGuiSetLoadFontsCallback(&load_fonts);
    rlImGuiSetup(true);
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // Remember the user's panel layout between runs -- except when making
    // screenshots, which should always show the default layout.
    io.IniFilename = opt_.screenshot.empty() ? "ember_layout.ini" : nullptr;
    apply_style();
    if (opt_.start_playing) play();

    while (!WindowShouldClose()) {
        frame();
        if (!opt_.screenshot.empty() && frame_count_ > opt_.screenshot_frame) break;
    }

    if (viewport_rt_.id != 0) UnloadRenderTexture(viewport_rt_);
    rlImGuiShutdown();
    CloseWindow();
    return 0;
}

void EditorApp::frame() {
    const float dt = GetFrameTime();
    anim_time_ += dt;
    if (mode_ == Mode::Play) update_play(dt);
    if (selected_ != kNoEntity && !world_.find(selected_)) selected_ = kNoEntity;   // e.g. undone or collected

    BeginDrawing();
    ClearBackground(Color{20, 20, 24, 255});
    rlImGuiBegin();

    // Main menu bar.
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Level")) new_level();
            if (ImGui::MenuItem("Open...")) open_load_popup_ = true;
            if (ImGui::MenuItem("Save", "Ctrl+S")) save();
            if (ImGui::MenuItem("Save As...")) open_save_as_popup_ = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Quit")) CloseWindow();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, history_.can_undo() && mode_ == Mode::Edit)) undo();
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, history_.can_redo() && mode_ == Mode::Edit)) redo();
            ImGui::Separator();
            if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, selected_ != kNoEntity)) duplicate_selected();
            if (ImGui::MenuItem("Delete", "Del", false, selected_ != kNoEntity)) delete_selected();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Snap to grid", "G", &snap_);
            if (ImGui::MenuItem("Frame selection", "F", false, selected_ != kNoEntity)) frame_selection();
            if (ImGui::MenuItem("Frame entire level", "Home")) frame_all_pending_ = true;
            if (ImGui::MenuItem("Reset layout")) layout_built_ = false;
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // Status bar along the bottom of the window.
    const ImGuiWindowFlags bar_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
    if (ImGui::BeginViewportSideBar("##StatusBar", ImGui::GetMainViewport(), ImGuiDir_Down, ImGui::GetFrameHeight(), bar_flags)) {
        if (ImGui::BeginMenuBar()) {
            ImGui::TextColored(mode_ == Mode::Play ? ImVec4{0.4f, 0.9f, 0.5f, 1} : ImVec4{0.96f, 0.55f, 0.22f, 1},
                               mode_ == Mode::Play ? "PLAYING" : "EDITING");
            ImGui::Separator();
            ImGui::Text("%zu entities", world_.size());
            ImGui::Separator();
            ImGui::Text("zoom %d%%", static_cast<int>(cam_zoom_ * 100));
            ImGui::Separator();
            ImGui::Text("x %.0f  y %.0f", static_cast<double>(mouse_world_.x), static_cast<double>(mouse_world_.y));
            ImGui::Separator();
            ImGui::Text("snap %s", snap_ ? "on" : "off");
            if (!status_.empty() && GetTime() < status_until_) { ImGui::Separator(); ImGui::TextUnformatted(status_.c_str()); }
            ImGui::SameLine(ImGui::GetWindowWidth() - 70);
            ImGui::Text("%d FPS", GetFPS());
            ImGui::EndMenuBar();
        }
    }
    ImGui::End();

    const ImGuiID dock_id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
    if (!layout_built_) build_default_layout(dock_id);

    draw_toolbar();
    draw_content_browser();
    draw_outliner();
    draw_details();
    draw_history();
    draw_viewport();
    draw_file_popups();
    handle_shortcuts();

    rlImGuiEnd();

    // Automation: grab the finished frame BEFORE it is presented.
    // (For manual screenshots, raylib already has one built in: press F12.)
    if (!opt_.screenshot.empty() && frame_count_ == opt_.screenshot_frame) {
        Image shot = LoadImageFromScreen();
        ExportImage(shot, opt_.screenshot.c_str());
        UnloadImage(shot);
    }
    EndDrawing();
    update_window_title();
    ++frame_count_;
}

void EditorApp::build_default_layout(ImGuiID id) {
    // Unreal-style arrangement:
    //   +---------------------- Toolbar -----------------------+
    //   | Content |                              | Outliner     |
    //   | Browser |          Viewport            |--------------|
    //   |         |                              | Details      |
    //   |         |------------------------------|              |
    //   |         |          History             |              |
    //   +---------+------------------------------+--------------+
    ImGuiDockNode* existing = ImGui::DockBuilderGetNode(id);
    const bool ini_restored = existing && existing->IsSplitNode() && frame_count_ == 0 && ImGui::GetIO().IniFilename;
    layout_built_ = true;
    if (ini_restored) return;   // the user's saved layout wins

    ImGui::DockBuilderRemoveNode(id);
    ImGui::DockBuilderAddNode(id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(id, ImGui::GetMainViewport()->WorkSize);

    ImGuiID center = id;
    ImGuiID top    = ImGui::DockBuilderSplitNode(center, ImGuiDir_Up, 0.055f, nullptr, &center);
    ImGuiID left   = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.16f, nullptr, &center);
    ImGuiID right  = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.24f, nullptr, &center);
    ImGuiID right_bottom = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.64f, nullptr, &right);
    ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.20f, nullptr, &center);

    ImGui::DockBuilderDockWindow("Toolbar", top);
    ImGui::DockBuilderDockWindow("Content Browser", left);
    ImGui::DockBuilderDockWindow("Outliner", right);
    ImGui::DockBuilderDockWindow("Details", right_bottom);
    ImGui::DockBuilderDockWindow("History", bottom);
    ImGui::DockBuilderDockWindow("Viewport", center);
    if (ImGuiDockNode* n = ImGui::DockBuilderGetNode(top)) n->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
    ImGui::DockBuilderFinish(id);
}

// ---------------------------------------------------------------------------
// Play-In-Editor
// ---------------------------------------------------------------------------

void EditorApp::play() {
    if (mode_ == Mode::Play) return;
    bool has_player = false;
    for (const auto& e : world_.entities()) if (e.player && e.body) has_player = true;
    if (!has_player) { set_status("Can't play: the level needs a Player (drag one from the Content Browser)"); return; }

    edit_snapshot_ = world_;          // remember the level exactly as it is
    game_ = begin_play(world_);
    accumulator_ = 0.0f;
    edit_cam_center_ = cam_center_;   // the game gets its own camera...
    edit_cam_zoom_ = cam_zoom_;
    cam_zoom_ = 1.0f;
    if (const Entity* p = world_.find_player()) cam_center_ = p->transform.rect().center();
    placing_.reset();
    drag_ = Drag::None;
    mode_ = Mode::Play;
    set_status("Playing: Arrows/A-D to move, Space/W to jump, R to restart, Esc to stop");
}

void EditorApp::stop() {
    if (mode_ != Mode::Play) return;
    world_ = edit_snapshot_;          // throw away everything the game changed
    mode_ = Mode::Edit;
    cam_center_ = edit_cam_center_;   // ...and the editor gets its view back
    cam_zoom_ = edit_cam_zoom_;
    set_status("Stopped: level restored to how it was before Play");
}

void EditorApp::restart_play() {
    world_ = edit_snapshot_;
    game_ = begin_play(world_);
    accumulator_ = 0.0f;
}

void EditorApp::update_play(float frame_dt) {
    // Fixed timestep: real frame time piles up in the accumulator and is
    // spent in exact 1/60 s simulation steps. Clamp it so a long pause
    // (dragging the window, a breakpoint) doesn't cause a burst of steps.
    accumulator_ += std::min(frame_dt, 0.25f);

    Input in;
    const bool typing = ImGui::GetIO().WantTextInput;
    if (!typing) {
        in.left  = IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A);
        in.right = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D) || opt_.hold_right;
        in.jump_pressed = IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W);
    }
    while (accumulator_ >= kFixedDt) {
        step(world_, in, game_);
        in.jump_pressed = false;      // a key press is one jump, not one per step
        accumulator_ -= kFixedDt;
    }

    // Game camera: glide toward the player (a little ahead and above), but
    // stop following once they fall far below the level -- watching the
    // player plummet into the void isn't fun.
    if (const Entity* p = world_.find_player()) {
        Vec2 target = p->transform.rect().center() + Vec2{80.0f, -60.0f};
        target.y = std::min(target.y, game_.kill_y - 600.0f);
        cam_center_ = cam_center_ + (target - cam_center_) * 0.12f;
    }
}

// ---------------------------------------------------------------------------
// Editing actions
// ---------------------------------------------------------------------------

void EditorApp::execute(std::unique_ptr<Command> cmd) {
    if (mode_ == Mode::Play) return;   // never record edits to the throwaway play world
    history_.execute(std::move(cmd), world_);
}

void EditorApp::undo() {
    if (mode_ == Mode::Edit && history_.can_undo()) {
        const std::string what = history_.undo_label(history_.undo_count() - 1);
        history_.undo(world_);
        set_status("Undo: " + what);
    }
}

void EditorApp::redo() {
    if (mode_ == Mode::Edit && history_.redo(world_)) set_status("Redo");
}

void EditorApp::delete_selected() {
    if (mode_ != Mode::Edit || selected_ == kNoEntity) return;
    const std::string name = world_.find(selected_)->name;
    execute(std::make_unique<DeleteEntityCommand>(selected_));
    selected_ = kNoEntity;
    set_status("Deleted " + name);
}

void EditorApp::duplicate_selected() {
    if (mode_ != Mode::Edit || selected_ == kNoEntity) return;
    Entity copy = *world_.find(selected_);
    copy.id = kNoEntity;
    const float dx = std::max(grid_ * 2, 32.0f);
    copy.transform.pos.x += dx;
    if (copy.patrol) { copy.patrol->min_x += dx; copy.patrol->max_x += dx; }
    auto cmd = std::make_unique<CreateEntityCommand>(copy, world_.index_of(selected_) + 1);
    CreateEntityCommand* raw = cmd.get();
    execute(std::move(cmd));
    selected_ = raw->id();
}

void EditorApp::save() {
    std::string err;
    const std::filesystem::path p(level_path_);
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
    // While playing, save the level as it was BEFORE Play, never the game in progress.
    const World& level = mode_ == Mode::Play ? edit_snapshot_ : world_;
    if (save_level(level_path_, level, err)) {
        saved_version_ = history_.version();
        set_status("Saved " + level_path_);
    } else {
        set_status("Save failed: " + err);
    }
}

void EditorApp::load(const std::string& path) {
    if (mode_ == Mode::Play) stop();
    std::string err;
    if (auto w = load_level(path, err)) {
        world_ = std::move(*w);
        history_.clear();
        saved_version_ = history_.version();
        level_path_ = path;
        selected_ = kNoEntity;
        set_status("Opened " + path);
    } else {
        set_status("Open failed: " + err);
    }
}

void EditorApp::new_level() {
    if (mode_ == Mode::Play) stop();
    world_ = World{};
    Entity ground = make_prefab(Prefab::Platform, {0, 520});
    ground.transform.size = {960, 64};
    ground.name = "Ground";
    world_.create(ground);
    world_.create(make_prefab(Prefab::Player, {96, 470}));
    history_.clear();
    saved_version_ = history_.version() - 1;   // brand new = unsaved
    level_path_ = "levels/untitled.json";
    selected_ = kNoEntity;
    set_status("New level");
}

void EditorApp::set_status(std::string msg) {
    std::printf("[ember] %s\n", msg.c_str());   // also log to the terminal
    std::fflush(stdout);
    status_ = std::move(msg);
    status_until_ = GetTime() + 5.0;
}

void EditorApp::update_window_title() {
    std::string t = "Ember Editor - " + std::filesystem::path(level_path_).filename().string();
    if (dirty()) t += " *";
    if (mode_ == Mode::Play) t += "  [PLAYING]";
    if (t != title_shown_) { SetWindowTitle(t.c_str()); title_shown_ = t; }
}

// ---------------------------------------------------------------------------
// Keyboard shortcuts. ImGuiMod_Ctrl automatically means Cmd on macOS, and
// RouteGlobal means a focused text box still gets its own keys first.
// ---------------------------------------------------------------------------

void EditorApp::handle_shortcuts() {
    const ImGuiInputFlags g = ImGuiInputFlags_RouteGlobal;
    if (ImGui::Shortcut(ImGuiKey_F5, g)) mode_ == Mode::Play ? stop() : play();
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_P, g)) mode_ == Mode::Play ? stop() : play();
    if (ImGui::Shortcut(ImGuiKey_Escape, g)) {
        if (mode_ == Mode::Play) stop();
        else if (placing_) placing_.reset();
        else selected_ = kNoEntity;
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, g)) save();

    if (mode_ == Mode::Play) {
        if (ImGui::Shortcut(ImGuiKey_R, g)) restart_play();
        return;
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, g)) undo();
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z, g)) redo();
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, g)) redo();
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_D, g)) duplicate_selected();
    if (ImGui::Shortcut(ImGuiKey_Delete, g) || ImGui::Shortcut(ImGuiKey_Backspace, g)) delete_selected();
    if (ImGui::Shortcut(ImGuiKey_F, g)) frame_selection();
    if (ImGui::Shortcut(ImGuiKey_G, g)) snap_ = !snap_;
}

}  // namespace editor
