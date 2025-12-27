// main.cpp
#include <memory>
#include <SDL3/SDL_main.h>
#include <NeneEngine/NeneNode.hpp>

// ゲームを生成
std::unique_ptr<NeneRoot> create_game();

int main(int, char**) {
    try {
        auto game = create_game();
        return game->run();
    } catch (const std::exception& e) {
        SDL_Log("FATAL: %s", e.what());
        return 1;
    } catch (...) {
        SDL_Log("FATAL: unknown exception");
        return 1;
    }
}
