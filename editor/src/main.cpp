// Ember Editor entry point.
//
//   ./build/ember                          open levels/demo.json (or the built-in demo)
//   ./build/ember path/to/level.json       open a specific level
//
// Automation flags (used to make the README screenshots):
//   --screenshot out.png --frames N        render N frames, save the window, quit
//   --play                                 start in Play mode
//   --hold-right                           simulate holding the Right arrow
//   --select NAME                          select the entity called NAME

#include "editor_app.hpp"

#include <cstdlib>
#include <string>

int main(int argc, char** argv) {
    editor::Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
        if (a == "--screenshot")      opt.screenshot = next();
        else if (a == "--frames")     opt.screenshot_frame = std::atoi(next().c_str());
        else if (a == "--play")       opt.start_playing = true;
        else if (a == "--hold-right") opt.hold_right = true;
        else if (a == "--select")     opt.select = next();
        else if (a == "--size")       { opt.width = std::atoi(next().c_str()); opt.height = std::atoi(next().c_str()); }
        else                          opt.level_path = a;
    }
    editor::EditorApp app(opt);
    return app.run();
}
