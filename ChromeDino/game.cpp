// game.cpp
#include <memory>
#include <NeneEngine/NeneNode.hpp>

class Game final : public NeneRoot {
public:
    Game()
    : NeneRoot(
        "game",                 // NeneNodeとしての名前
        "Game Title",           // ウィンドウタイトル
        960, 540,               // 横幅, 縦幅
        SDL_WINDOW_RESIZABLE,   // リサイズ設定 (ウィンドウの端をつかんでサイズを変えられるかどうか. 今はできるようになってる)
        100, 100,               // ウィンドウ出現位置
        ""                      // アイコン画像へのパス (空ならスキップされる)
      )
    {}

protected:
    void init_node() override {
        // ここで子ノードを生成する
        // いまはウィンドウを生成するだけなので何もしない
    }
};

// main.cpp から呼ぶためのファクトリ関数
// main.cpp で生成してもいいけど, 結合を疎にするためにこう書く方が理想的
std::unique_ptr<NeneRoot> create_game() {
    return std::make_unique<Game>();
}
