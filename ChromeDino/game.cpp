// ChromeDino/game.cpp
#include <memory>
#include <NeneEngine/NeneNode.hpp>

// いまは「ウィンドウを立ち上げるだけ」なので中身は空でOK
class ChromeDinoGame final : public NeneRoot {
public:
    ChromeDinoGame()
    : NeneRoot(
        "game",                         // ルートノード名
        "Chrome Dino (NeneEngine)",     // ウィンドウタイトル
        960, 540,                       // w, h
        SDL_WINDOW_RESIZABLE,           // flags
        100, 100,                       // x, y（まず固定でOK）
        ""                              // icon_path（空ならスキップ）
      )
    {}

protected:
    void init_node() override {
        // TODO: 次の段階で scene_switch / play_scene / dino / ui_canvas / score などを add_child します
    }
};

// main.cpp から呼ぶためのファクトリ関数
std::unique_ptr<NeneRoot> create_game() {
    return std::make_unique<ChromeDinoGame>();
}
