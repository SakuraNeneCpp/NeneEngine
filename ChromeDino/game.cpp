// game.cpp
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <stdexcept>
#include <unordered_set>
#include <sstream>
#include <cstdlib>
#include <random>
#include <cmath>
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
        // 念のため必要なサービスが注入されているか確認する
        if (!asset_loader || !path_service || !global_settings) nnthrow("services not ready (asset_loader/path_service/global_settings)");
        if (!collision_world) nnthrow("services not ready (collision_world)");
        // テクスチャ
        sprite_tex_ = asset_loader->get_texture(path_service->resolve("assets/sprites/sprite.png"));
        if (!sprite_tex_) nnthrow("failed to load dino sprite texture");
        w_ = 88.0f;
        h_ = 96.0f;
        // 初期位置
        x_ = 120.0f;
        y_ = global_settings->ground_y - h_;
        // 走りアニメ
        run_src_[0] = SDL_FRect{ 1514.2f, 0.0f, 88.0f, 96.0f };
        run_src_[1] = SDL_FRect{ 1602.2f, 0.0f, 88.0f, 96.0f };
        jump_src_ = run_src_[0];
        // 状態
        on_ground_ = true;
        vy_ = 0.0f;
        anim_accum_ = 0.0f;
        anim_idx_ = 0;
        dead_ = false;
        // CollisionWorld登録（恐竜の凸近似）
        NeneColorPolygon poly;
        poly.owner_name = this->name;
        // 88x96 のローカル座標で、凸多角形の8点近似（時計回り）
        poly.vertices = {
            SDL_FPoint{  6.0f, 28.0f }, // 尻尾/背中側（左上寄り）
            SDL_FPoint{ 18.0f,  8.0f }, // 背中上
            SDL_FPoint{ 60.0f,  6.0f }, // 頭上
            SDL_FPoint{ 80.0f, 30.0f }, // 鼻先〜頭前
            SDL_FPoint{ 84.0f, 72.0f }, // 体前
            SDL_FPoint{ 66.0f, 94.0f }, // 足元前（右下）
            SDL_FPoint{ 22.0f, 94.0f }, // 足元後（左下）
            SDL_FPoint{  6.0f, 72.0f }, // 体後
        };
        poly.position = SDL_FPoint{ x_, y_ };
        poly.color = NenePolygonColor::Blue; // 味方属性
        poly.layer = kLayerPlayer;
        poly.mask  = kMaskPlayerHits;
        poly.enabled = true;
        collider_id_ = collision_world->add_collider(std::move(poly));
    }
    // 外部イベント
    void handle_sdl_event(const SDL_Event& ev) override {
        if (dead_) return;
        // スペースキーでジャンプ
        if (ev.type == SDL_EVENT_KEY_DOWN) {
            if (ev.key.key == SDLK_SPACE || ev.key.key == SDLK_UP) {
                try_jump_();
            }
        }
    }
    // タイムラプス
    void handle_time_lapse(const float& dt) override {
        if (!global_settings) return;
        if (dead_) return;
        // 地上にいれば走るアニメーションを再生し続ける
        if (on_ground_) {
            anim_accum_ += dt;
            if (anim_accum_ >= run_frame_sec_) {
                anim_accum_ = 0.0f;
                anim_idx_ = (anim_idx_ + 1) % 2;
            }
        }
        // ジャンプ中なら物理演算に伴って座標を更新し続ける
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
        // コライダーの位置も更新する
        if (collision_world && collider_id_ != 0) {
            collision_world->set_position(collider_id_, SDL_FPoint{ x_, y_ });
        }
    }
    // レンダリング
    void render(SDL_Renderer* r) override {
        if (!r) return;
        if (!sprite_tex_) return;
        const SDL_FRect* src = (!on_ground_) ? &jump_src_ : &run_src_[anim_idx_];
        SDL_FRect dst { x_, y_, w_, h_ };
        SDL_RenderTexture(r, sprite_tex_, src, &dst);
    }
private:
    void try_jump_() {
        if (!on_ground_) return;
        on_ground_ = false;
        vy_ = -jump_speed_;
    }
    // コライダー設定
    static constexpr std::uint32_t kLayerPlayer   = 1u << 0;
    static constexpr std::uint32_t kLayerObstacle = 1u << 1;
    static constexpr std::uint32_t kMaskPlayerHits = kLayerObstacle;
    // テクスチャ
    SDL_Texture* sprite_tex_ = nullptr;
    SDL_FRect run_src_[2]{};
    SDL_FRect jump_src_{};
    // 位置とサイズ
    float x_ = 0.0f;
    float y_ = 0.0f;
    float w_ = 0.0f;
    float h_ = 0.0f;
    // 状態
    float vy_ = 0.0f;
    bool  on_ground_ = true;
    bool  dead_ = false;
    // アニメーション設定
    float anim_accum_ = 0.0f;
    int   anim_idx_ = 0;
    // 運動設定
    float jump_speed_    = 900.0f;
    float run_frame_sec_ = 0.10f;
    // CollisionWorld登録ID
    NeneCollisionWorld::ColliderId collider_id_ = 0;
};

// 地面
class Ground final : public NeneNode {
public:
    explicit Ground(std::string name) : NeneNode(std::move(name)) {}
protected:
    void init_node() override {
        if (!asset_loader || !path_service || !global_settings) {
            nnthrow("services not ready (asset_loader/path_service/global_settings)");
        }
        sprite_tex_ = asset_loader->get_texture(path_service->resolve("assets/sprites/sprite.png"));
        if (!sprite_tex_) nnthrow("failed to load ground sprite texture");
        float tw = 0.0f, th = 0.0f;
        if (!SDL_GetTextureSize(sprite_tex_, &tw, &th)) {
            nnthrow("SDL_GetTextureSize failed");
        }
        // ここだけ数値調整ポイント（見え方が違ったら kSrcH を変える）
        constexpr float kSrcH = 28.0f;
        src_ = SDL_FRect{ 0.0f, th - kSrcH, tw, kSrcH };
        scroll_ = 0.0f;
        // 背景扱いで奥へ
        set_render_z(-100);
    }
    void handle_time_lapse(const float& dt) override {
        if (!global_settings) return;
        const float speed = global_settings->scroll_speed;
        scroll_ += speed * dt;
        // wrap（src_.w が 0 になることは無い想定）
        if (src_.w > 1.0f) {
            scroll_ = std::fmod(scroll_, src_.w);
            if (scroll_ < 0.0f) scroll_ += src_.w;
        }
    }
    void render(SDL_Renderer* r) override {
        if (!r || !global_settings || !sprite_tex_) return;
        const float ww = static_cast<float>(global_settings->window_w);
        const float y  = global_settings->ground_y - 10;
        const float src_x = scroll_;
        const float src_y = src_.y;
        const float src_h = src_.h;
        const float src_w = src_.w;
        // 画面幅ぶんを「srcのどこから切り出すか」で2回描画（隙間なし）
        const float w1 = (src_x + ww <= src_w) ? ww : (src_w - src_x);
        // 1枚目
        SDL_FRect s1{ src_x, src_y, w1, src_h };
        SDL_FRect d1{ 0.0f,  y,    w1, src_h };
        SDL_RenderTexture(r, sprite_tex_, &s1, &d1);
        // 2枚目（wrap する場合のみ）
        const float w2 = ww - w1;
        if (w2 > 0.0f) {
            SDL_FRect s2{ 0.0f, src_y, w2, src_h };
            SDL_FRect d2{ w1,   y,     w2, src_h };
            SDL_RenderTexture(r, sprite_tex_, &s2, &d2);
        }
    }
private:
    SDL_Texture* sprite_tex_ = nullptr;
    SDL_FRect src_{};
    float scroll_ = 0.0f;
};

// サボテン
#include <random>

// サボテン
class Cactus final : public NeneNode {
public:
    struct Variant {
        SDL_FRect src;
        float w;
        float h;
    };
    Cactus(std::string name, Variant v)
        : NeneNode(std::move(name)), variant_(v) {}
protected:
    void init_node() override {
        if (!asset_loader || !path_service || !global_settings) nnthrow("services not ready (asset_loader/path_service/global_settings)");
        if (!collision_world) nnthrow("services not ready (collision_world)");
        sprite_tex_ = asset_loader->get_texture(path_service->resolve("assets/sprites/sprite.png"));
        if (!sprite_tex_) nnthrow("failed to load sprite texture");
        // 種類ごとのサイズ・画像
        w_ = variant_.w;
        h_ = variant_.h;
        src_ = variant_.src;
        // 画面右外から出現
        x_ = global_settings->window_w + spawn_margin_;
        y_ = global_settings->ground_y - h_;
        // コライダー登録
        NeneColorPolygon poly;
        poly.owner_name = this->name;
        poly.vertices = {
            SDL_FPoint{ 0.0f, 0.0f },
            SDL_FPoint{ w_,   0.0f },
            SDL_FPoint{ w_,   h_   },
            SDL_FPoint{ 0.0f, h_   },
        };
        poly.position = SDL_FPoint{ x_, y_ };
        poly.color = NenePolygonColor::Red; // 敵属性
        poly.layer = kLayerObstacle;
        poly.mask  = kMaskObstacleHits;
        poly.enabled = true;
        collider_id_ = collision_world->add_collider(std::move(poly));
        speed_ = global_settings->scroll_speed;
    }
    void handle_time_lapse(const float& dt) override {
        if (!global_settings) return;
        x_ -= speed_ * dt;
        if (collision_world && collider_id_ != 0) {
            collision_world->set_position(collider_id_, SDL_FPoint{ x_, y_ });
        }
        if (x_ + w_ < -despawn_margin_) {
            send_mail(NeneMail("world", this->name, "despawn", this->name));
        }
    }
    void render(SDL_Renderer* r) override {
        if (!r || !sprite_tex_) return;
        SDL_FRect dst{ x_, y_, w_, h_ };
        SDL_RenderTexture(r, sprite_tex_, &src_, &dst);
    }
private:
    static constexpr std::uint32_t kLayerPlayer   = 1u << 0;
    static constexpr std::uint32_t kLayerObstacle = 1u << 1;
    static constexpr std::uint32_t kMaskObstacleHits = kLayerPlayer;
    Variant variant_;
    SDL_Texture* sprite_tex_ = nullptr;
    SDL_FRect src_{};
    float x_ = 0.0f, y_ = 0.0f, w_ = 0.0f, h_ = 0.0f;
    float speed_ = 0.0f;
    float spawn_margin_ = 40.0f;
    float despawn_margin_ = 60.0f;
    NeneCollisionWorld::ColliderId collider_id_ = 0;
};



// 審判（CollisionWorldを監視し続ける）
class Referee final : public NeneNode {
public:
    explicit Referee(std::string name) : NeneNode(std::move(name)) {}
protected:
    void init_node() override {
        if (!collision_world) nnthrow("services not ready (collision_world)");
    }
    void handle_time_lapse(const float& dt) override {
        (void)dt;
        if (!collision_world) nnthrow("collision world lost");
        // まだターゲットIDが無いなら探す
        // (生成順など都合で最初の数フレームだけターゲットが無くても問題ない)
        if (target_id_ == 0) {
            target_id_ = find_target_id_();
            if (target_id_ == 0) return;
        }
        NeneColorPolygon* target = collision_world->find(target_id_);
        // ターゲットを見つけられなかったとき
        // (理論上起きないはず)
        if (!target) {
            nnerr("target lost");
            target_id_ = 0;
            prev_.clear();
            return;
        }
        std::unordered_set<Id> now;
        // 衝突判定
        if (auto hit = collision_world->detect_collision(*target)) {
            const NeneColorPolygon& other = hit->get();
            now.insert(other.id);
            if (prev_.find(other.id) == prev_.end()) {
                nnlog("collision detected");
                std::ostringstream oss;
                oss << "target=" << target_name_
                    << " other=" << other.owner_name
                    << " color=" << static_cast<int>(other.color);
                send_mail(NeneMail(this->name, "collision_detected", oss.str()));
            }
        }
        prev_.swap(now);
    }
private:
    using Id = NeneCollisionWorld::ColliderId;
    Id find_target_id_() const {
        if (!collision_world) return 0;
        for (const auto& c : collision_world->colliders()) {
            if (!c.enabled) continue;
            if (c.owner_name == target_name_) return c.id;
        }
        return 0;
    }
    std::unordered_set<Id> prev_;
    std::string target_name_ = "dino";
    Id target_id_ = 0;
};


// ワールド
class World final : public NeneNode {
public:
    explicit World(std::string name) : NeneNode(std::move(name)) {}
protected:
    void init_node() override {
        add_child(std::make_unique<Ground>("ground"));
        add_child(std::make_unique<Dino>("dino"));
        add_child(std::make_unique<Referee>("referee"));
        spawn_accum_ = 0.0f;
        obstacle_seq_ = 0;
        // 最初の出現までの時間（0.8〜1.6秒）
        next_spawn_in_ = frand_(0.8f, 1.6f);
    }
    void handle_time_lapse(const float& dt) override {
        if (global_settings) {
            float& score = global_settings->ensuref("score", 0.0f);
            score += dt * 100.0f; // 毎秒100点
        }
        spawn_accum_ += dt;
        if (spawn_accum_ >= next_spawn_in_) {
            spawn_accum_ = 0.0f;
            spawn_obstacle_();
            // 次の間隔をランダムに（0.7〜1.7秒）
            next_spawn_in_ = frand_(0.7f, 1.7f);
        }
    }
    void handle_nene_mail(const NeneMail& mail) override {
        // 障害物を消去
        if (mail.subject == "despawn") remove_child(mail.body);
        // Referee のブロードキャストを受けた時
        if (mail.subject == "collision_detected") {
            // World 以下の time_lapse, sdl_event パルスを遮断
            this->valve_time_lapse = false;
            this->valve_sdl_event = false;
            // 障害物の生成を停止
            spawn_accum_ = 0.0f;
            // ゲームオーバーに移行
            if (global_settings) global_settings->setf("game_over", 1.0f);
            return;
        }
    }
private:
    void spawn_obstacle_() {
        std::uniform_int_distribution<int> dist(0, static_cast<int>(cactus_variants_.size()) - 1);
        const auto v = cactus_variants_[dist(rng_)];
        const std::string name = "cactus_" + std::to_string(obstacle_seq_++);
        add_child(std::make_unique<Cactus>(name, v));
    }
    static float frand_(float a, float b) {
        const float t = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
        return a + (b - a) * t;
    }
    std::mt19937 rng_{ std::random_device{}() };
    // サボテンの種類
    float small_base_x = 446.0f;
    float small_base_y = 0.0f;
    float small_w = 34.0f;
    float small_h = 70.0f;
    float big_base_x = 650.0f;
    float big_base_y = 0.0f;
    float big_w = 50.5f;
    float big_h = 96.0f;
    std::vector<Cactus::Variant> cactus_variants_ = {
        // 小
        { SDL_FRect{ small_base_x+small_w*0, small_base_y, small_w, small_h }, small_w, small_h },
        { SDL_FRect{ small_base_x+small_w*1, small_base_y, small_w, small_h }, small_w, small_h },
        { SDL_FRect{ small_base_x+small_w*2, small_base_y, small_w, small_h }, small_w, small_h },
        { SDL_FRect{ small_base_x+small_w*3, small_base_y, small_w, small_h }, small_w, small_h },
        { SDL_FRect{ small_base_x+small_w*4, small_base_y, small_w, small_h }, small_w, small_h },
        { SDL_FRect{ small_base_x+small_w*5, small_base_y, small_w, small_h }, small_w, small_h },
        // 大
        // { SDL_FRect{ big_base_x+big_w*0, big_base_y, big_w, big_h }, big_w, big_h },
        // { SDL_FRect{ big_base_x+big_w*1, big_base_y, big_w, big_h }, big_w, big_h },
        // { SDL_FRect{ big_base_x+big_w*2, big_base_y, big_w, big_h }, big_w, big_h },
        // { SDL_FRect{ big_base_x+big_w*3, big_base_y, big_w, big_h }, big_w, big_h },
        // 3つ組
        // { SDL_FRect{ big_base_x+big_w*4, big_base_y, 100, big_h }, 100, big_h },

    };
    float spawn_accum_ = 0.0f;
    float spawn_interval_ = 1.35f;
    int obstacle_seq_ = 0;
    float next_spawn_in_ = 1.0f;
};


// オーバーレイ（スコア + Game Over）
class Overlay final : public NeneNode {
public:
    explicit Overlay(std::string name) : NeneNode(std::move(name)) {}
protected:
    void init_node() override {
        if (!font_loader || !path_service || !global_settings) nnthrow("services not ready (font_loader/path_service/global_settings)");
        font_path_ = path_service->resolve("assets/fonts/NotoSansJP-Regular.ttf");
        // 固定テキストは一度だけ作ればOK
        game_over_tex_ = font_loader->get_text_texture(font_path_, 64, "Game Over", SDL_Color{255,255,255,255});
        restart_tex_ = font_loader->get_text_texture(font_path_, 24, "Press Space to Restart", SDL_Color{255,255,255,255});
        // スコア初期テクスチャ
        update_score_texture_(0);
        // 初期状態
        last_score_int_ = 0;
        // 手前に表示される
        set_render_z(1000);
        // 点滅アニメーション設定
        blink_accum_ = 0.0f;
        press_visible_ = true;
    }
    void handle_time_lapse(const float& dt) override {
        if (!global_settings) return;
        // スコア表示更新（score が変化したときだけテクスチャ更新）
        const int score_i = static_cast<int>(global_settings->getf("score", 0.0f));
        if (score_i != last_score_int_) {
            last_score_int_ = score_i;
            update_score_texture_(score_i);
        }
        // Game Over 中だけ「Press...」を点滅
        const bool game_over = (global_settings->getf("game_over", 0.0f) > 0.5f);
        if (game_over) {
            blink_accum_ += dt;
            if (blink_accum_ >= 0.5f) {
                blink_accum_ = 0.0f;
                press_visible_ = !press_visible_;
            }
        } else {
            // Game Overじゃないときは常に表示状態に戻す
            blink_accum_ = 0.0f;
            press_visible_ = true;
        }
    }
    void render(SDL_Renderer* r) override {
        if (!r || !global_settings) return;
        if (!score_tex_) return;
        int w = 0, h = 0;
        if (!SDL_GetRenderOutputSize(r, &w, &h)) return;
        // スコア
        float sw = 0.0f, sh = 0.0f;
        SDL_GetTextureSize(score_tex_, &sw, &sh);
        const float pad = 16.0f;
        SDL_FRect score_dst{
            static_cast<float>(w) - pad - sw,
            pad,
            sw, sh
        };
        SDL_RenderTexture(r, score_tex_, nullptr, &score_dst);
        // Game Over文字
        const bool game_over = (global_settings->getf("game_over", 0.0f) > 0.5f);
        if (!game_over) return;
        if (game_over_tex_) {
            float gw = 0.0f, gh = 0.0f;
            SDL_GetTextureSize(game_over_tex_, &gw, &gh);
            SDL_FRect go_dst{
                (static_cast<float>(w) - gw) * 0.5f,
                (static_cast<float>(h) - gh) * 0.5f - 40.0f,
                gw, gh
            };
            SDL_RenderTexture(r, game_over_tex_, nullptr, &go_dst);
        }
        if (restart_tex_ && press_visible_) {
            float rw = 0.0f, rh = 0.0f;
            SDL_GetTextureSize(restart_tex_, &rw, &rh);
            SDL_FRect rs_dst{
                (static_cast<float>(w) - rw) * 0.5f,
                (static_cast<float>(h) - rh) * 0.5f + 40.0f,
                rw, rh
            };
            SDL_RenderTexture(r, restart_tex_, nullptr, &rs_dst);
        }
    }
    void handle_sdl_event(const SDL_Event& ev) override {
        if (!global_settings) return;
        const bool game_over = (global_settings->getf("game_over", 0.0f) > 0.5f);
        if (!game_over) return;
        if (ev.type == SDL_EVENT_KEY_DOWN) {
            if (ev.key.key == SDLK_SPACE) {
                // スイッチ: PlayScene → PlayScene (これでリセットできる)
                send_mail(NeneMail("scene_switch", this->name, "switch_to", "play_scene"));
            }
        }
    }
private:
    void update_score_texture_(int score_i) {
        // スコアの表示(5桁)
        std::string s = std::to_string(score_i);
        if (s.size() < 5) s = std::string(5 - s.size(), '0') + s;
        score_tex_ = font_loader->get_text_texture(
            font_path_, 28, s, SDL_Color{255,255,255,255});
    }
    std::string font_path_;
    SDL_Texture* score_tex_ = nullptr;
    SDL_Texture* game_over_tex_ = nullptr;
    SDL_Texture* restart_tex_ = nullptr;
    int last_score_int_ = 0;
    float blink_accum_ = 0.0f;
    bool  press_visible_ = true;
};


// プレイシーン
class PlayScene final : public NeneNode {
public:
    explicit PlayScene(std::string name) : NeneNode(std::move(name)) {}
protected:
    void init_node(){
        if (global_settings) {
            global_settings->setf("score", 0.0f);
            global_settings->setf("game_over", 0.0f);
        }
        if (collision_world) collision_world->clear(); // リセットのためにコライダーをクリア
        else nnerr("no collision world");
        add_child(std::make_unique<World>("world"));
        add_child(std::make_unique<Overlay>("overlay"));
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
        // 「Press...」文字
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
        // 「Press...」文字サイズ
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
        // 「Press...」を少し下に、中央寄せ、点滅
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



