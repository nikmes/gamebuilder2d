#pragma once
#include <string>

namespace gb2d::games {

class Game {
public:
    virtual ~Game() = default;

    virtual const char* id() const = 0;
    virtual const char* name() const = 0;

    virtual void init(int width, int height) = 0;
    virtual void update(float dt, int width, int height, bool acceptInput) = 0;
    virtual void render(int width, int height) = 0;
    virtual void unload() = 0;

    virtual void onResize(int width, int height) { reset(width, height); }

    virtual void reset(int width, int height) {
        unload();
        init(width, height);
    }
};

} // namespace gb2d::games
