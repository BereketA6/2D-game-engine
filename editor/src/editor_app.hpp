#pragma once
// The editor application: owns the level, the undo history, the camera and
// all UI state. Implementation is split by area so each file stays readable:
//
//   editor_app.cpp : main loop, Play-In-Editor, save/load, shortcuts
//   panels.cpp     : Toolbar, Outliner, Details, Content Browser, History
//   viewport.cpp   : the level view -- mouse picking, dragging, placing, camera
//   renderer.cpp   : drawing entities (no editor logic at all)

#include "ember/commands.hpp"
#include "ember/prefabs.hpp"
#include "ember/systems.hpp"
#include "ember/world.hpp"

#include "imgui.h"
#include "raylib.h"

#include <optional>
#include <string>

namespace editor {

struct Options {
    std::string level_path = "levels/demo.json";
    int width = 1440, height = 860;
    // Automation hooks (used to produce screenshots for the README):
    std::string screenshot;        // save a PNG of the whole window...
    int screenshot_frame = 60;     // ...on this frame, then quit
    bool start_playing = false;
    bool hold_right = false;       // simulate holding the Right key in Play mode
    std::string select;            // select the entity with this name at startup
};

enum class Mode { Edit, Play };

class EditorApp {
public:
    explicit EditorApp(Options opt);
    int run();

private:
    // --- editor_app.cpp ---
    void frame();
    void update_play(float frame_dt);
    void build_default_layout(ImGuiID dockspace_id);
    void handle_shortcuts();
    void play();
    void stop();
    void restart_play();
    void save();
    void load(const std::string& path);
    void new_level();
    void undo();
    void redo();
    void delete_selected();
    void duplicate_selected();
    void execute(std::unique_ptr<ember::Command> cmd);
    void set_status(std::string msg);
    void update_window_title();
    bool dirty() const { return history_.version() != saved_version_; }

    // --- panels.cpp ---
    void draw_toolbar();
    void draw_outliner();
    void draw_details();
    void draw_content_browser();
    void draw_history();
    void draw_file_popups();

    // --- viewport.cpp ---
    void draw_viewport();
    void viewport_mouse(ImVec2 image_min, ImVec2 image_size, bool hovered);
    void render_viewport_texture(int w, int h);
    ember::Vec2 screen_to_world(ImVec2 screen, ImVec2 image_min, ImVec2 image_size) const;
    ember::EntityId pick(ember::Vec2 world_point) const;
    ember::Vec2 snapped(ember::Vec2 p) const;
    void frame_selection();
    void place_prefab(ember::Prefab p, ember::Vec2 world_center);
    void draw_hud(int w, int h);
    void frame_all(int w, int h);

    Options opt_;
    std::string level_path_;

    ember::World world_;             // the level -- while playing, the LIVE game world
    ember::World edit_snapshot_;     // the level as it was when Play was pressed
    ember::History history_;
    std::size_t saved_version_ = 0;

    Mode mode_ = Mode::Edit;
    ember::GameState game_;
    float accumulator_ = 0.0f;       // leftover real time not yet simulated
    float anim_time_ = 0.0f;

    ember::EntityId selected_ = ember::kNoEntity;
    std::optional<ember::Prefab> placing_;

    // Camera: which world point is at the viewport centre, and how zoomed in.
    ember::Vec2 cam_center_{1150.0f, 330.0f};
    float cam_zoom_ = 0.62f;
    ember::Vec2 edit_cam_center_;    // editor camera, restored when Play stops
    float edit_cam_zoom_ = 1.0f;

    bool snap_ = true;
    float grid_ = 16.0f;

    // Mouse drag in the viewport.
    enum class Drag { None, Move, Resize, Pan };
    Drag drag_ = Drag::None;
    ember::Entity drag_before_;      // entity as it was when the drag started
    ember::Vec2 drag_grab_offset_;   // mouse position relative to entity corner
    ImVec2 pan_last_{0, 0};
    ember::Vec2 mouse_world_;
    bool viewport_hovered_ = false;

    // Details panel: entity state at the moment a widget started being edited.
    std::optional<ember::Entity> details_before_;

    RenderTexture2D viewport_rt_{};
    int rt_w_ = 0, rt_h_ = 0;

    std::string status_;
    double status_until_ = 0.0;
    std::string title_shown_;
    char path_buf_[512] = {};
    bool open_load_popup_ = false, open_save_as_popup_ = false;
    bool layout_built_ = false;
    bool frame_all_pending_ = true;   // fit the whole level in view once the viewport size is known
    int frame_count_ = 0;
};

}  // namespace editor
