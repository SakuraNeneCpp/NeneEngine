// 依存性グループB: NeneServer
#pragma once
#include <map>
#include <memory>
#include <string>
#include <SDL3/SDL.h>
#include <NeneObject.hpp>

class NeneServer {
public:
    NeneServer(); // 引数を追加する. 引数に基づいて初期化する.
    ~NeneServer();
    std::shared_ptr<SDL_Window> window;
    std::shared_ptr<SDL_Renderer> renderer;
    std::shared_ptr<NeneViewWorld> view_world;
    std::shared_ptr<NeneCamera> camera;
};

class NeneViewWorld {
public:
    std::map<int, NeneViewObject> objects;
};

class NeneCamera {
public:
    void shoot();
};