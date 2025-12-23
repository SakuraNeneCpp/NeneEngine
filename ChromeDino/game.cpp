// game.cpp
#include <iostream>
#include <memory>
#include <string>
#include <stdexcept>
#include <SDL3/SDL.h>
#include <NeneEngine/NeneNode.hpp>

// assets/が.exeの隣にある想定
// assets/から欲しいアセットへのパスを渡すと絶対パスの残りを補う.
// 引数: "assets/..."
static std::string asset_path(const char* rel_from_assets_root){
    const char* base = SDL_GetBasePath();
    if (!base) { // 最悪, 相対パスを返す
        SDL_Log("SDL_GetBasePath failed: %s", SDL_GetError());
        return rel_from_assets_root;
    }
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
        // Press...文字
        press_tex_ = font_loader->get_text_texture(font_path_, 24, "Press Space to Start", SDL_Color{255, 255, 255, 255});
    }
    void render(SDL_Renderer* r) override {
        if (!r) return;
        if (!sprite_tex_ || !title_tex_ || !press_tex_) return;
        // 画面サイズ
        int w = 0, h = 0;
        if (!SDL_GetRenderOutputSize(r, &w, &h)) {
            return;
        }
        // 恐竜 (テクスチャアトラス内)
        const SDL_FRect dino_src { 1514.2f, 0.0f, 88.0f, 96.0f };
        const float dino_w = 88.0f;
        const float dino_h = 96.0f;
        // タイトル文字のテクスチャサイズ
        float text_w = 0.0f, text_h = 0.0f;
        if (!SDL_GetTextureSize(title_tex_, &text_w, &text_h)) {
            text_w = 0.0f;
            text_h = 0.0f;
        }
        // Press...文字サイズ
        float press_w = 0.0f, press_h = 0.0f;
        SDL_GetTextureSize(press_tex_, &press_w, &press_h);
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
        // 下段（Press...）を少し下に、中央寄せ、点滅
        if (press_visible_) {
            const float press_x = (static_cast<float>(w) - press_w) * 0.5f;
            const float press_y = y0 + group_h + 40.0f;
            SDL_FRect press_dst{ press_x, press_y, press_w, press_h };
            SDL_RenderTexture(r, press_tex_, nullptr, &press_dst);
        }
    }
    void handle_sdl_event(const SDL_Event& ev) override
    {
        if (ev.type == SDL_EVENT_KEY_DOWN) {
            if (ev.key.key == SDLK_SPACE) {
                // scene_switch にメールでシーン切替要求
                // (to, from, subject, body)
                send_mail(NeneMail("scene_switch", this->name, "switch_scene", "play_scene"));
            }
        }
    }
    void handle_time_lapse(const float& dt) override
    {
        blink_accum_ += dt;
        if (blink_accum_ >= 0.5f) {        // 0.5秒ごとにON/OFF
            blink_accum_ = 0.0f;
            press_visible_ = !press_visible_;
        }
    }
private:
    SDL_Texture* sprite_tex_ = nullptr;
    SDL_Texture* title_tex_  = nullptr;
    SDL_Texture* press_tex_  = nullptr;
    std::string font_path_;
    float blink_accum_ = 0.0f;
    bool  press_visible_ = true;
};

// プレイシーン
class PlayScene final : public NeneNode {
public:
    explicit PlayScene(std::string name) : NeneNode(std::move(name)) {}

protected:
    void render(SDL_Renderer* r) override
    {
        // とりあえず何もしない
        (void)r;
    }
};

// シーンスイッチ
class SceneSwitch final : public NeneNode {
public:
    explicit SceneSwitch(std::string name)
        : NeneNode(std::move(name)) {}

protected:
    void init_node() override
    {
        auto t = std::make_unique<TitleScene>("title_scene");
        title_ = t.get();
        add_child(std::move(t));

        auto p = std::make_unique<PlayScene>("play_scene");
        play_ = p.get();
        add_child(std::move(p));

        // 初期はタイトルだけ有効、プレイは無効（＝伝播しない）
        title_->set_active(true);
        play_->set_active(false);

        state_ = State::Title;
    }

    void handle_nene_mail(const NeneMail& mail) override
    {
        // scene_switch 宛の "switch_scene" を受け取ったら切替
        if (mail.subject == "switch_scene" && mail.body == "play_scene") {
            switch_to_play();
        }
        if (mail.subject == "switch_scene" && mail.body == "title_scene") {
            switch_to_title();
        }
    }

    void render(SDL_Renderer* r) override
    {
        if (!r) return;
        // 背景（黒）
        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
        SDL_RenderClear(r);
        // 実際の内容は、active な子にだけ伝播して描かれる
    }

private:
    enum class State { Title, Play };
    State state_ = State::Title;

    TitleScene* title_ = nullptr;
    PlayScene*  play_  = nullptr;

    void switch_to_play()
    {
        if (state_ == State::Play) return;
        title_->set_active(false);
        play_->set_active(true);
        state_ = State::Play;
        SDL_Log("[scene_switch] switched to play_scene");
    }

    void switch_to_title()
    {
        if (state_ == State::Title) return;
        play_->set_active(false);
        title_->set_active(true);
        state_ = State::Title;
        SDL_Log("[scene_switch] switched to title_scene");
    }
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



