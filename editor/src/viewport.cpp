// The Viewport panel: renders the level into an off-screen texture
// (RenderTexture) and shows that texture as an image inside an ImGui window
// -- the same trick every engine editor uses to put the game view in a panel.
//
// Mouse controls (edit mode):
//   Left click            select (topmost entity under the cursor)
//   Left drag             move the selection (snaps to the grid)
//   Drag corner handle    resize
//   Right/Middle drag     pan the camera      (or hold Space + left drag)
//   Mouse wheel           zoom toward the cursor
//   Arrow keys            nudge the selection by one grid step

#include "editor_app.hpp"
#include "renderer.hpp"

#include "rlImGui.h"

#include <algorithm>
#include <cmath>

using namespace ember;

namespace editor {

static constexpr const char* kPrefabPayload = "EMBER_PREFAB";
static const Color kSelect{245, 140, 56, 255};

Vec2 EditorApp::screen_to_world(ImVec2 screen, ImVec2 image_min, ImVec2 image_size) const {
    const Vec2 local{screen.x - image_min.x - image_size.x * 0.5f, screen.y - image_min.y - image_size.y * 0.5f};
    return cam_center_ + local * (1.0f / cam_zoom_);
}

Vec2 EditorApp::snapped(Vec2 p) const {
    return snap_ ? Vec2{snap_to(p.x, grid_), snap_to(p.y, grid_)} : p;
}

EntityId EditorApp::pick(Vec2 point) const {
    // Search front-to-back: highest sprite layer first, and among equal
    // layers the one later in the list (drawn last = on top).
    EntityId best = kNoEntity;
    int best_layer = -1'000'000;
    for (const auto& e : world_.entities()) {
        if (!e.transform.rect().contains(point)) continue;
        const int layer = e.sprite ? e.sprite->layer : 0;
        if (layer >= best_layer) { best = e.id; best_layer = layer; }
    }
    return best;
}

void EditorApp::frame_selection() {
    if (const Entity* e = world_.find(selected_)) cam_center_ = e->transform.rect().center();
}

// Fit every entity in the level into the viewport (the "Home" key).
void EditorApp::frame_all(int w, int h) {
    if (world_.size() == 0) return;
    Rect b = world_.entities().front().transform.rect();
    for (const auto& e : world_.entities()) {
        const Rect r = e.transform.rect();
        const float x2 = std::max(b.right(), r.right()), y2 = std::max(b.bottom(), r.bottom());
        b.x = std::min(b.x, r.x);
        b.y = std::min(b.y, r.y);
        b.w = x2 - b.x;
        b.h = y2 - b.y;
    }
    const float margin = 80.0f;
    const float zoom = std::clamp(std::min(static_cast<float>(w) / (b.w + margin), static_cast<float>(h) / (b.h + margin)), 0.15f, 2.0f);
    // While playing, the game owns the camera -- frame the EDITOR camera
    // that comes back when Play stops.
    (mode_ == Mode::Play ? edit_cam_zoom_ : cam_zoom_) = zoom;
    (mode_ == Mode::Play ? edit_cam_center_ : cam_center_) = b.center();
}

void EditorApp::place_prefab(Prefab p, Vec2 world_center) {
    if (mode_ != Mode::Edit) return;
    const Vec2 size = make_prefab(p, {0, 0}).transform.size;
    auto cmd = std::make_unique<CreateEntityCommand>(make_prefab(p, snapped(world_center - size * 0.5f)));
    CreateEntityCommand* raw = cmd.get();
    execute(std::move(cmd));
    selected_ = raw->id();
    set_status(std::string("Placed ") + prefab_name(p));
}

// Size of the resize handle, in WORLD units (constant on screen at any zoom).
static Rect resize_handle(const Entity& e, float zoom) {
    const float s = 10.0f / zoom;
    const Rect r = e.transform.rect();
    return {r.right() - s * 0.5f, r.bottom() - s * 0.5f, s, s};
}

void EditorApp::viewport_mouse(ImVec2 image_min, ImVec2 image_size, bool hovered) {
    ImGuiIO& io = ImGui::GetIO();
    mouse_world_ = screen_to_world(io.MousePos, image_min, image_size);
    viewport_hovered_ = hovered;

    // --- Camera: zoom toward the cursor, so the point under it stays put.
    if (hovered && io.MouseWheel != 0.0f) {
        const Vec2 before = mouse_world_;
        cam_zoom_ = std::clamp(cam_zoom_ * std::pow(1.12f, io.MouseWheel), 0.15f, 4.0f);
        const Vec2 after = screen_to_world(io.MousePos, image_min, image_size);
        cam_center_ += before - after;
        mouse_world_ = before;
    }

    // Right-click while placing = cancel placing (instead of starting a pan).
    if (placing_ && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) { placing_.reset(); return; }

    // --- Camera: pan.
    const bool space = ImGui::IsKeyDown(ImGuiKey_Space) && mode_ == Mode::Edit;
    if (drag_ == Drag::None && hovered &&
        (ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
         (space && ImGui::IsMouseClicked(ImGuiMouseButton_Left)))) {
        drag_ = Drag::Pan;
        pan_last_ = io.MousePos;
    }
    if (drag_ == Drag::Pan) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseDown(ImGuiMouseButton_Right) ||
            ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
            const Vec2 delta{io.MousePos.x - pan_last_.x, io.MousePos.y - pan_last_.y};
            cam_center_ += delta * (-1.0f / cam_zoom_);
            pan_last_ = io.MousePos;
        } else {
            drag_ = Drag::None;
        }
        return;
    }

    if (mode_ != Mode::Edit) return;   // no editing while the game runs

    // --- Left click: place, start a resize, or select + start a move.
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (placing_) {
            place_prefab(*placing_, mouse_world_);
            if (!io.KeyShift) placing_.reset();   // hold Shift to keep stamping
            return;
        }
        const Entity* sel = world_.find(selected_);
        if (sel && resize_handle(*sel, cam_zoom_).contains(mouse_world_)) {
            drag_ = Drag::Resize;
            drag_before_ = *sel;
        } else {
            selected_ = pick(mouse_world_);
            if (const Entity* e = world_.find(selected_)) {
                drag_ = Drag::Move;
                drag_before_ = *e;
                drag_grab_offset_ = mouse_world_ - e->transform.pos;
            }
        }
    }

    // --- Dragging: change the world LIVE every frame for instant feedback,
    // then record ONE undo step when the button is released.
    if (drag_ == Drag::Move || drag_ == Drag::Resize) {
        Entity* e = world_.find(drag_before_.id);
        if (!e) { drag_ = Drag::None; return; }
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (drag_ == Drag::Move) {
                const Vec2 pos = snapped(mouse_world_ - drag_grab_offset_);
                const float dx = pos.x - e->transform.pos.x;
                e->transform.pos = pos;
                if (e->patrol) { e->patrol->min_x += dx; e->patrol->max_x += dx; }   // patrol range follows
            } else {
                const Vec2 corner = snapped(mouse_world_);
                e->transform.size = {std::max(8.0f, corner.x - e->transform.pos.x),
                                     std::max(8.0f, corner.y - e->transform.pos.y)};
            }
        } else {
            if (!(*e == drag_before_)) {
                const char* what = drag_ == Drag::Move ? "Move" : "Resize";
                history_.push_applied(std::make_unique<ModifyEntityCommand>(drag_before_, *e, what));
                set_status(TextFormat("%s %s to (%.0f, %.0f) size %.0fx%.0f", what, e->name.c_str(),
                                      static_cast<double>(e->transform.pos.x), static_cast<double>(e->transform.pos.y),
                                      static_cast<double>(e->transform.size.x), static_cast<double>(e->transform.size.y)));
            }
            drag_ = Drag::None;
        }
    }

    // --- Arrow keys nudge the selection (one undo step per press).
    if (ImGui::IsWindowFocused() && !io.WantTextInput && drag_ == Drag::None) {
        if (Entity* e = world_.find(selected_)) {
            Vec2 d{0, 0};
            const float stepv = snap_ ? grid_ : 1.0f;
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))  d.x -= stepv;
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) d.x += stepv;
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))    d.y -= stepv;
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))  d.y += stepv;
            if (d.x != 0 || d.y != 0) {
                Entity after = *e;
                after.transform.pos += d;
                if (after.patrol) { after.patrol->min_x += d.x; after.patrol->max_x += d.x; }
                execute(std::make_unique<ModifyEntityCommand>(*e, after, "Nudge"));
            }
        }
    }
}

void EditorApp::draw_hud(int w, int h) {
    DrawRectangleRounded({12, 12, 250, 56}, 0.25f, 6, Color{0, 0, 0, 120});
    DrawText(TextFormat("Coins  %d / %d", game_.score, game_.coins_total), 24, 20, 20, Color{255, 220, 110, 255});
    DrawText(TextFormat("Time   %.1fs", static_cast<double>(game_.time)), 24, 44, 18, RAYWHITE);

    if (game_.status == GameStatus::Playing) return;
    const bool won = game_.status == GameStatus::Won;
    DrawRectangle(0, 0, w, h, Color{0, 0, 0, 110});
    const char* title = won ? "LEVEL COMPLETE!" : "OUCH!";
    const int title_size = 48;
    DrawText(title, (w - MeasureText(title, title_size)) / 2, h / 2 - 70, title_size, won ? Color{120, 230, 150, 255} : Color{255, 120, 120, 255});
    const char* line = TextFormat("%d / %d coins   %.1f seconds", game_.score, game_.coins_total, static_cast<double>(game_.time));
    DrawText(line, (w - MeasureText(line, 22)) / 2, h / 2 - 8, 22, RAYWHITE);
    const char* hint = "R  restart        Esc  back to editor";
    DrawText(hint, (w - MeasureText(hint, 18)) / 2, h / 2 + 30, 18, Color{220, 220, 220, 255});
}

void EditorApp::render_viewport_texture(int w, int h) {
    if (w != rt_w_ || h != rt_h_) {   // panel resized -> new texture of the new size
        if (viewport_rt_.id != 0) UnloadRenderTexture(viewport_rt_);
        viewport_rt_ = LoadRenderTexture(w, h);
        rt_w_ = w; rt_h_ = h;
    }

    BeginTextureMode(viewport_rt_);
    draw_sky(w, h);

    Camera2D cam{};
    cam.offset = {static_cast<float>(w) * 0.5f, static_cast<float>(h) * 0.5f};
    cam.target = {cam_center_.x, cam_center_.y};
    cam.zoom = cam_zoom_;
    BeginMode2D(cam);

    const Rect visible{cam_center_.x - static_cast<float>(w) * 0.5f / cam_zoom_,
                       cam_center_.y - static_cast<float>(h) * 0.5f / cam_zoom_,
                       static_cast<float>(w) / cam_zoom_, static_cast<float>(h) / cam_zoom_};
    const bool editing = mode_ == Mode::Edit;
    if (editing) {
        float spacing = grid_;
        while (spacing * cam_zoom_ < 10.0f) spacing *= 2;   // don't draw a grey soup when zoomed out
        draw_grid(visible, spacing);
    }

    draw_world(world_, anim_time_);

    if (editing) {
        const float px = 1.0f / cam_zoom_;   // one screen pixel, in world units

        // Patrol ranges: where each enemy / moving platform will walk.
        for (const auto& e : world_.entities()) {
            if (!e.patrol) continue;
            const bool sel = e.id == selected_;
            const float y = e.transform.rect().bottom() + 6 * px;
            const float x1 = e.patrol->min_x, x2 = e.patrol->max_x + e.transform.size.x;
            const Color c = sel ? kSelect : Color{255, 255, 255, 90};
            DrawLineEx({x1, y}, {x2, y}, (sel ? 3 : 2) * px, c);
            DrawLineEx({x1, y - 5 * px}, {x1, y + 5 * px}, 2 * px, c);
            DrawLineEx({x2, y - 5 * px}, {x2, y + 5 * px}, 2 * px, c);
        }

        // Hover highlight.
        if (viewport_hovered_ && drag_ == Drag::None && !placing_) {
            if (const Entity* h_e = world_.find(pick(mouse_world_)); h_e && h_e->id != selected_) {
                const Rect r = h_e->transform.rect();
                DrawRectangleLinesEx({r.x - px, r.y - px, r.w + 2 * px, r.h + 2 * px}, px, Color{255, 255, 255, 170});
            }
        }

        // Selection outline + resize handle.
        if (const Entity* s = world_.find(selected_)) {
            const Rect r = s->transform.rect();
            DrawRectangleLinesEx({r.x - 2 * px, r.y - 2 * px, r.w + 4 * px, r.h + 4 * px}, 2 * px, kSelect);
            const Rect hnd = resize_handle(*s, cam_zoom_);
            DrawRectangleRec({hnd.x, hnd.y, hnd.w, hnd.h}, kSelect);
            DrawRectangleLinesEx({hnd.x, hnd.y, hnd.w, hnd.h}, px, WHITE);
        }

        // Ghost preview of the prefab about to be placed.
        if (placing_ && viewport_hovered_) {
            World ghost;
            Entity g = make_prefab(*placing_, {0, 0});
            g = make_prefab(*placing_, snapped(mouse_world_ - g.transform.size * 0.5f));
            if (g.sprite) g.sprite->color.a = 140;
            const Rect r = g.transform.rect();
            ghost.create(g);
            draw_world(ghost, anim_time_);
            DrawRectangleLinesEx({r.x, r.y, r.w, r.h}, px, Color{255, 255, 255, 200});
        }
    }
    EndMode2D();

    // Screen-space overlays.
    if (editing) {
        if (const Entity* s = world_.find(selected_)) {
            const Vector2 top = GetWorldToScreen2D({s->transform.pos.x, s->transform.pos.y}, cam);
            DrawText(s->name.c_str(), static_cast<int>(top.x), static_cast<int>(top.y) - 18, 16, Color{255, 236, 210, 255});
        }
        const char* hint = placing_ ? "Click to place  (Shift = keep placing, Right-click/Esc = cancel)"
                                    : "LMB select/drag   Corner = resize   RMB/MMB drag = pan   Wheel = zoom   F5 = Play";
        DrawRectangle(0, h - 26, w, 26, Color{0, 0, 0, 90});
        DrawText(hint, 10, h - 20, 14, Color{235, 235, 235, 255});
    } else {
        draw_hud(w, h);
    }
    EndTextureMode();
}

void EditorApp::draw_viewport() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const int w = std::max(1, static_cast<int>(avail.x));
    const int h = std::max(1, static_cast<int>(avail.y));
    const ImVec2 image_min = ImGui::GetCursorScreenPos();
    const ImVec2 size{static_cast<float>(w), static_cast<float>(h)};

    // Wait a couple of frames: on frame 0 the dock layout isn't applied yet and
    // the viewport briefly has the wrong size.
    if (frame_all_pending_ && frame_count_ >= 2 && w > 50 && h > 50) { frame_all(w, h); frame_all_pending_ = false; }
    if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Home)) frame_all(w, h);

    const bool hovered = ImGui::IsWindowHovered() &&
                         ImGui::IsMouseHoveringRect(image_min, {image_min.x + size.x, image_min.y + size.y});

    viewport_mouse(image_min, size, hovered);   // input first, so this frame's picture reflects it
    render_viewport_texture(w, h);
    rlImGuiImageRenderTexture(&viewport_rt_);

    // An invisible button on top of the image: it stops a click in the
    // viewport from dragging the window around, and it's the drop target
    // for prefabs dragged from the Content Browser.
    ImGui::SetCursorScreenPos(image_min);
    ImGui::InvisibleButton("##viewport_canvas", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kPrefabPayload)) {
            const int index = *static_cast<const int*>(p->Data);
            place_prefab(kAllPrefabs[static_cast<std::size_t>(index)], mouse_world_);
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

}  // namespace editor
