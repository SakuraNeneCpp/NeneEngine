// 依存性グループB: NeneServer
#pragma once
#include <map>
#include <memory>
#include <string>
#include <SDL3/SDL.h>
#include <NeneObject.hpp>

class NeneServer {
public:
    NeneServer();
    ~NeneServer();
    NeneViewWorld view_world;
    NeneCamera camera;
};

class NeneViewWorld {
public:
    std::map<int, NeneViewObject> objects;
};

class NeneCamera {
public:
    int x = 0;
    int y = 0;
    float scale = 1.0;
    float rotate = 0;
    void shoot(NeneViewWorld&, SDL_Renderer&);
};