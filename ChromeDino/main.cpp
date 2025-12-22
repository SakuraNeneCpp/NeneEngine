// main.cpp
#include <memory>
#include <SDL3/SDL_main.h>
#include <NeneEngine/NeneNode.hpp>

// ゲームを生成
// create_game() については game.cpp を参照
std::unique_ptr<NeneRoot> create_game();

int main(int, char**) {
    auto game = create_game();
    return game->run();
}
