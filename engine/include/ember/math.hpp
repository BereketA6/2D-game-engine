#pragma once
// Tiny 2D math: a vector and an axis-aligned rectangle.
//
// Coordinate system: x grows RIGHT, y grows DOWN (same as the screen and as
// raylib). One world unit = one pixel at zoom 1.0. So "gravity" is a
// POSITIVE y acceleration, and "jumping" sets a NEGATIVE y velocity.

#include <algorithm>
#include <cmath>

namespace ember {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator*(float s) const { return {x * s, y * s}; }
    constexpr Vec2& operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
    constexpr bool operator==(const Vec2&) const = default;

    float length() const { return std::sqrt(x * x + y * y); }
};

// Axis-aligned bounding box (AABB): top-left corner + size.
// Every collision in the engine is an AABB-vs-AABB test -- simple, fast,
// and exactly what most 2D platformers use.
struct Rect {
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;

    constexpr float left() const { return x; }
    constexpr float right() const { return x + w; }
    constexpr float top() const { return y; }
    constexpr float bottom() const { return y + h; }
    constexpr Vec2 center() const { return {x + w * 0.5f, y + h * 0.5f}; }

    // True if the two rectangles overlap (touching edges do NOT count).
    constexpr bool overlaps(const Rect& o) const {
        return x < o.right() && right() > o.x && y < o.bottom() && bottom() > o.y;
    }
    constexpr bool contains(Vec2 p) const { return p.x >= x && p.x <= right() && p.y >= y && p.y <= bottom(); }
    constexpr bool operator==(const Rect&) const = default;
};

inline float snap_to(float v, float grid) { return grid > 0.0f ? std::round(v / grid) * grid : v; }

}  // namespace ember
