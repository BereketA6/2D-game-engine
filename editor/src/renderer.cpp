#include "renderer.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace ember;

namespace editor {

Color to_color(Rgba c) { return Color{c.r, c.g, c.b, c.a}; }

static Color shade(Color c, float f) {   // f < 1 darkens, f > 1 lightens
    auto ch = [f](unsigned char v) { return static_cast<unsigned char>(std::clamp(static_cast<float>(v) * f, 0.0f, 255.0f)); };
    return Color{ch(c.r), ch(c.g), ch(c.b), c.a};
}

void draw_sky(int width, int height) {
    DrawRectangleGradientV(0, 0, width, height, Color{94, 154, 214, 255}, Color{196, 226, 240, 255});
}

void draw_grid(Rect v, float spacing) {
    if (spacing <= 0) return;
    const Color minor{255, 255, 255, 28}, major{255, 255, 255, 60};
    const float x0 = std::floor(v.x / spacing) * spacing, y0 = std::floor(v.y / spacing) * spacing;
    for (float x = x0; x <= v.right(); x += spacing) {
        const bool is_major = std::fmod(std::abs(x), spacing * 8) < 0.5f;
        DrawLineV({x, v.y}, {x, v.bottom()}, is_major ? major : minor);
    }
    for (float y = y0; y <= v.bottom(); y += spacing) {
        const bool is_major = std::fmod(std::abs(y), spacing * 8) < 0.5f;
        DrawLineV({v.x, y}, {v.right(), y}, is_major ? major : minor);
    }
}

// --- One function per shape --------------------------------------------------

static void draw_box(Rectangle r, Color c) {
    DrawRectangleRec(r, c);
    // A lighter top strip (grass / highlight) and a darker outline.
    DrawRectangleRec({r.x, r.y, r.width, std::min(8.0f, r.height * 0.3f)}, shade(c, 1.25f));
    DrawRectangleLinesEx(r, 2, shade(c, 0.7f));
}

static void draw_coin(Rectangle r, Color c, float time) {
    // Fake a spinning coin by squashing its width with cos(time).
    const float squash = 0.55f + 0.45f * std::abs(std::cos(time * 2.5f));
    const Vector2 center{r.x + r.width / 2, r.y + r.height / 2};
    const float rx = r.width / 2 * squash, ry = r.height / 2;
    DrawEllipse(static_cast<int>(center.x), static_cast<int>(center.y), rx, ry, shade(c, 0.8f));
    DrawEllipse(static_cast<int>(center.x), static_cast<int>(center.y), rx * 0.7f, ry * 0.7f, c);
    DrawEllipse(static_cast<int>(center.x - rx * 0.25f), static_cast<int>(center.y - ry * 0.3f), rx * 0.2f, ry * 0.2f, shade(c, 1.4f));
}

static void draw_spikes(Rectangle r, Color c) {
    const int n = std::max(1, static_cast<int>(r.width / 16));
    const float w = r.width / static_cast<float>(n);
    for (int i = 0; i < n; ++i) {
        const float x = r.x + w * static_cast<float>(i);
        DrawTriangle({x + w / 2, r.y}, {x, r.y + r.height}, {x + w, r.y + r.height}, c);   // counter-clockwise
        DrawTriangleLines({x + w / 2, r.y}, {x, r.y + r.height}, {x + w, r.y + r.height}, shade(c, 0.6f));
    }
}

static void draw_face(Rectangle r, float look) {   // look: -1 left, 0 ahead, +1 right
    const float eye_y = r.y + r.height * 0.32f;
    const float spacing = r.width * 0.22f;
    const float cx = r.x + r.width / 2 + look * r.width * 0.12f;
    for (float side : {-1.0f, 1.0f}) {
        DrawCircleV({cx + side * spacing, eye_y}, r.width * 0.13f, WHITE);
        DrawCircleV({cx + side * spacing + look * 1.5f, eye_y}, r.width * 0.06f, Color{30, 30, 40, 255});
    }
}

static void draw_hero(const Entity& e, Rectangle r, Color c) {
    DrawRectangleRounded(r, 0.35f, 6, c);
    DrawRectangleRoundedLinesEx(r, 0.35f, 6, 2, shade(c, 0.65f));
    float look = 0;
    if (e.body && std::abs(e.body->velocity.x) > 1) look = e.body->velocity.x > 0 ? 1.0f : -1.0f;
    draw_face(r, look);
}

static void draw_blob(const Entity& e, Rectangle r, Color c) {
    DrawRectangleRounded(r, 0.6f, 8, c);
    DrawRectangleRoundedLinesEx(r, 0.6f, 8, 2, shade(c, 0.6f));
    const float look = e.patrol ? static_cast<float>(e.patrol->direction) : 0.0f;
    draw_face(r, look);
    // angry eyebrows
    const float cx = r.x + r.width / 2, y = r.y + r.height * 0.15f;
    DrawLineEx({cx - r.width * 0.35f, y - 2}, {cx - r.width * 0.08f, y + 3}, 2, Color{40, 20, 30, 255});
    DrawLineEx({cx + r.width * 0.35f, y - 2}, {cx + r.width * 0.08f, y + 3}, 2, Color{40, 20, 30, 255});
}

static void draw_flag(Rectangle r, Color c, float time) {
    const float pole_x = r.x + 4;
    DrawRectangleRec({pole_x, r.y, 4, r.height}, Color{230, 230, 235, 255});
    const float wave = std::sin(time * 4.0f) * 4.0f;
    DrawTriangle({pole_x + 4, r.y + 4}, {pole_x + 4, r.y + 36}, {r.x + r.width + 18 + wave, r.y + 20}, c);
    DrawCircleV({pole_x + 2, r.y}, 5, Color{255, 214, 90, 255});
}

static void draw_cloud(Rectangle r, Color c) {
    const float h = r.height;
    DrawCircleV({r.x + r.width * 0.25f, r.y + h * 0.6f}, h * 0.4f, c);
    DrawCircleV({r.x + r.width * 0.5f, r.y + h * 0.42f}, h * 0.5f, c);
    DrawCircleV({r.x + r.width * 0.75f, r.y + h * 0.62f}, h * 0.38f, c);
    DrawRectangleRounded({r.x + r.width * 0.12f, r.y + h * 0.55f, r.width * 0.76f, h * 0.45f}, 1.0f, 8, c);
}

void draw_world(const World& world, float time) {
    // Painter's algorithm: draw back-to-front by layer. stable_sort keeps
    // Outliner order for entities on the same layer.
    std::vector<const Entity*> order;
    order.reserve(world.size());
    for (const auto& e : world.entities()) if (e.sprite) order.push_back(&e);
    std::stable_sort(order.begin(), order.end(), [](const Entity* a, const Entity* b) { return a->sprite->layer < b->sprite->layer; });

    for (const Entity* e : order) {
        const Rect rr = e->transform.rect();
        const Rectangle r{rr.x, rr.y, rr.w, rr.h};
        const Color c = to_color(e->sprite->color);
        switch (e->sprite->shape) {
            case Shape::Box:    draw_box(r, c); break;
            case Shape::Circle: draw_coin(r, c, time); break;
            case Shape::Spikes: draw_spikes(r, c); break;
            case Shape::Hero:   draw_hero(*e, r, c); break;
            case Shape::Blob:   draw_blob(*e, r, c); break;
            case Shape::Flag:   draw_flag(r, c, time); break;
            case Shape::Cloud:  draw_cloud(r, c); break;
        }
    }
}

}  // namespace editor
