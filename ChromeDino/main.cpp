// ChromeDino/main.cpp
#include <memory>
#include <SDL3/SDL_main.h>          // SDL3では推奨 :contentReference[oaicite:1]{index=1}
#include <NeneEngine/NeneNode.hpp>

// game.cpp 側の関数を参照
std::unique_ptr<NeneRoot> create_game();

int main(int, char**) {
    auto game = create_game();
    return game->run();
}
