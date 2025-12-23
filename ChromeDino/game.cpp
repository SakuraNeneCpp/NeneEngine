// game.cpp
#include <iostream>
#include <memory>
#include <string>
#include <stdexcept>
#include <SDL3/SDL.h>
#include <NeneEngine/NeneNode.hpp>

static std::string asset_path(const char* rel_from_assets_root){
    const char* base = SDL_GetBasePath();
    if (!base) {
        SDL_Log("SDL_GetBasePath failed: %s", SDL_GetError());
        // いったん相対で返す（最低限のフォールバック）
        return std::string("assets/") + rel_from_assets_root;
    }
    // SDL3: base は SDL が内部キャッシュするので SDL_free しない :contentReference[oaicite:1]{index=1}
    return std::string(base) + rel_from_assets_root;
}


// タイトルシーン
class TitleScene final : public NeneNode {
public:
    explicit TitleScene(std::string name)
        : NeneNode(std::move(name)) {}
protected:
    void init_node() override {

        // 共有サービスは add_child 時に親から引き継がれている想定
        if (!asset_loader || !font_loader) {
            throw std::runtime_error("[TitleScene] services not ready (asset_loader/font_loader)");
        }
        // スプライト
        sprite_tex_ = asset_loader->get_texture(asset_path("assets/sprites/sprite.png"));
        // フォント
        font_path_ = asset_path("assets/fonts/NotoSansJP-Regular.ttf");
        // タイトル文字
        title_tex_ = font_loader->get_text_texture(font_path_, 56, "ChromeDino", SDL_Color{255, 255, 255, 255});
    }
    void render(SDL_Renderer* r) override {
        if (!r) return;
        if (!sprite_tex_ || !title_tex_) {
            std::cout << "no tex" << std::endl;
            return;
        }
        // 画面サイズ
        int w = 0, h = 0;
        if (!SDL_GetRenderOutputSize(r, &w, &h)) {
            return;
        }
        // 恐竜 (テクスチャアトラス内)
        const SDL_FRect dino_src { 1515.0f, 0.0f, 88.0f, 96.0f };
        const float dino_w = 88.0f;
        const float dino_h = 96.0f;
        // 文字のテクスチャサイズ
        float text_w = 0.0f, text_h = 0.0f;
        if (!SDL_GetTextureSize(title_tex_, &text_w, &text_h)) {
            text_w = 0.0f;
            text_h = 0.0f;
        }
        // 横並びレイアウト：中央寄せ
        const float gap = 28.0f;
        const float total_w = dino_w + gap + text_w;
        const float group_h = (dino_h > text_h) ? dino_h : text_h;
        const float x0 = (static_cast<float>(w) - total_w) * 0.5f;
        const float y0 = (static_cast<float>(h) * 0.5f) - (group_h * 0.5f); // 画面のちょうど半分の高さ
        SDL_FRect dino_dst { x0, y0 + (group_h - dino_h) * 0.5f, dino_w, dino_h };
        SDL_FRect text_dst { x0 + dino_w + gap, y0 + (group_h - text_h) * 0.5f, text_w, text_h };
        SDL_RenderTexture(r, sprite_tex_, &dino_src, &dino_dst);
        SDL_RenderTexture(r, title_tex_, nullptr, &text_dst);
    }
private:
    SDL_Texture* sprite_tex_ = nullptr;
    SDL_Texture* title_tex_  = nullptr;
    std::string font_path_;
};

// シーンスイッチ
class SceneSwitch final : public NeneNode {
public:
    explicit SceneSwitch(std::string name)
        : NeneNode(std::move(name)) {}
protected:
    void init_node() override {
        // タイトルシーンを子に持つ
        add_child(std::make_unique<TitleScene>("title_scene"));
    }
    void render(SDL_Renderer*) override {}
};

// ルートノード
class Game final : public NeneRoot {
public:
    Game()
    : NeneRoot(
        "game",
        "ChromeDino",
        960, 540,
        0, // リサイズ不可
        100, 100,
        asset_path("assets/icons/T-Rex.svg").c_str()
      )
    {}
protected:
    void init_node() override {
        // シーンスイッチを子に持つ
        add_child(std::make_unique<SceneSwitch>("scene_switch"));
    }
};
std::unique_ptr<NeneRoot> create_game() {
    return std::make_unique<Game>();
}



