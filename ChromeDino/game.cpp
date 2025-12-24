// game.cpp
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <stdexcept>
#include <SDL3/SDL.h>
#include <NeneEngine/NeneNode.hpp>

// 恐竜
class Dino final : public NeneNode {
public:
    explicit Dino(std::string name) : NeneNode(std::move(name)) {}

    SDL_FRect hitbox() const { return SDL_FRect{ x_, y_, w_, h_ }; }

    void set_dead(bool v) { dead_ = v; }
    bool is_dead() const { return dead_; }

protected:
    void init_node() override {
        // services
        if (!asset_loader || !path_service || !global_settings) {
            nnthrow("services not ready (asset_loader/path_service/global_settings)");
        }
        if (!collision_world_aabb_n) {
            nnthrow("services not ready (collision_world_aabb_n)");
        }

        sprite_tex_ = asset_loader->get_texture(path_service->resolve("assets/sprites/sprite.png"));
        if (!sprite_tex_) nnthrow("failed to load dino sprite texture");

        // サイズ
        w_ = 88.0f;
        h_ = 96.0f;

        // 初期位置
        x_ = 120.0f;
        y_ = global_settings->ground_y - h_;

        // 走りアニメ
        run_src_[0] = SDL_FRect{ 1514.2f, 0.0f, 88.0f, 96.0f };
        run_src_[1] = SDL_FRect{ 1602.2f, 0.0f, 88.0f, 96.0f };
        jump_src_ = run_src_[0];

        on_ground_ = true;
        vy_ = 0.0f;
        anim_accum_ = 0.0f;
        anim_idx_ = 0;
        dead_ = false;

        // --- CollisionWorld 登録 ---
        NeneCollisionWorld2D_AABB_N::ColliderDesc desc;
        desc.owner_name = this->name;     // "dino"
        desc.aabb = hitbox();             // 初期AABB
        desc.layer = kLayerPlayer;
        desc.mask  = kMaskPlayerHits;     // 障害物だけ当てたい、など
        desc.enabled = true;
        collider_id_ = collision_world_aabb_n->create_collider(desc);

        // ターゲットにする（Dinoだけ衝突検出したい前提）
        collision_world_aabb_n->set_target(collider_id_);
    }

    void handle_time_lapse(const float& dt) override {
        if (!global_settings) return;
        if (dead_) return;

        // 走りアニメ
        if (on_ground_) {
            anim_accum_ += dt;
            if (anim_accum_ >= run_frame_sec_) {
                anim_accum_ = 0.0f;
                anim_idx_ = (anim_idx_ + 1) % 2;
            }
        }

        // 物理
        vy_ += global_settings->gravity * dt;
        y_  += vy_ * dt;

        const float ground_y = global_settings->ground_y - h_;
        if (y_ >= ground_y) {
            y_ = ground_y;
            vy_ = 0.0f;
            on_ground_ = true;
        } else {
            on_ground_ = false;
        }

        // --- CollisionWorldへAABB更新 ---
        if (collision_world_aabb_n && collider_id_ != 0) {
            collision_world_aabb_n->set_aabb(collider_id_, hitbox());
        }
    }

    void render(SDL_Renderer* r) override {
        if (!r) return;
        if (!sprite_tex_) return;

        const SDL_FRect* src = (!on_ground_) ? &jump_src_ : &run_src_[anim_idx_];
        SDL_FRect dst { x_, y_, w_, h_ };
        SDL_RenderTexture(r, sprite_tex_, src, &dst);
    }

    // エンジン側に「ノード破棄フック」があるならそこで destroy_collider すると安全
    // 例: void on_destroy() override { ... } みたいな
    // なければ一旦は「Dinoは消えない」前提でもOK

private:
    void try_jump_() {
        if (!on_ground_) return;
        on_ground_ = false;
        vy_ = -jump_speed_;
    }

private:
    // ---- collision filters (例) ----
    static constexpr std::uint32_t kLayerPlayer = 1u << 0;
    static constexpr std::uint32_t kLayerObstacle = 1u << 1;
    static constexpr std::uint32_t kMaskPlayerHits = kLayerObstacle;

private:
    SDL_Texture* sprite_tex_ = nullptr;
    SDL_FRect run_src_[2]{};
    SDL_FRect jump_src_{};

    float x_ = 0.0f;
    float y_ = 0.0f;
    float w_ = 0.0f;
    float h_ = 0.0f;

    float vy_ = 0.0f;
    bool  on_ground_ = true;
    bool  dead_ = false;

    float anim_accum_ = 0.0f;
    int   anim_idx_ = 0;

    float jump_speed_    = 900.0f;
    float run_frame_sec_ = 0.10f;

    // --- CollisionWorld 登録ID ---
    NeneCollisionWorld2D_AABB_N::ColliderId collider_id_ = 0;

    // NeneNode に注入されている想定（あなたの実装名に合わせる）
    // NeneCollisionWorld2D_AABB_N* collision_world_aabb_n = nullptr;
};

// 障害物

// 審判
#include <unordered_set>
#include <sstream>

// 審判（衝突を監視して通知）
class Referee final : public NeneNode {
public:
    explicit Referee(std::string name) : NeneNode(std::move(name)) {}

protected:
    void init_node() override {
        // NeneServerにCollisionWorldを追加して、NeneNodeに注入されている想定
        // 例: NeneNode::collision_world_aabb_n のようなメンバがある前提
        if (!collision_world_aabb_n) {
            nnthrow("services not ready (collision_world_aabb_n)");
        }
    }

    void handle_time_lapse(const float& dt) override {
        (void)dt;
        if (!collision_world_aabb_n) return;

        const auto& hits = collision_world_aabb_n->step();

        // 1) いま当たっている相手集合を作る
        std::unordered_set<Id> now;
        now.reserve(hits.size());

        for (const auto& h : hits) {
            now.insert(h.other_id);

            // 2) 新規衝突（enter）のみ通知（毎フレームspamしたくないので）
            if (prev_.find(h.other_id) == prev_.end()) {
                // bodyは自由。ここでは "target=dino other=xxx" 形式にする
                std::ostringstream oss;
                oss << "target=" << target_name_ << " other=" << h.other_owner;

                broadcast_("collision_enter", oss.str());
            }
        }

        // 3) （任意）離脱通知が欲しければ
        // for (auto old_id : prev_) {
        //     if (now.find(old_id) == now.end()) {
        //         auto owner = collision_world_aabb_n->owner_name(old_id);
        //         std::ostringstream oss;
        //         oss << "target=" << target_name_ << " other=" << (owner ? *owner : "(unknown)");
        //         broadcast_("collision_exit", oss.str());
        //     }
        // }

        prev_.swap(now);
    }

private:
    using Id = NeneCollisionWorld2D_AABB_N::ColliderId;

    void broadcast_(const std::string& subject, const std::string& body) {
        // ブロードキャスト宛先（エンジン仕様に合わせて変えてOK）
        send_mail(NeneMail("*", this->name, subject, body));
    }

private:
    std::unordered_set<Id> prev_;
    std::string target_name_ = "dino"; // 必要ならRefereeに渡してもOK

    // これがNeneNodeに注入されている想定（名前はあなたの実装に合わせて）
    // NeneNode側に public/protected で用意してください:
    //   NeneCollisionWorld2D_AABB_N* collision_world_aabb_n = nullptr;
};


// オーバーレイ

// スコア

// ワールド
class World final : public NeneNode {
public:
    explicit World(std::string name) : NeneNode(std::move(name)) {}

protected:
    void init_node(){
        add_child(std::make_unique<Dino>("dino"));
        add_child(std::make_unique<Referee>("referee"));
    }
};


// UI

// プレイシーン
class PlayScene final : public NeneNode {
public:
    explicit PlayScene(std::string name) : NeneNode(std::move(name)) {}

protected:
    void init_node(){
        add_child(std::make_unique<World>("world"));
    }
};

// タイトルシーン
class TitleScene final : public NeneNode {
public:
    explicit TitleScene(std::string name) : NeneNode(std::move(name)) {}
protected:
    void init_node() override {
        // 共有サービスは add_child 時に親から引き継がれている想定
        if (!asset_loader || !font_loader || !path_service) nnthrow("services not ready (asset_loader/font_loader/path_service)");
        // スプライト
        sprite_tex_ = asset_loader->get_texture(path_service->resolve("assets/sprites/sprite.png"));
        // フォント
        font_path_ = path_service->resolve("assets/fonts/NotoSansJP-Regular.ttf");
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
        if (!SDL_GetRenderOutputSize(r, &w, &h)) return;
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
                send_mail(NeneMail("scene_switch", this->name, "switch_to", "play_scene"));
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

// シーンスイッチ
class SceneSwitch final : public NeneSwitch {
public:
    explicit SceneSwitch(std::string name) : NeneSwitch(std::move(name)) {}

protected:
    void init_node() override {
        register_node("title_scene", [] {
            return std::make_unique<TitleScene>("title_scene");
        });
        register_node("play_scene", [] {
            return std::make_unique<PlayScene>("play_scene");
        });
        set_initial_node("title_scene");
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
        SDL_WINDOW_RESIZABLE,
        100, 100,
        icon_path().c_str()
      )
    {}
protected:
    void init_node() override {
        // シーンスイッチを生成.
        add_child(std::make_unique<SceneSwitch>("scene_switch"));
    }
private:
    static const std::string& icon_path() {
        static std::string p = PathService::resolve_base("assets/icons/T-Rex.svg");
        return p;
    }
};
std::unique_ptr<NeneRoot> create_game() {
    return std::make_unique<Game>();
}



