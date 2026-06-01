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
#include <vector>
#include <algorithm>
#include <functional>
#include <mutex>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <NeneEngine/NeneNode.hpp>

static constexpr int kStageCount = 3;
static constexpr float kStageClearScore = 1000.0f;
static constexpr const char* kStageProgressSlot = "stage_progress";
static constexpr const char* kStageProgressNode = "stage_progress";
static constexpr const char* kSettingsSlot = "_settings";
static constexpr const char* kSettingSeVolume = "se_volume";
static constexpr const char* kSettingBgmVolume = "bgm_volume";
static constexpr const char* kSettingCursorVisible = "cursor_visible";
static constexpr const char* kSettingDinoControl = "dino_control";
static constexpr const char* kFlagPaused = "paused";
static constexpr const char* kGameFontPath = "assets/fonts/NotoSansJP-Regular.ttf";
static constexpr const char* kSpriteSheetPath = "assets/sprites/sprite.png";
static constexpr const char* kSplashImagePath = "assets/splashscreen.png";
static constexpr const char* kSeCursorMovePath = "assets/se/soundeffect-lab/cursor_move.mp3";
static constexpr const char* kSeConfirmPath = "assets/se/soundeffect-lab/confirm.mp3";
static constexpr const char* kSeDinoJumpPath = "assets/se/fc_sitefactory/se_jump_001.wav";
static constexpr const char* kStageBgmPaths[kStageCount] = {
    "assets/bgm/maou/maou_bgm_8bit17.mp3",
    "assets/bgm/maou/maou_bgm_8bit27.mp3",
    "assets/bgm/maou/maou_14_shining_star.mp3",
};
static constexpr const char* kStageSceneNames[kStageCount] = {
    "stage_1_scene",
    "stage_2_scene",
    "stage_3_scene",
};

static int clamp_stage(int stage) {
    if (stage < 1) return 1;
    if (stage > kStageCount) return kStageCount;
    return stage;
}

static const char* stage_scene_name(int stage) {
    return kStageSceneNames[clamp_stage(stage) - 1];
}

static std::string stage_clear_key(int stage) {
    return "stage_" + std::to_string(stage) + "_cleared";
}

static const char* stage_bgm_path(int stage) {
    return kStageBgmPaths[clamp_stage(stage) - 1];
}

static bool is_winning_run_stage(int stage) {
    return clamp_stage(stage) == 3;
}

static bool is_winning_run_stage(const NeneBlackboard* blackboard) {
    if (!blackboard) return false;
    return is_winning_run_stage(static_cast<int>(blackboard->getf("selected_stage", 1.0f)));
}

static float sound_volume_gain(const NeneBlackboard& blackboard, const char* key) {
    const float value = blackboard.getf(key, 5.0f);
    return std::clamp(value / 9.0f, 0.0f, 1.0f);
}

static void apply_sound_settings(NeneSoundLoader& sound_loader, const NeneBlackboard& blackboard) {
    sound_loader.set_se_volume(sound_volume_gain(blackboard, kSettingSeVolume));
    sound_loader.set_bgm_volume(sound_volume_gain(blackboard, kSettingBgmVolume));
}

static void play_sound_effect(const std::shared_ptr<NeneSoundLoader>& sound_loader,
                              const std::shared_ptr<PathService>& path_service,
                              const std::shared_ptr<NeneBlackboard>& blackboard,
                              const char* rel_path) {
    if (!sound_loader || !path_service) return;
    if (blackboard) apply_sound_settings(*sound_loader, *blackboard);
    sound_loader->play_se(path_service->resolve(rel_path));
}

static int dino_control_preset(const NeneBlackboard& blackboard) {
    return (blackboard.getf(kSettingDinoControl, 0.0f) > 0.5f) ? 1 : 0;
}

static const char* dino_confirm_key_name(const NeneBlackboard& blackboard) {
    return dino_control_preset(blackboard) == 0 ? "Space" : "Enter";
}

static std::vector<NeneInputBinding> dino_control_bindings(int preset) {
    if (preset == 1) {
        return {
            NeneInputBinding(NeneInputDevice::Keyboard, NeneInputControl::Button,
                             static_cast<int>(SDLK_D), "move_right"),
            NeneInputBinding(NeneInputDevice::Keyboard, NeneInputControl::Button,
                             static_cast<int>(SDLK_A), "move_left"),
            NeneInputBinding(NeneInputDevice::Keyboard, NeneInputControl::Button,
                             static_cast<int>(SDLK_LSHIFT), "dash"),
            NeneInputBinding(NeneInputDevice::Keyboard, NeneInputControl::Button,
                             static_cast<int>(SDLK_RSHIFT), "dash"),
            NeneInputBinding(NeneInputDevice::Keyboard, NeneInputControl::Button,
                             static_cast<int>(SDLK_W), "jump"),
            NeneInputBinding(NeneInputDevice::Keyboard, NeneInputControl::Button,
                             static_cast<int>(SDLK_RETURN), "jump"),
        };
    }
    return {
        NeneInputBinding(NeneInputDevice::Keyboard, NeneInputControl::Button,
                         static_cast<int>(SDLK_RIGHT), "move_right"),
        NeneInputBinding(NeneInputDevice::Keyboard, NeneInputControl::Button,
                         static_cast<int>(SDLK_LEFT), "move_left"),
        NeneInputBinding(NeneInputDevice::Keyboard, NeneInputControl::Button,
                         static_cast<int>(SDLK_LSHIFT), "dash"),
        NeneInputBinding(NeneInputDevice::Keyboard, NeneInputControl::Button,
                         static_cast<int>(SDLK_RSHIFT), "dash"),
        NeneInputBinding(NeneInputDevice::Keyboard, NeneInputControl::Button,
                         static_cast<int>(SDLK_SPACE), "jump"),
        NeneInputBinding(NeneInputDevice::Keyboard, NeneInputControl::Button,
                         static_cast<int>(SDLK_UP), "jump"),
    };
}

static void apply_dino_control_setting(NeneBlackboard& blackboard) {
    blackboard.input_maps["play"] = dino_control_bindings(dino_control_preset(blackboard));
}

static void apply_cursor_setting(const NeneBlackboard& blackboard) {
    if (blackboard.getf(kSettingCursorVisible, 1.0f) > 0.5f) {
        SDL_ShowCursor();
    } else {
        SDL_HideCursor();
    }
}

static void ensure_game_settings(NeneBlackboard& blackboard) {
    if (blackboard.fps != 30 && blackboard.fps != 60) blackboard.fps = 60;
    blackboard.ensure_persistentf(kSettingSeVolume, 5.0f);
    blackboard.ensure_persistentf(kSettingBgmVolume, 5.0f);
    blackboard.ensure_persistentf(kSettingCursorVisible, 1.0f);
    blackboard.ensure_persistentf(kSettingDinoControl, 0.0f);
}

static void apply_game_settings(NeneBlackboard& blackboard) {
    ensure_game_settings(blackboard);
    apply_cursor_setting(blackboard);
    apply_dino_control_setting(blackboard);
}

static Uint8 alpha_byte(float alpha) {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    return static_cast<Uint8>(std::round(alpha * 255.0f));
}

static std::shared_ptr<SDL_Surface> make_surface_ptr(SDL_Surface* surface) {
    return std::shared_ptr<SDL_Surface>(surface, [](SDL_Surface* p) {
        if (p) SDL_DestroySurface(p);
    });
}

static std::mutex& ttf_preload_mutex() {
    static std::mutex mutex;
    return mutex;
}

static void add_texture_asset_task(NeneTaskGroup& tasks,
                                   const std::shared_ptr<NeneTaskServer>& task_server,
                                   const std::shared_ptr<NeneImageLoader>& asset_loader,
                                   std::string label,
                                   std::string path) {
    if (!asset_loader || path.empty()) return;
    if (!task_server) {
        asset_loader->get_texture(path);
        return;
    }

    auto surface = std::make_shared<std::shared_ptr<SDL_Surface>>();
    tasks.add(task_server->submit(
        std::string("texture decode: ") + label,
        [surface, path](NeneTaskContext& ctx) {
            ctx.throw_if_stop_requested();
            SDL_Surface* raw = IMG_Load(path.c_str());
            if (!raw) {
                throw std::runtime_error(std::string("IMG_Load failed: ") + SDL_GetError());
            }
            *surface = make_surface_ptr(raw);
            ctx.set_progress(1.0f);
        },
        [asset_loader, path, surface] {
            asset_loader->get_texture_from_surface(path, surface->get());
            surface->reset();
        }));
}

static void add_text_asset_task(NeneTaskGroup& tasks,
                                const std::shared_ptr<NeneTaskServer>& task_server,
                                const std::shared_ptr<NeneFontLoader>& font_loader,
                                std::string font_path,
                                std::string text,
                                int size,
                                SDL_Color color) {
    if (!font_loader || font_path.empty() || text.empty()) return;
    if (!task_server) {
        font_loader->get_text_texture(font_path, size, text, color);
        return;
    }

    auto surface = std::make_shared<std::shared_ptr<SDL_Surface>>();
    tasks.add(task_server->submit(
        std::string("text surface: ") + text,
        [surface, font_path, text, size, color](NeneTaskContext& ctx) {
            ctx.throw_if_stop_requested();
            std::lock_guard<std::mutex> lock(ttf_preload_mutex());
            TTF_Font* font = TTF_OpenFont(font_path.c_str(), size);
            if (!font) {
                throw std::runtime_error(std::string("TTF_OpenFont failed: ") + SDL_GetError());
            }
            SDL_Surface* raw = TTF_RenderText_Blended(font, text.c_str(), 0, color);
            TTF_CloseFont(font);
            if (!raw) {
                throw std::runtime_error(std::string("TTF_RenderText_Blended failed: ") + SDL_GetError());
            }
            *surface = make_surface_ptr(raw);
            ctx.set_progress(1.0f);
        },
        [font_loader, font_path, text, size, color, surface] {
            font_loader->get_text_texture_from_surface(font_path, size, text, color, surface->get());
            surface->reset();
        }));
}

static void add_audio_asset_task(NeneTaskGroup& tasks,
                                 const std::shared_ptr<NeneTaskServer>& task_server,
                                 const std::shared_ptr<NeneSoundLoader>& sound_loader,
                                 std::string path) {
    if (!sound_loader || path.empty()) return;
    if (!task_server) {
        sound_loader->preload(path);
        return;
    }
    tasks.add(task_server->submit(
        std::string("audio: ") + path,
        [sound_loader, path = std::move(path)](NeneTaskContext& ctx) {
            ctx.throw_if_stop_requested();
            sound_loader->preload(path);
            ctx.set_progress(1.0f);
        }));
}

static void submit_super_chrome_dino_asset_tasks(
    NeneTaskGroup& tasks,
    const std::shared_ptr<NeneTaskServer>& task_server,
    const std::shared_ptr<NeneImageLoader>& asset_loader,
    const std::shared_ptr<NeneFontLoader>& font_loader,
    const std::shared_ptr<NeneSoundLoader>& sound_loader,
    const std::shared_ptr<PathService>& path_service) {
    if (!asset_loader || !font_loader || !path_service) {
        throw std::runtime_error("submit_super_chrome_dino_asset_tasks: services not ready");
    }

    const std::vector<std::string> texture_paths = {
        kSpriteSheetPath,
        "assets/ui/GoogleFontsIcons/lock_40dp_FFFFFF_FILL0_wght400_GRAD0_opsz40.png",
        "assets/ui/GoogleFontsIcons/check_40dp_FFFFFF_FILL0_wght400_GRAD0_opsz40.png",
        "assets/ui/kenney_input-prompts_1.5/Keyboard & Mouse/Default/keyboard_arrow_up.png",
        "assets/ui/kenney_input-prompts_1.5/Keyboard & Mouse/Default/keyboard_arrow_down.png",
        "assets/ui/kenney_input-prompts_1.5/Keyboard & Mouse/Default/keyboard_arrow_left.png",
        "assets/ui/kenney_input-prompts_1.5/Keyboard & Mouse/Default/keyboard_arrow_right.png",
        "assets/ui/kenney_input-prompts_1.5/Keyboard & Mouse/Default/keyboard_space_icon.png",
        "assets/ui/kenney_input-prompts_1.5/Keyboard & Mouse/Default/keyboard_escape.png",
    };

    for (const auto& rel_path : texture_paths) {
        const std::string path = path_service->resolve(rel_path);
        add_texture_asset_task(
            tasks,
            task_server,
            asset_loader,
            rel_path,
            path);
    }

    const std::vector<std::string> audio_paths = {
        kSeCursorMovePath,
        kSeConfirmPath,
        kSeDinoJumpPath,
        kStageBgmPaths[0],
        kStageBgmPaths[1],
        kStageBgmPaths[2],
    };
    for (const auto& rel_path : audio_paths) {
        add_audio_asset_task(tasks, task_server, sound_loader, path_service->resolve(rel_path));
    }

    const std::string font_path = path_service->resolve(kGameFontPath);
    auto add_text = [&](std::string text, int size, SDL_Color color) {
        add_text_asset_task(
            tasks,
            task_server,
            font_loader,
            font_path,
            std::move(text),
            size,
            color);
    };
    auto add_text_pair = [&](const std::string& text, int size) {
        add_text(text, size, SDL_Color{255, 255, 255, 255});
        add_text(text, size, SDL_Color{160, 160, 160, 255});
    };

    add_text("SuperChromeDino", 56, SDL_Color{255, 255, 255, 255});
    add_text("Continue", 32, SDL_Color{255, 255, 255, 255});
    add_text("Continue", 32, SDL_Color{150, 150, 150, 255});
    add_text("New Game", 32, SDL_Color{255, 255, 255, 255});
    add_text("Settings", 32, SDL_Color{255, 255, 255, 255});
    add_text("Quit Game", 32, SDL_Color{255, 255, 255, 255});
    add_text("Select", 24, SDL_Color{255, 255, 255, 255});
    add_text("Confirm", 24, SDL_Color{255, 255, 255, 255});

    add_text("Game Over", 64, SDL_Color{255, 255, 255, 255});
    add_text("Stage Clear", 64, SDL_Color{255, 255, 255, 255});
    add_text("Paused", 64, SDL_Color{255, 255, 255, 255});
    add_text("Press Space to Restart", 24, SDL_Color{255, 255, 255, 255});
    add_text("Press Space to Stage Select", 24, SDL_Color{255, 255, 255, 255});
    add_text("Press Enter to Restart", 24, SDL_Color{255, 255, 255, 255});
    add_text("Press Enter to Stage Select", 24, SDL_Color{255, 255, 255, 255});
    add_text("Press P to Resume", 24, SDL_Color{255, 255, 255, 255});
    add_text("00000", 28, SDL_Color{255, 255, 255, 255});

    add_text("Settings", 40, SDL_Color{255, 255, 255, 255});
    add_text("Select Option", 24, SDL_Color{255, 255, 255, 255});
    add_text("Change Option", 24, SDL_Color{255, 255, 255, 255});
    add_text("Return to Title", 24, SDL_Color{255, 255, 255, 255});
    add_text("Saved!", 24, SDL_Color{255, 255, 255, 255});
    for (const auto& text : {
        "FPS", "SE Volume", "BGM Volume", "Cursor in Window", "Dino Control",
        "60", "30", "Show", "Hide", "Arrows", "+ Space", "WASD", "+ Enter",
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    }) {
        add_text_pair(text, 28);
    }

    add_text("Stage Select", 40, SDL_Color{255, 255, 255, 255});
    add_text("1", 44, SDL_Color{255, 255, 255, 255});
    add_text("2", 44, SDL_Color{255, 255, 255, 255});
    add_text("3", 44, SDL_Color{255, 255, 255, 255});
}

// 恐竜
class Dino final : public NeneNode {
public:
    explicit Dino(std::string name) : NeneNode(std::move(name)) {}
    ~Dino() override { remove_colliders_(); }
    SDL_FRect hitbox() const { return SDL_FRect{ x_, y_, w_, h_ }; }
    void set_dead(bool v) { dead_ = v; }
    bool is_dead() const { return dead_; }
protected:
    void init_node() override {
        // 念のため必要なサービスが注入されているか確認する
        if (!asset_loader || !path_service || !blackboard) nnthrow("services not ready (asset_loader/path_service/blackboard)");
        if (!collision_world) nnthrow("services not ready (collision_world)");
        // テクスチャ
        sprite_tex_ = asset_loader->get_texture(path_service->resolve("assets/sprites/sprite.png"));
        if (!sprite_tex_) nnthrow("failed to load dino sprite texture");
        w_ = 88.0f;
        h_ = 96.0f;
        // 初期位置
        x_ = (static_cast<float>(blackboard->window_w) - w_) * 0.5f;
        y_ = blackboard->ground_y - h_;
        // 走りアニメ
        run_src_[0] = SDL_FRect{ 1514.2f, 0.0f, 88.0f, 96.0f };
        run_src_[1] = SDL_FRect{ 1602.2f, 0.0f, 88.0f, 96.0f };
        jump_src_ = run_src_[0];
        // 状態
        on_ground_ = true;
        vy_ = 0.0f;
        vx_ = 0.0f;
        anim_accum_ = 0.0f;
        anim_idx_ = 0;
        facing_dir_ = 1;
        dead_ = false;
        winning_run_ = is_winning_run_stage(blackboard.get());
        star_accum_ = 0.0f;
        register_colliders_();
    }
    // 内部入力
    void handle_nene_input(const NeneInput& input) override {
        if (dead_) return;
        if (input.action == "jump" && input.phase == NeneInputPhase::Pressed) {
            try_jump_();
        }
    }
    // タイムラプス
    void handle_time_lapse(const float& dt) override {
        if (!blackboard) return;
        if (dead_) return;
        vx_ = blackboard->getf("world_scroll_speed", 0.0f);
        const bool moving = (std::fabs(vx_) > 0.001f);
        if (moving) facing_dir_ = (vx_ > 0.0f) ? 1 : -1;
        x_ = (static_cast<float>(blackboard->window_w) - w_) * 0.5f;
        // 地上で移動入力があるときだけ足踏みアニメーションを再生する
        if (on_ground_ && moving) {
            anim_accum_ += dt;
            if (anim_accum_ >= run_frame_sec_) {
                anim_accum_ = 0.0f;
                anim_idx_ = (anim_idx_ + 1) % 2;
            }
        } else if (on_ground_) {
            anim_accum_ = 0.0f;
            anim_idx_ = 0;
        }
        if (winning_run_) star_accum_ += dt;
        // ジャンプ中なら物理演算に従って座標を更新し続ける
        vy_ += blackboard->gravity * dt;
        y_  += vy_ * dt;
        const float ground_y = blackboard->ground_y - h_;
        if (y_ >= ground_y) {
            y_ = ground_y;
            vy_ = 0.0f;
            on_ground_ = true;
        } else {
            on_ground_ = false;
        }
        sync_colliders_();
    }
    // レンダリング
    void render(SDL_Renderer* r) override {
        if (!r) return;
        if (!sprite_tex_) return;
        const SDL_FRect* src = (!on_ground_) ? &jump_src_ : &run_src_[anim_idx_];
        SDL_FRect dst { x_, y_, w_, h_ };
        if (winning_run_) apply_star_tint_();
        if (facing_dir_ < 0) {
            SDL_RenderTextureRotated(r, sprite_tex_, src, &dst, 0.0, nullptr, SDL_FLIP_HORIZONTAL);
        } else {
            SDL_RenderTexture(r, sprite_tex_, src, &dst);
        }
        if (winning_run_) SDL_SetTextureColorMod(sprite_tex_, 255, 255, 255);
        // コライダー可視化
        if (blackboard && blackboard->getf("show_hitbox", 0.0f) > 0.5f) {
            if (collision_world) {
                for (const auto id : collider_ids_) {
                    if (auto* c = collision_world->find(id)) {
                        c->debug_render_filled(r);
                    }
                }
            }
        }
    }
private:
    using Vertices = std::vector<SDL_FPoint>;

    void try_jump_() {
        if (winning_run_) return;
        if (!on_ground_) return;
        on_ground_ = false;
        vy_ = -jump_speed_;
        play_sound_effect(sound_loader, path_service, blackboard, kSeDinoJumpPath);
    }
    void apply_star_tint_() {
        const float phase = star_accum_ * 8.0f;
        const auto channel = [](float p) -> Uint8 {
            return static_cast<Uint8>(std::round(205.0f + 50.0f * (0.5f + 0.5f * std::sin(p))));
        };
        SDL_SetTextureColorMod(
            sprite_tex_,
            channel(phase),
            channel(phase + 2.1f),
            channel(phase + 4.2f));
    }
    static Vertices rect_(float x, float y, float rw, float rh) {
        return Vertices{
            SDL_FPoint{ x,      y      },
            SDL_FPoint{ x + rw, y      },
            SDL_FPoint{ x + rw, y + rh },
            SDL_FPoint{ x,      y + rh },
        };
    }
    Vertices maybe_flip_(Vertices v) const {
        if (facing_dir_ >= 0) return v;
        for (auto& p : v) p.x = w_ - p.x;
        std::reverse(v.begin(), v.end());
        return v;
    }
    std::vector<Vertices> collider_vertices_(int frame) const {
        std::vector<Vertices> parts;
        parts.push_back(maybe_flip_(Vertices{
            SDL_FPoint{  0.0f, 38.0f },
            SDL_FPoint{ 12.0f, 34.0f },
            SDL_FPoint{ 42.0f, 48.0f },
            SDL_FPoint{ 42.0f, 58.0f },
            SDL_FPoint{ 14.0f, 56.0f },
            SDL_FPoint{  0.0f, 50.0f },
        }));
        parts.push_back(maybe_flip_(Vertices{
            SDL_FPoint{ 12.0f, 46.0f },
            SDL_FPoint{ 52.0f, 40.0f },
            SDL_FPoint{ 70.0f, 56.0f },
            SDL_FPoint{ 58.0f, 76.0f },
            SDL_FPoint{ 22.0f, 78.0f },
            SDL_FPoint{  8.0f, 58.0f },
        }));
        parts.push_back(maybe_flip_(Vertices{
            SDL_FPoint{ 34.0f, 30.0f },
            SDL_FPoint{ 58.0f, 30.0f },
            SDL_FPoint{ 70.0f, 48.0f },
            SDL_FPoint{ 56.0f, 60.0f },
            SDL_FPoint{ 30.0f, 50.0f },
        }));
        parts.push_back(maybe_flip_(rect_(40.0f, 4.0f, 44.0f, 28.0f)));
        parts.push_back(maybe_flip_(rect_(56.0f, 28.0f, 24.0f, 18.0f)));

        if (frame == 0) {
            parts.push_back(maybe_flip_(rect_(20.0f, 76.0f, 16.0f, 20.0f)));
            parts.push_back(maybe_flip_(rect_(40.0f, 76.0f, 18.0f, 8.0f)));
        } else {
            parts.push_back(maybe_flip_(rect_(24.0f, 80.0f, 14.0f, 8.0f)));
            parts.push_back(maybe_flip_(rect_(40.0f, 80.0f, 16.0f, 16.0f)));
        }
        return parts;
    }
    int collider_frame_() const {
        return on_ground_ ? anim_idx_ : 0;
    }
    void register_colliders_() {
        if (!collision_world) return;
        remove_colliders_();
        const int frame = collider_frame_();
        const auto parts = collider_vertices_(frame);
        collider_ids_.reserve(parts.size());
        for (const auto& vertices : parts) {
            NeneColorPolygon poly;
            poly.owner_name = this->name;
            poly.vertices = vertices;
            poly.position = SDL_FPoint{ x_, y_ };
            poly.color = NenePolygonColor::Blue;
            poly.layer = kLayerPlayer;
            poly.mask  = kMaskPlayerHits;
            poly.enabled = true;
            poly.debug_draw = true;
            poly.debug_alpha = 0.25f;
            collider_ids_.push_back(collision_world->add_collider(std::move(poly)));
        }
        collider_frame_cache_ = frame;
        collider_facing_cache_ = facing_dir_;
    }
    void sync_colliders_() {
        if (!collision_world) return;
        const int frame = collider_frame_();
        if (frame != collider_frame_cache_ || facing_dir_ != collider_facing_cache_) {
            const auto parts = collider_vertices_(frame);
            const std::size_t n = std::min(collider_ids_.size(), parts.size());
            for (std::size_t i = 0; i < n; ++i) {
                if (auto* c = collision_world->find(collider_ids_[i])) {
                    c->vertices = parts[i];
                }
            }
            collider_frame_cache_ = frame;
            collider_facing_cache_ = facing_dir_;
        }
        for (const auto id : collider_ids_) {
            collision_world->set_position(id, SDL_FPoint{ x_, y_ });
        }
    }
    void remove_colliders_() {
        if (collision_world) {
            for (const auto id : collider_ids_) {
                collision_world->remove_collider(id);
            }
        }
        collider_ids_.clear();
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
    float vx_ = 0.0f;
    float vy_ = 0.0f;
    bool  on_ground_ = true;
    bool  dead_ = false;
    bool  winning_run_ = false;
    int   facing_dir_ = 1;
    // アニメーション設定
    float anim_accum_ = 0.0f;
    float star_accum_ = 0.0f;
    int   anim_idx_ = 0;
    // 運動設定
    float jump_speed_    = 900.0f;
    float run_frame_sec_ = 0.10f;
    // CollisionWorld登録ID
    std::vector<NeneCollisionWorld::ColliderId> collider_ids_;
    int collider_frame_cache_ = -1;
    int collider_facing_cache_ = 1;
};

// 地面
class Ground final : public NeneNode {
public:
    explicit Ground(std::string name) : NeneNode(std::move(name)) {}
protected:
    void init_node() override {
        if (!asset_loader || !path_service || !blackboard) {
            nnthrow("services not ready (asset_loader/path_service/blackboard)");
        }
        sprite_tex_ = asset_loader->get_texture(path_service->resolve("assets/sprites/sprite.png"));
        if (!sprite_tex_) nnthrow("failed to load ground sprite texture");
        float tw = 0.0f, th = 0.0f;
        if (!SDL_GetTextureSize(sprite_tex_, &tw, &th)) {
            nnthrow("SDL_GetTextureSize failed");
        }
        constexpr float kSrcH = 28.0f;
        src_ = SDL_FRect{ 0.0f, th - kSrcH, tw, kSrcH };
        scroll_ = 0.0f;
        // 背景扱いで奥へ
        set_render_z(-100);
    }
    void handle_time_lapse(const float& dt) override {
        if (!blackboard) return;
        const float speed = blackboard->getf("world_scroll_speed", 0.0f);
        scroll_ += speed * dt;
        // wrap（src_.w が 0 になることは無い想定）
        if (src_.w > 1.0f) {
            scroll_ = std::fmod(scroll_, src_.w);
            if (scroll_ < 0.0f) scroll_ += src_.w;
        }
    }
    void render(SDL_Renderer* r) override {
        if (!r || !blackboard || !sprite_tex_) return;
        const float ww = static_cast<float>(blackboard->window_w);
        const float y  = blackboard->ground_y - 10;
        const float src_x = scroll_;
        const float src_y = src_.y;
        const float src_h = src_.h;
        const float src_w = src_.w;
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
class Cactus final : public NeneNode {
public:
    struct Variant {
        SDL_FRect src;
        float w;
        float h;
    };
    Cactus(std::string name, Variant v)
        : NeneNode(std::move(name)), variant_(v) {}
    // ノードが消えるときにコライダーも消す
    ~Cactus() override {
        if (collision_world && collider_id_ != 0) {
            collision_world->remove_collider(collider_id_);
            collider_id_ = 0;
        }
    }

protected:
    void init_node() override {
        if (!asset_loader || !path_service || !blackboard) nnthrow("services not ready (asset_loader/path_service/blackboard)");
        if (!collision_world) nnthrow("services not ready (collision_world)");
        sprite_tex_ = asset_loader->get_texture(path_service->resolve("assets/sprites/sprite.png"));
        if (!sprite_tex_) nnthrow("failed to load sprite texture");
        // 種類ごとのサイズ・画像
        w_ = variant_.w;
        h_ = variant_.h;
        src_ = variant_.src;
        // 画面右外から出現
        x_ = blackboard->window_w + spawn_margin_;
        y_ = blackboard->ground_y - h_;
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
        // コライダー可視化設定
        poly.debug_draw = true;
        poly.debug_alpha = 0.25f;
        // コライダー登録
        collider_id_ = collision_world->add_collider(std::move(poly));
        winning_run_ = is_winning_run_stage(blackboard.get());
        knocked_down_ = false;
        knock_angle_ = 0.0f;
    }
    void handle_time_lapse(const float& dt) override {
        if (!blackboard) return;
        const float speed = blackboard->getf("world_scroll_speed", 0.0f);
        x_ -= speed * dt;
        if (knocked_down_) {
            knock_angle_ = std::min(90.0f, knock_angle_ + knock_rotation_speed_ * dt);
            x_ -= knock_slide_speed_ * dt;
        }
        if (collision_world && collider_id_ != 0) {
            collision_world->set_position(collider_id_, SDL_FPoint{ x_, y_ });
        }
        if (winning_run_ && !knocked_down_) {
            try_knock_down_on_dino_collision_();
        }
        if (x_ + w_ < -despawn_margin_
            || x_ > static_cast<float>(blackboard->window_w) + spawn_margin_ + despawn_margin_) {
            send_mail(NeneMail("cactus_factory", this->name, "despawn", this->name));
        }
    }
    void render(SDL_Renderer* r) override {
        if (!r || !sprite_tex_) return;
        SDL_FRect dst{ x_, y_, w_, h_ };
        if (knocked_down_) {
            SDL_FPoint pivot{ w_ * 0.5f, h_ };
            SDL_RenderTextureRotated(r, sprite_tex_, &src_, &dst, knock_angle_, &pivot, SDL_FLIP_NONE);
        } else {
            SDL_RenderTexture(r, sprite_tex_, &src_, &dst);
        }
        // コライダー可視化
        if (blackboard && blackboard->getf("show_hitbox", 0.0f) > 0.5f) {
            if (collision_world && collider_id_ != 0) {
                if (auto* c = collision_world->find(collider_id_)) {
                    c->debug_render_filled(r);
                }
            }
        }
    }
private:
    void try_knock_down_on_dino_collision_() {
        if (!collision_world || collider_id_ == 0) return;
        auto* self = collision_world->find(collider_id_);
        if (!self || !self->enabled) return;
        auto hit = collision_world->detect_collision(*self);
        if (!hit) return;
        if (hit->get().owner_name != "dino") return;
        knock_down_();
    }
    void knock_down_() {
        knocked_down_ = true;
        knock_angle_ = 0.0f;
        x_ -= 10.0f;
        if (collision_world && collider_id_ != 0) {
            collision_world->set_enabled(collider_id_, false);
        }
    }
    static constexpr std::uint32_t kLayerPlayer   = 1u << 0;
    static constexpr std::uint32_t kLayerObstacle = 1u << 1;
    static constexpr std::uint32_t kMaskObstacleHits = kLayerPlayer;
    Variant variant_;
    SDL_Texture* sprite_tex_ = nullptr;
    SDL_FRect src_{};
    float x_ = 0.0f, y_ = 0.0f, w_ = 0.0f, h_ = 0.0f;
    float spawn_margin_ = 40.0f;
    float despawn_margin_ = 60.0f;
    bool winning_run_ = false;
    bool knocked_down_ = false;
    float knock_angle_ = 0.0f;
    float knock_slide_speed_ = 140.0f;
    float knock_rotation_speed_ = 360.0f;
    NeneCollisionWorld::ColliderId collider_id_ = 0;
};

// サボテン工場
class CactusFactory final : public NeneFactory {
public:
    explicit CactusFactory(std::string name)
        : NeneFactory(std::move(name)) {}
protected:
    void init_node() override {
        const float small_base_x = 446.0f;
        const float small_base_y = 0.0f;
        const float small_w = 34.0f;
        const float small_h = 70.0f;
        cactus_variants_ = {
            // 小
            { SDL_FRect{ small_base_x+small_w*0, small_base_y, small_w, small_h }, small_w, small_h },
            { SDL_FRect{ small_base_x+small_w*1, small_base_y, small_w, small_h }, small_w, small_h },
            { SDL_FRect{ small_base_x+small_w*2, small_base_y, small_w, small_h }, small_w, small_h },
            { SDL_FRect{ small_base_x+small_w*3, small_base_y, small_w, small_h }, small_w, small_h },
            { SDL_FRect{ small_base_x+small_w*4, small_base_y, small_w, small_h }, small_w, small_h },
            { SDL_FRect{ small_base_x+small_w*5, small_base_y, small_w, small_h }, small_w, small_h },
        };
        // NeneFactory に型登録：spawnメールで cactus を生成する
        register_type("cactus",
            [this](std::string instance_name, std::string_view /*arg*/) -> std::unique_ptr<NeneNode> {
                if (cactus_variants_.empty()) nnthrow("cactus_variants_ is empty");

                std::uniform_int_distribution<int> dist(0, (int)cactus_variants_.size() - 1);
                const auto v = cactus_variants_[dist(rng_)];
                return std::make_unique<Cactus>(std::move(instance_name), v); // Cactus(name, Variant) :contentReference[oaicite:4]{index=4}
            }
        );
        spawn_accum_ = 0.0f;
        obstacle_seq_ = 0;
        winning_run_ = is_winning_run_stage(blackboard.get());
        // 最初の出現までの時間（ステージ3は短め）
        next_spawn_in_ = winning_run_ ? frand_(0.25f, 0.65f) : frand_(0.8f, 1.6f);
    }
    void handle_time_lapse(const float& dt) override {
        const float speed = blackboard ? blackboard->getf("world_scroll_speed", 0.0f) : 0.0f;
        if (speed <= 1.0f) {
            spawn_accum_ = 0.0f;
            return;
        }
        spawn_accum_ += dt;
        if (spawn_accum_ >= next_spawn_in_) {
            spawn_accum_ = 0.0f;
            spawn_obstacle_();
            // 次の間隔をランダムに（ステージ3は短め）
            next_spawn_in_ = winning_run_ ? frand_(0.35f, 0.85f) : frand_(0.7f, 1.7f);
        }
    }
    void handle_nene_mail(const NeneMail& mail) override {
        // 障害物を消去
        if (mail.subject == "despawn") remove_child(mail.body);
    }
private:
    std::vector<Cactus::Variant> cactus_variants_;
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
    float spawn_accum_ = 0.0f;
    float spawn_interval_ = 1.35f;
    int obstacle_seq_ = 0;
    float next_spawn_in_ = 1.0f;
    bool winning_run_ = false;
};

// プテラノドン
class Pteranodon final : public NeneNode {
public:
    struct Variant {
        float ground_clearance;
    };
    Pteranodon(std::string name, Variant v)
        : NeneNode(std::move(name)), variant_(v) {}
    ~Pteranodon() override {
        if (collision_world && collider_id_ != 0) {
            collision_world->remove_collider(collider_id_);
            collider_id_ = 0;
        }
    }

protected:
    void init_node() override {
        if (!asset_loader || !path_service || !blackboard) nnthrow("services not ready (asset_loader/path_service/blackboard)");
        if (!collision_world) nnthrow("services not ready (collision_world)");
        sprite_tex_ = asset_loader->get_texture(path_service->resolve("assets/sprites/sprite.png"));
        if (!sprite_tex_) nnthrow("failed to load sprite texture");

        src_[0] = SDL_FRect{ 260.0f, 0.0f, 92.0f, 80.0f };
        src_[1] = SDL_FRect{ 352.0f, 0.0f, 92.0f, 80.0f };
        w_ = 92.0f;
        h_ = 80.0f;
        x_ = static_cast<float>(blackboard->window_w) + spawn_margin_;
        y_ = blackboard->ground_y - variant_.ground_clearance - h_;

        NeneColorPolygon poly;
        poly.owner_name = this->name;
        poly.vertices = {
            SDL_FPoint{ 8.0f, 10.0f },
            SDL_FPoint{ w_ - 8.0f, 10.0f },
            SDL_FPoint{ w_ - 8.0f, h_ - 8.0f },
            SDL_FPoint{ 8.0f, h_ - 8.0f },
        };
        poly.position = SDL_FPoint{ x_, y_ };
        poly.color = NenePolygonColor::Red;
        poly.layer = kLayerObstacle;
        poly.mask  = kMaskObstacleHits;
        poly.enabled = true;
        poly.debug_draw = true;
        poly.debug_alpha = 0.25f;
        collider_id_ = collision_world->add_collider(std::move(poly));
    }
    void handle_time_lapse(const float& dt) override {
        if (!blackboard) return;
        anim_accum_ += dt;
        if (anim_accum_ >= anim_frame_sec_) {
            anim_accum_ = 0.0f;
            anim_idx_ = (anim_idx_ + 1) % 2;
        }

        x_ -= blackboard->scroll_speed * flight_speed_multiplier_ * dt;
        if (collision_world && collider_id_ != 0) {
            collision_world->set_position(collider_id_, SDL_FPoint{ x_, y_ });
        }
        if (x_ + w_ < -despawn_margin_) {
            send_mail(NeneMail("pteranodon_factory", this->name, "despawn", this->name));
        }
    }
    void render(SDL_Renderer* r) override {
        if (!r || !sprite_tex_) return;
        SDL_FRect dst{ x_, y_, w_, h_ };
        SDL_RenderTexture(r, sprite_tex_, &src_[anim_idx_], &dst);
        if (blackboard && blackboard->getf("show_hitbox", 0.0f) > 0.5f) {
            if (collision_world && collider_id_ != 0) {
                if (auto* c = collision_world->find(collider_id_)) {
                    c->debug_render_filled(r);
                }
            }
        }
    }

private:
    static constexpr std::uint32_t kLayerPlayer   = 1u << 0;
    static constexpr std::uint32_t kLayerObstacle = 1u << 1;
    static constexpr std::uint32_t kMaskObstacleHits = kLayerPlayer;
    Variant variant_;
    SDL_Texture* sprite_tex_ = nullptr;
    SDL_FRect src_[2]{};
    float x_ = 0.0f, y_ = 0.0f, w_ = 0.0f, h_ = 0.0f;
    float spawn_margin_ = 80.0f;
    float despawn_margin_ = 80.0f;
    float anim_accum_ = 0.0f;
    int anim_idx_ = 0;
    float anim_frame_sec_ = 0.16f;
    float flight_speed_multiplier_ = 1.2f;
    NeneCollisionWorld::ColliderId collider_id_ = 0;
};

// プテラノドン工場
class PteranodonFactory final : public NeneFactory {
public:
    explicit PteranodonFactory(std::string name)
        : NeneFactory(std::move(name)) {}
protected:
    void init_node() override {
        pteranodon_variants_ = {
            // 低空: 地上の恐竜に当たるのでジャンプで回避する
            { 12.0f },
            // 高空: 地上では下を通れるが、ジャンプすると当たりやすい
            { 110.0f },
        };
        spawn_accum_ = 0.0f;
        obstacle_seq_ = 0;
        next_spawn_in_ = frand_(1.3f, 2.3f);
    }
    void handle_time_lapse(const float& dt) override {
        spawn_accum_ += dt;
        if (spawn_accum_ >= next_spawn_in_) {
            spawn_accum_ = 0.0f;
            spawn_obstacle_();
            next_spawn_in_ = frand_(1.8f, 3.2f);
        }
    }
    void handle_nene_mail(const NeneMail& mail) override {
        if (mail.subject == "despawn") remove_child(mail.body);
    }
private:
    void spawn_obstacle_() {
        if (pteranodon_variants_.empty()) nnthrow("pteranodon_variants_ is empty");
        std::uniform_int_distribution<int> dist(0, static_cast<int>(pteranodon_variants_.size()) - 1);
        const auto v = pteranodon_variants_[dist(rng_)];
        const std::string name = "pteranodon_" + std::to_string(obstacle_seq_++);
        add_child(std::make_unique<Pteranodon>(name, v));
    }
    static float frand_(float a, float b) {
        const float t = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
        return a + (b - a) * t;
    }
    std::mt19937 rng_{ std::random_device{}() };
    std::vector<Pteranodon::Variant> pteranodon_variants_;
    float spawn_accum_ = 0.0f;
    int obstacle_seq_ = 0;
    float next_spawn_in_ = 1.0f;
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
        if (blackboard && blackboard->getf("stage_clear", 0.0f) > 0.5f) return;
        std::unordered_set<Id> now;
        for (const auto& target : collision_world->colliders()) {
            if (!target.enabled) continue;
            if (target.owner_name != target_name_) continue;
            if (auto hit = collision_world->detect_collision(target)) {
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
        }
        prev_.swap(now);
    }
private:
    using Id = NeneCollisionWorld::ColliderId;
    std::unordered_set<Id> prev_;
    std::string target_name_ = "dino";
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
        add_child(std::make_unique<CactusFactory>("cactus_factory"));
        const int stage = blackboard ? clamp_stage(static_cast<int>(blackboard->getf("selected_stage", 1.0f))) : 1;
        if (stage == 2) {
            add_child(std::make_unique<PteranodonFactory>("pteranodon_factory"));
        }
    }
    void handle_time_lapse(const float& dt) override {
        if (blackboard) {
            const bool move_left = input_server && input_server->is_down("move_left");
            const bool move_right = input_server && input_server->is_down("move_right");
            const bool dash = input_server && input_server->is_down("dash");
            const int move_dir = (move_right ? 1 : 0) - (move_left ? 1 : 0);
            const float dash_mul = dash ? 1.8f : 1.0f;
            float speed = static_cast<float>(move_dir) * blackboard->scroll_speed * dash_mul;
            if (is_winning_run_stage(blackboard.get())) {
                float run_mul = move_right ? 1.35f : 1.0f;
                if (move_left && !move_right) run_mul = 0.55f;
                speed = blackboard->scroll_speed * run_mul * dash_mul;
            }
            blackboard->setf("world_scroll_speed", speed);
            float& score = blackboard->ensuref("score", 0.0f);
            if (speed > 0.0f) score += dt * 100.0f; // 右へ進んだ時間を得点化
            if (score >= kStageClearScore && blackboard->getf("stage_clear", 0.0f) < 0.5f) {
                score = kStageClearScore;
                blackboard->setf("stage_clear", 1.0f);
                blackboard->setf("world_scroll_speed", 0.0f);
                this->valve_time_lapse = false;
                this->valve_sdl_event = false;
                send_mail(NeneMail(this->name, "stage_cleared", ""));
            }
        }
    }
    void handle_nene_mail(const NeneMail& mail) override {
        // Referee のブロードキャストを受けた時
        if (mail.subject == "collision_detected") {
            if (blackboard && blackboard->getf("stage_clear", 0.0f) > 0.5f) return;
            if (is_winning_run_stage(blackboard.get())) return;
            // World 以下の time_lapse, sdl_event パルスを遮断
            this->valve_time_lapse = false;
            this->valve_sdl_event = false;
            // ゲームオーバーに移行
            if (blackboard) blackboard->setf("game_over", 1.0f);
            return;
        }
    }
private:
};


// オーバーレイ（スコア + Game Over）
class Overlay final : public NeneNode {
public:
    explicit Overlay(std::string name) : NeneNode(std::move(name)) {}
protected:
    void init_node() override {
        if (!font_loader || !path_service || !blackboard) nnthrow("services not ready (font_loader/path_service/blackboard)");
        font_path_ = path_service->resolve("assets/fonts/NotoSansJP-Regular.ttf");
        // 固定テキストは一度だけ作ればOK
        game_over_tex_ = font_loader->get_text_texture(font_path_, 64, "Game Over", SDL_Color{255,255,255,255});
        stage_clear_tex_ = font_loader->get_text_texture(font_path_, 64, "Stage Clear", SDL_Color{255,255,255,255});
        paused_tex_ = font_loader->get_text_texture(font_path_, 64, "Paused", SDL_Color{255,255,255,255});
        pause_prompt_tex_ = font_loader->get_text_texture(font_path_, 24, "Press P to Resume", SDL_Color{255,255,255,255});
        const std::string confirm_key = dino_confirm_key_name(*blackboard);
        restart_tex_ = font_loader->get_text_texture(font_path_, 24, "Press " + confirm_key + " to Restart", SDL_Color{255,255,255,255});
        stage_select_tex_ = font_loader->get_text_texture(font_path_, 24, "Press " + confirm_key + " to Stage Select", SDL_Color{255,255,255,255});
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
        if (!blackboard) return;
        // スコア表示更新（score が変化したときだけテクスチャ更新）
        const int score_i = static_cast<int>(blackboard->getf("score", 0.0f));
        if (score_i != last_score_int_) {
            last_score_int_ = score_i;
            update_score_texture_(score_i);
        }
        // 終了表示中だけ「Press...」を点滅
        const bool game_over = (blackboard->getf("game_over", 0.0f) > 0.5f);
        const bool stage_clear = (blackboard->getf("stage_clear", 0.0f) > 0.5f);
        if (game_over || stage_clear) {
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
        if (!r || !blackboard) return;
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
        const bool game_over = (blackboard->getf("game_over", 0.0f) > 0.5f);
        const bool stage_clear = (blackboard->getf("stage_clear", 0.0f) > 0.5f);
        const bool paused = (blackboard->getf(kFlagPaused, 0.0f) > 0.5f);
        if (paused && !game_over && !stage_clear) {
            render_pause_(r, static_cast<float>(w), static_cast<float>(h));
            return;
        }
        if (!game_over && !stage_clear) return;
        SDL_Texture* message_tex = game_over ? game_over_tex_ : stage_clear_tex_;
        SDL_Texture* prompt_tex = game_over ? restart_tex_ : stage_select_tex_;
        if (message_tex) {
            float gw = 0.0f, gh = 0.0f;
            SDL_GetTextureSize(message_tex, &gw, &gh);
            SDL_FRect go_dst{
                (static_cast<float>(w) - gw) * 0.5f,
                (static_cast<float>(h) - gh) * 0.5f - 40.0f,
                gw, gh
            };
            SDL_RenderTexture(r, message_tex, nullptr, &go_dst);
        }
        if (prompt_tex && press_visible_) {
            float rw = 0.0f, rh = 0.0f;
            SDL_GetTextureSize(prompt_tex, &rw, &rh);
            SDL_FRect rs_dst{
                (static_cast<float>(w) - rw) * 0.5f,
                (static_cast<float>(h) - rh) * 0.5f + 40.0f,
                rw, rh
            };
            SDL_RenderTexture(r, prompt_tex, nullptr, &rs_dst);
        }
    }
    void handle_nene_input(const NeneInput& input) override {
        if (!blackboard) return;
        const bool game_over = (blackboard->getf("game_over", 0.0f) > 0.5f);
        const bool stage_clear = (blackboard->getf("stage_clear", 0.0f) > 0.5f);
        if (!game_over && !stage_clear) return;
        if (input.action != "jump") return;
        if (input.phase != NeneInputPhase::Pressed) return;
        const char* next_scene = stage_clear
            ? "stage_select_scene"
            : stage_scene_name(static_cast<int>(blackboard->getf("selected_stage", 1.0f)));
        play_sound_effect(sound_loader, path_service, blackboard, kSeConfirmPath);
        send_mail(NeneMail("scene_switch", this->name, "switch_to", next_scene));
    }
private:
    void update_score_texture_(int score_i) {
        // スコアの表示(5桁)
        std::string s = std::to_string(score_i);
        if (s.size() < 5) s = std::string(5 - s.size(), '0') + s;
        score_tex_ = font_loader->get_text_texture(
            font_path_, 28, s, SDL_Color{255,255,255,255});
    }
    void render_pause_(SDL_Renderer* r, float w, float h) const {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 150);
        const SDL_FRect veil{ 0.0f, 0.0f, w, h };
        SDL_RenderFillRect(r, &veil);
        render_texture_centered_(r, paused_tex_, w * 0.5f, h * 0.5f - 34.0f);
        render_texture_centered_(r, pause_prompt_tex_, w * 0.5f, h * 0.5f + 36.0f);
    }
    static void render_texture_centered_(SDL_Renderer* r, SDL_Texture* tex, float cx, float cy) {
        if (!tex) return;
        float tw = 0.0f, th = 0.0f;
        SDL_GetTextureSize(tex, &tw, &th);
        const SDL_FRect dst{ cx - tw * 0.5f, cy - th * 0.5f, tw, th };
        SDL_RenderTexture(r, tex, nullptr, &dst);
    }
    std::string font_path_;
    SDL_Texture* score_tex_ = nullptr;
    SDL_Texture* game_over_tex_ = nullptr;
    SDL_Texture* restart_tex_ = nullptr;
    SDL_Texture* stage_clear_tex_ = nullptr;
    SDL_Texture* stage_select_tex_ = nullptr;
    SDL_Texture* paused_tex_ = nullptr;
    SDL_Texture* pause_prompt_tex_ = nullptr;
    int last_score_int_ = 0;
    float blink_accum_ = 0.0f;
    bool  press_visible_ = true;
};


// プレイシーン
class PlayScene final : public NeneNode {
public:
    explicit PlayScene(std::string name, int stage)
        : NeneNode(std::move(name)), stage_(clamp_stage(stage)) {}
    ~PlayScene() override {
        if (sound_loader) sound_loader->stop_bgm();
    }
protected:
    void init_node() override {
        if (blackboard) {
            blackboard->setf("selected_stage", static_cast<float>(stage_));
            blackboard->setf("score", 0.0f);
            blackboard->setf("game_over", 0.0f);
            blackboard->setf("stage_clear", 0.0f);
            blackboard->setf(kFlagPaused, 0.0f);
            blackboard->setf("world_scroll_speed", 0.0f);
            // コライダー可視化
            blackboard->setf("show_hitbox", 1.0f);
            blackboard->set_default_input_map("play", dino_control_bindings(0));
        }
        if (collision_world) collision_world->clear(); // リセットのためにコライダーをクリア
        else nnerr("no collision world");
        start_stage_bgm_();
        add_child(std::make_unique<NeneInputInterpreter>("input", "play"));
        add_child(std::make_unique<World>("world"));
        add_child(std::make_unique<Overlay>("overlay"));
    }
    void handle_sdl_event(const SDL_Event& ev) override {
        if (ev.type != SDL_EVENT_KEY_DOWN) return;
        if (ev.key.repeat) return;
        if (ev.key.key != SDLK_P) return;
        if (is_finished_()) return;
        set_paused_(!paused_);
    }
    void handle_nene_mail(const NeneMail& mail) override {
        if (mail.subject != "stage_cleared") return;
        if (stage_clear_saved_) return;
        stage_clear_saved_ = true;
        save_stage_clear_();
    }
private:
    void start_stage_bgm_() {
        if (!sound_loader || !path_service) return;
        if (blackboard) apply_sound_settings(*sound_loader, *blackboard);
        sound_loader->play_bgm(path_service->resolve(stage_bgm_path(stage_)));
    }
    bool is_finished_() const {
        if (!blackboard) return false;
        return blackboard->getf("game_over", 0.0f) > 0.5f
            || blackboard->getf("stage_clear", 0.0f) > 0.5f;
    }
    void set_paused_(bool paused) {
        paused_ = paused;
        if (blackboard) {
            blackboard->setf(kFlagPaused, paused ? 1.0f : 0.0f);
            if (paused) blackboard->setf("world_scroll_speed", 0.0f);
        }
        auto it = children.find("world");
        if (it == children.end() || !it->second) return;
        it->second->set_valve_sdl_event(!paused);
        it->second->set_valve_nene_input(!paused);
        it->second->set_valve_time_lapse(!paused);
    }
    void save_stage_clear_() {
        if (!blackboard) return;
        if (!save_service) {
            nnerr("save service is not configured");
            return;
        }
        const int stage = stage_;

        NeneSaveDocument doc;
        if (save_service->slot_exists(kStageProgressSlot)) {
            try {
                doc = save_service->load(kStageProgressSlot);
            } catch (const std::exception& e) {
                nnerr(std::string("failed to load stage progress; recreating: ") + e.what());
                doc.clear();
            }
        }
        try {
            auto& record = doc.node(kStageProgressNode);
            record.type = kStageProgressNode;
            NeneSaveWriter writer(record);
            writer.set_bool(stage_clear_key(stage), true);
            save_service->save(kStageProgressSlot, doc);
            const int next_stage = (stage < kStageCount) ? stage + 1 : stage;
            blackboard->setf("selected_stage", static_cast<float>(next_stage));
        } catch (const std::exception& e) {
            nnerr(std::string("failed to save stage progress: ") + e.what());
        }
    }
    int stage_ = 1;
    bool paused_ = false;
    bool stage_clear_saved_ = false;
};

// 起動時スプラッシュ
class SplashScene final : public NeneNode {
public:
    explicit SplashScene(std::string name) : NeneNode(std::move(name)) {}
protected:
    void init_node() override {
        if (!asset_loader || !path_service) {
            nnthrow("services not ready (asset_loader/path_service)");
        }

        splash_tex_ = asset_loader->get_texture(path_service->resolve(kSplashImagePath));
        if (!splash_tex_) nnthrow("failed to load splash texture");
        SDL_SetTextureBlendMode(splash_tex_, SDL_BLENDMODE_BLEND);

        submit_super_chrome_dino_asset_tasks(
            tasks_, task_server, asset_loader, font_loader, sound_loader, path_service);
    }

    void handle_time_lapse(const float& dt) override {
        switch (phase_) {
            case Phase::FadeIn:
                alpha_ += dt / kFadeInSec;
                if (alpha_ >= 1.0f) {
                    alpha_ = 1.0f;
                    phase_ = Phase::Hold;
                }
                break;
            case Phase::Hold:
                alpha_ = 1.0f;
                if (tasks_.all_done()) {
                    report_task_errors_();
                    phase_ = Phase::FadeOut;
                }
                break;
            case Phase::FadeOut:
                alpha_ -= dt / kFadeOutSec;
                if (alpha_ <= 0.0f) {
                    alpha_ = 0.0f;
                    phase_ = Phase::Done;
                    send_mail(NeneMail("scene_switch", this->name, "switch_to", "title_scene"));
                }
                break;
            case Phase::Done:
                break;
        }
    }

    void render(SDL_Renderer* r) override {
        if (!r || !splash_tex_) return;

        int w = 0, h = 0;
        if (!SDL_GetRenderOutputSize(r, &w, &h)) return;
        float tw = 0.0f, th = 0.0f;
        if (!SDL_GetTextureSize(splash_tex_, &tw, &th)) return;
        if (tw <= 0.0f || th <= 0.0f) return;

        const float ww = static_cast<float>(w);
        const float wh = static_cast<float>(h);
        const float scale = std::min(ww / tw, wh / th);
        const float dw = tw * scale;
        const float dh = th * scale;
        const SDL_FRect dst{ (ww - dw) * 0.5f, (wh - dh) * 0.5f, dw, dh };

        SDL_SetTextureAlphaMod(splash_tex_, alpha_byte(alpha_));
        SDL_RenderTexture(r, splash_tex_, nullptr, &dst);
    }
private:
    enum class Phase {
        FadeIn,
        Hold,
        FadeOut,
        Done,
    };

    void report_task_errors_() {
        if (reported_task_errors_) return;
        reported_task_errors_ = true;
        if (tasks_.any_failed()) {
            nnerr(std::string("asset preload failed: ") + tasks_.first_error());
        }
    }

    static constexpr float kFadeInSec = 0.8f;
    static constexpr float kFadeOutSec = 0.45f;
    SDL_Texture* splash_tex_ = nullptr;
    NeneTaskGroup tasks_;
    Phase phase_ = Phase::FadeIn;
    float alpha_ = 0.0f;
    bool reported_task_errors_ = false;
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
        lock_tex_ = asset_loader->get_texture(path_service->resolve("assets/ui/GoogleFontsIcons/lock_40dp_FFFFFF_FILL0_wght400_GRAD0_opsz40.png"));
        key_up_tex_ = asset_loader->get_texture(path_service->resolve("assets/ui/kenney_input-prompts_1.5/Keyboard & Mouse/Default/keyboard_arrow_up.png"));
        key_down_tex_ = asset_loader->get_texture(path_service->resolve("assets/ui/kenney_input-prompts_1.5/Keyboard & Mouse/Default/keyboard_arrow_down.png"));
        key_space_tex_ = asset_loader->get_texture(path_service->resolve("assets/ui/kenney_input-prompts_1.5/Keyboard & Mouse/Default/keyboard_space_icon.png"));
        // フォント
        font_path_ = path_service->resolve("assets/fonts/NotoSansJP-Regular.ttf");
        // タイトル文字
        title_tex_ = font_loader->get_text_texture(font_path_, 56, "SuperChromeDino", SDL_Color{255, 255, 255, 255});
        continue_tex_ = font_loader->get_text_texture(font_path_, 32, "Continue", SDL_Color{255, 255, 255, 255});
        continue_disabled_tex_ = font_loader->get_text_texture(font_path_, 32, "Continue", SDL_Color{150, 150, 150, 255});
        new_game_tex_ = font_loader->get_text_texture(font_path_, 32, "New Game", SDL_Color{255, 255, 255, 255});
        settings_tex_ = font_loader->get_text_texture(font_path_, 32, "Settings", SDL_Color{255, 255, 255, 255});
        quit_game_tex_ = font_loader->get_text_texture(font_path_, 32, "Quit Game", SDL_Color{255, 255, 255, 255});
        select_tex_ = font_loader->get_text_texture(font_path_, 24, "Select", SDL_Color{255, 255, 255, 255});
        confirm_tex_ = font_loader->get_text_texture(font_path_, 24, "Confirm", SDL_Color{255, 255, 255, 255});
        continue_available_ = save_service && save_service->slot_exists(kStageProgressSlot);
        selected_item_ = continue_available_ ? MenuItem::Continue : MenuItem::NewGame;
    }
    void render(SDL_Renderer* r) override {
        if (!r) return;
        if (!sprite_tex_ || !title_tex_) return;
        // 画面サイズ
        int w = 0, h = 0;
        if (!SDL_GetRenderOutputSize(r, &w, &h)) return;
        const float ww = static_cast<float>(w);
        const float wh = static_cast<float>(h);
        const float unit = std::min(ww, wh);
        // 恐竜 (テクスチャアトラス内)
        const SDL_FRect dino_src { 1514.2f, 0.0f, 88.0f, 96.0f };
        const float dino_h = unit * 0.174f;
        const float dino_w = dino_h * (88.0f / 96.0f);
        // タイトル文字のテクスチャサイズ
        float text_w = 0.0f, text_h = 0.0f;
        if (!SDL_GetTextureSize(title_tex_, &text_w, &text_h)) {
            text_w = 0.0f;
            text_h = 0.0f;
        }
        // 横並びレイアウト：中央寄せ
        const float gap = 28.0f;
        const float total_w = dino_w + gap + text_w;
        const float group_h = (dino_h > text_h) ? dino_h : text_h;
        const float x0 = (ww - total_w) * 0.5f;
        const float y0 = wh * 0.37f - group_h * 0.5f;
        SDL_FRect dino_dst { x0, y0 + (group_h - dino_h) * 0.5f, dino_w, dino_h };
        SDL_FRect text_dst { x0 + dino_w + gap, y0 + (group_h - text_h) * 0.5f, text_w, text_h };
        SDL_RenderTexture(r, sprite_tex_, &dino_src, &dino_dst);
        SDL_RenderTexture(r, title_tex_, nullptr, &text_dst);

        const float menu_x = ww * 0.442f;
        const float menu_y = wh * 0.640f;
        const float menu_gap = unit * 0.052f;
        render_menu_item_(r, MenuItem::Continue, continue_available_ ? continue_tex_ : continue_disabled_tex_,
                          menu_x, menu_y);
        render_menu_item_(r, MenuItem::NewGame, new_game_tex_, menu_x, menu_y + menu_gap);
        render_menu_item_(r, MenuItem::Settings, settings_tex_, menu_x, menu_y + menu_gap * 2.0f);
        render_menu_item_(r, MenuItem::QuitGame, quit_game_tex_, menu_x, menu_y + menu_gap * 3.0f);
        if (!continue_available_) {
            const float lock_size = unit * 0.037f;
            float continue_w = 0.0f, continue_h = 0.0f;
            if (continue_disabled_tex_) SDL_GetTextureSize(continue_disabled_tex_, &continue_w, &continue_h);
            render_texture_scaled_centered_(r, lock_tex_, menu_x - unit * 0.052f, menu_y + continue_h * 0.5f,
                                            lock_size, lock_size);
        }
        render_controls_(r, ww, wh, unit);
    }
    void handle_sdl_event(const SDL_Event& ev) override
    {
        if (ev.type != SDL_EVENT_KEY_DOWN) return;
        if (ev.key.repeat) return;
        if (ev.key.key == SDLK_UP) {
            move_selection_(-1);
            return;
        }
        if (ev.key.key == SDLK_DOWN) {
            move_selection_(1);
            return;
        }
        if (ev.key.key == SDLK_SPACE) {
            decide_();
        }
    }
private:
    enum class MenuItem {
        Continue = 0,
        NewGame = 1,
        Settings = 2,
        QuitGame = 3
    };

    bool is_enabled_(MenuItem item) const {
        return item != MenuItem::Continue || continue_available_;
    }
    void move_selection_(int delta) {
        const auto before = selected_item_;
        int next = static_cast<int>(selected_item_);
        for (int i = 0; i < 4; ++i) {
            next += delta;
            if (next < 0) next = 3;
            if (next > 3) next = 0;
            const auto item = static_cast<MenuItem>(next);
            if (is_enabled_(item)) {
                selected_item_ = item;
                if (selected_item_ != before) {
                    play_sound_effect(sound_loader, path_service, blackboard, kSeCursorMovePath);
                }
                return;
            }
        }
    }
    void decide_() {
        if (selected_item_ == MenuItem::Continue) {
            if (!continue_available_) return;
            play_sound_effect(sound_loader, path_service, blackboard, kSeConfirmPath);
            if (blackboard) blackboard->setf("selected_stage", static_cast<float>(continue_stage_()));
            send_mail(NeneMail("scene_switch", this->name, "switch_to", "stage_select_scene"));
            return;
        }
        if (selected_item_ == MenuItem::NewGame) {
            play_sound_effect(sound_loader, path_service, blackboard, kSeConfirmPath);
            if (save_service) save_service->remove(kStageProgressSlot);
            if (blackboard) blackboard->setf("selected_stage", 1.0f);
            send_mail(NeneMail("scene_switch", this->name, "switch_to", "stage_select_scene"));
            return;
        }
        if (selected_item_ == MenuItem::Settings) {
            play_sound_effect(sound_loader, path_service, blackboard, kSeConfirmPath);
            send_mail(NeneMail("scene_switch", this->name, "switch_to", "settings_scene"));
            return;
        }
        if (selected_item_ == MenuItem::QuitGame) {
            play_sound_effect(sound_loader, path_service, blackboard, kSeConfirmPath);
            const std::string to = blackboard ? blackboard->root_name : std::string("game");
            send_mail(NeneMail(to, this->name, "quit", ""));
        }
    }
    int continue_stage_() const {
        bool cleared[kStageCount]{};
        if (save_service && save_service->slot_exists(kStageProgressSlot)) {
            try {
                const NeneSaveDocument doc = save_service->load(kStageProgressSlot);
                const auto* record = doc.find_node(kStageProgressNode);
                if (record) {
                    NeneSaveReader reader(*record);
                    for (int i = 0; i < kStageCount; ++i) {
                        cleared[i] = reader.get_bool(stage_clear_key(i + 1), false);
                    }
                }
            } catch (const std::exception& e) {
                nnerr(std::string("failed to load stage progress: ") + e.what());
            }
        }
        for (int stage = 1; stage <= kStageCount; ++stage) {
            if (!cleared[stage - 1]) return stage;
        }
        return kStageCount;
    }
    void render_menu_item_(SDL_Renderer* r, MenuItem item, SDL_Texture* text, float x, float y) const {
        float text_w = 0.0f, text_h = 0.0f;
        if (text) SDL_GetTextureSize(text, &text_w, &text_h);
        if (selected_item_ == item) {
            const float pointer_y = y + text_h * 0.5f;
            draw_filled_circle_(r, x - 28.0f, pointer_y, 10.0f, SDL_FColor{0.28f, 0.55f, 0.70f, 1.0f});
            draw_circle_outline_(r, x - 28.0f, pointer_y, 10.0f, SDL_Color{75, 150, 205, 255});
        }
        render_texture_left_(r, text, x, y);
    }
    void render_controls_(SDL_Renderer* r, float ww, float wh, float unit) const {
        const float key = unit * 0.0665f;
        const float y = wh - key - unit * 0.017f;
        const float label_y = y + key * 0.5f;
        const float gap = unit * 0.017f;

        const float select_x = ww * 0.282f;
        render_texture_scaled_(r, key_up_tex_, select_x, y, key, key);
        render_texture_scaled_(r, key_down_tex_, select_x + key + gap, y, key, key);
        render_texture_left_center_(r, select_tex_, select_x + (key + gap) * 2.0f + unit * 0.032f, label_y);

        const float confirm_x = ww * 0.516f;
        render_texture_scaled_(r, key_space_tex_, confirm_x, y, key, key);
        render_texture_left_center_(r, confirm_tex_, confirm_x + key + unit * 0.042f, label_y);
    }
    static void draw_filled_circle_(SDL_Renderer* r, float cx, float cy, float radius, SDL_FColor color) {
        constexpr int kSegments = 36;
        std::vector<SDL_Vertex> vertices;
        std::vector<int> indices;
        vertices.reserve(kSegments + 1);
        indices.reserve(kSegments * 3);
        vertices.push_back(SDL_Vertex{ SDL_FPoint{cx, cy}, color, SDL_FPoint{0.0f, 0.0f} });
        constexpr float kTwoPi = 6.2831853071795864769f;
        for (int i = 0; i < kSegments; ++i) {
            const float a = (static_cast<float>(i) / static_cast<float>(kSegments)) * kTwoPi;
            vertices.push_back(SDL_Vertex{
                SDL_FPoint{cx + std::cos(a) * radius, cy + std::sin(a) * radius},
                color,
                SDL_FPoint{0.0f, 0.0f}
            });
        }
        for (int i = 1; i <= kSegments; ++i) {
            indices.push_back(0);
            indices.push_back(i);
            indices.push_back((i == kSegments) ? 1 : i + 1);
        }
        SDL_RenderGeometry(r, nullptr, vertices.data(), static_cast<int>(vertices.size()),
                           indices.data(), static_cast<int>(indices.size()));
    }
    static void draw_circle_outline_(SDL_Renderer* r, float cx, float cy, float radius, SDL_Color color) {
        constexpr int kSegments = 48;
        SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
        constexpr float kTwoPi = 6.2831853071795864769f;
        float px = cx + radius;
        float py = cy;
        for (int i = 1; i <= kSegments; ++i) {
            const float a = (static_cast<float>(i) / static_cast<float>(kSegments)) * kTwoPi;
            const float x = cx + std::cos(a) * radius;
            const float y = cy + std::sin(a) * radius;
            SDL_RenderLine(r, px, py, x, y);
            px = x;
            py = y;
        }
    }
    static void render_texture_left_(SDL_Renderer* r, SDL_Texture* tex, float x, float y) {
        if (!tex) return;
        float tw = 0.0f, th = 0.0f;
        SDL_GetTextureSize(tex, &tw, &th);
        SDL_FRect dst{ x, y, tw, th };
        SDL_RenderTexture(r, tex, nullptr, &dst);
    }
    static void render_texture_left_center_(SDL_Renderer* r, SDL_Texture* tex, float left, float cy) {
        if (!tex) return;
        float tw = 0.0f, th = 0.0f;
        SDL_GetTextureSize(tex, &tw, &th);
        SDL_FRect dst{ left, cy - th * 0.5f, tw, th };
        SDL_RenderTexture(r, tex, nullptr, &dst);
    }
    static void render_texture_scaled_(SDL_Renderer* r, SDL_Texture* tex, float x, float y, float w, float h) {
        if (!tex) return;
        SDL_FRect dst{ x, y, w, h };
        SDL_RenderTexture(r, tex, nullptr, &dst);
    }
    static void render_texture_scaled_centered_(SDL_Renderer* r, SDL_Texture* tex, float cx, float cy, float w, float h) {
        if (!tex) return;
        SDL_FRect dst{ cx - w * 0.5f, cy - h * 0.5f, w, h };
        SDL_RenderTexture(r, tex, nullptr, &dst);
    }
    SDL_Texture* sprite_tex_ = nullptr;
    SDL_Texture* title_tex_  = nullptr;
    SDL_Texture* lock_tex_ = nullptr;
    SDL_Texture* continue_tex_ = nullptr;
    SDL_Texture* continue_disabled_tex_ = nullptr;
    SDL_Texture* new_game_tex_ = nullptr;
    SDL_Texture* settings_tex_ = nullptr;
    SDL_Texture* quit_game_tex_ = nullptr;
    SDL_Texture* select_tex_ = nullptr;
    SDL_Texture* confirm_tex_ = nullptr;
    SDL_Texture* key_up_tex_ = nullptr;
    SDL_Texture* key_down_tex_ = nullptr;
    SDL_Texture* key_space_tex_ = nullptr;
    std::string font_path_;
    MenuItem selected_item_ = MenuItem::NewGame;
    bool continue_available_ = false;
};

// ユーザー設定シーン
class SettingsScene final : public NeneNode {
public:
    explicit SettingsScene(std::string name) : NeneNode(std::move(name)) {}
protected:
    void init_node() override {
        if (!asset_loader || !font_loader || !path_service || !blackboard) {
            nnthrow("services not ready (asset_loader/font_loader/path_service/blackboard)");
        }
        ensure_game_settings(*blackboard);
        apply_game_settings(*blackboard);
        if (sound_loader) apply_sound_settings(*sound_loader, *blackboard);

        font_path_ = path_service->resolve("assets/fonts/NotoSansJP-Regular.ttf");
        key_up_tex_ = asset_loader->get_texture(path_service->resolve("assets/ui/kenney_input-prompts_1.5/Keyboard & Mouse/Default/keyboard_arrow_up.png"));
        key_down_tex_ = asset_loader->get_texture(path_service->resolve("assets/ui/kenney_input-prompts_1.5/Keyboard & Mouse/Default/keyboard_arrow_down.png"));
        key_left_tex_ = asset_loader->get_texture(path_service->resolve("assets/ui/kenney_input-prompts_1.5/Keyboard & Mouse/Default/keyboard_arrow_left.png"));
        key_right_tex_ = asset_loader->get_texture(path_service->resolve("assets/ui/kenney_input-prompts_1.5/Keyboard & Mouse/Default/keyboard_arrow_right.png"));
        key_escape_tex_ = asset_loader->get_texture(path_service->resolve("assets/ui/kenney_input-prompts_1.5/Keyboard & Mouse/Default/keyboard_escape.png"));

        title_tex_ = font_loader->get_text_texture(font_path_, 40, "Settings", SDL_Color{255,255,255,255});
        select_option_tex_ = font_loader->get_text_texture(font_path_, 24, "Select Option", SDL_Color{255,255,255,255});
        change_option_tex_ = font_loader->get_text_texture(font_path_, 24, "Change Option", SDL_Color{255,255,255,255});
        return_title_tex_ = font_loader->get_text_texture(font_path_, 24, "Return to Title", SDL_Color{255,255,255,255});
        saved_tex_ = font_loader->get_text_texture(font_path_, 24, "Saved!", SDL_Color{255,255,255,255});
        autosave_settings_();
    }
    void render(SDL_Renderer* r) override {
        if (!r || !blackboard) return;
        int w = 0, h = 0;
        if (!SDL_GetRenderOutputSize(r, &w, &h)) return;

        const float ww = static_cast<float>(w);
        const float wh = static_cast<float>(h);
        const float unit = std::min(ww, wh);
        const float title_x = ww * 0.041f;
        const float title_y = wh * 0.112f;
        const float label_right = ww * 0.197f;
        const float divider_x = ww * 0.233f;
        const float option_x = ww * 0.269f;
        const float option2_x = ww * 0.405f;
        const float y0 = wh * 0.305f;
        const float gap_y = unit * 0.098f;

        render_texture_left_top_(r, title_tex_, title_x, title_y);

        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        SDL_RenderLine(r, divider_x, wh * 0.262f, divider_x, wh * 0.692f);

        for (int i = 0; i < kItemCount; ++i) {
            const float y = y0 + gap_y * static_cast<float>(i);
            render_label_(r, i, label_right, (i == 4) ? y + 12.0f : y);
        }
        render_fps_(r, option_x, option2_x, y0);
        render_volume_(r, kSettingSeVolume, option_x, y0 + gap_y);
        render_volume_(r, kSettingBgmVolume, option_x, y0 + gap_y * 2.0f);
        render_binary_(r, option_x, option2_x, y0 + gap_y * 3.0f,
                       "Show", "Hide", blackboard->getf(kSettingCursorVisible, 1.0f) > 0.5f);
        render_dino_control_(r, option_x, option2_x, y0 + gap_y * 4.0f);
        render_controls_(r, ww, wh, unit);
        render_saved_(r, ww, wh, unit);
    }
    void handle_sdl_event(const SDL_Event& ev) override {
        if (ev.type != SDL_EVENT_KEY_DOWN) return;
        if (ev.key.repeat) return;
        if (ev.key.key == SDLK_UP) {
            move_selection_(-1);
            return;
        }
        if (ev.key.key == SDLK_DOWN) {
            move_selection_(1);
            return;
        }
        if (ev.key.key == SDLK_LEFT) {
            change_option_(-1);
            return;
        }
        if (ev.key.key == SDLK_RIGHT) {
            change_option_(1);
            return;
        }
        if (ev.key.key == SDLK_ESCAPE) {
            play_sound_effect(sound_loader, path_service, blackboard, kSeConfirmPath);
            send_mail(NeneMail("scene_switch", this->name, "switch_to", "title_scene"));
        }
    }
private:
    static constexpr int kItemCount = 5;

    void move_selection_(int delta) {
        selected_item_ += delta;
        if (selected_item_ < 0) selected_item_ = kItemCount - 1;
        if (selected_item_ >= kItemCount) selected_item_ = 0;
        play_sound_effect(sound_loader, path_service, blackboard, kSeCursorMovePath);
    }
    void change_option_(int delta) {
        if (!blackboard) return;
        if (selected_item_ == 0) {
            blackboard->fps = (blackboard->fps == 60) ? 30 : 60;
        } else if (selected_item_ == 1) {
            const int v = std::clamp(volume_value_(kSettingSeVolume) + delta, 0, 9);
            blackboard->set_persistentf(kSettingSeVolume, static_cast<float>(v));
        } else if (selected_item_ == 2) {
            const int v = std::clamp(volume_value_(kSettingBgmVolume) + delta, 0, 9);
            blackboard->set_persistentf(kSettingBgmVolume, static_cast<float>(v));
        } else if (selected_item_ == 3) {
            const bool show = blackboard->getf(kSettingCursorVisible, 1.0f) <= 0.5f;
            blackboard->set_persistentf(kSettingCursorVisible, show ? 1.0f : 0.0f);
            apply_cursor_setting(*blackboard);
        } else if (selected_item_ == 4) {
            const int next = dino_control_preset(*blackboard) == 0 ? 1 : 0;
            blackboard->set_persistentf(kSettingDinoControl, static_cast<float>(next));
            apply_dino_control_setting(*blackboard);
        }
        if (sound_loader) apply_sound_settings(*sound_loader, *blackboard);
        play_sound_effect(sound_loader, path_service, blackboard, kSeCursorMovePath);
        autosave_settings_();
    }
    int volume_value_(const std::string& key) const {
        if (!blackboard) return 5;
        return std::clamp(static_cast<int>(std::round(blackboard->getf(key, 5.0f))), 0, 9);
    }
    void autosave_settings_() {
        if (!save_service || !blackboard) return;
        try {
            NeneSaveDocument doc;
            doc.set_metadata("root_name", blackboard->root_name);
            blackboard->save_settings(doc);
            save_service->save(kSettingsSlot, doc);
            saved_visible_ = true;
        } catch (const std::exception& e) {
            saved_visible_ = false;
            nnerr(std::string("failed to autosave settings: ") + e.what());
        }
    }
    SDL_Texture* text_(const std::string& text, int size, SDL_Color color) {
        return font_loader->get_text_texture(font_path_, size, text, color);
    }
    void render_label_(SDL_Renderer* r, int index, float right, float cy) {
        const SDL_Color color = (index == selected_item_)
            ? SDL_Color{255,255,255,255}
            : SDL_Color{160,160,160,255};
        static constexpr const char* labels[kItemCount] = {
            "FPS", "SE Volume", "BGM Volume", "Cursor in Window", "Dino Control"
        };
        render_texture_right_center_(r, text_(labels[index], 28, color), right, cy);
    }
    void render_fps_(SDL_Renderer* r, float x0, float x1, float cy) {
        const bool is_60 = blackboard && blackboard->fps == 60;
        render_option_text_(r, "60", x0, cy, is_60);
        render_option_text_(r, "30", x1, cy, !is_60);
    }
    void render_volume_(SDL_Renderer* r, const std::string& key, float x, float cy) {
        const int selected = volume_value_(key);
        const float digit_gap = 27.0f;
        for (int i = 0; i <= 9; ++i) {
            render_option_text_(r, std::to_string(i), x + digit_gap * static_cast<float>(i), cy, i == selected);
        }
    }
    void render_binary_(SDL_Renderer* r, float x0, float x1, float cy,
                        const std::string& left, const std::string& right, bool left_selected) {
        render_option_text_(r, left, x0, cy, left_selected);
        render_option_text_(r, right, x1, cy, !left_selected);
    }
    void render_dino_control_(SDL_Renderer* r, float x0, float x1, float cy) {
        const bool arrows = !blackboard || dino_control_preset(*blackboard) == 0;
        render_option_text_(r, "Arrows", x0, cy - 12.0f, arrows);
        render_option_text_(r, "+ Space", x0, cy + 17.0f, arrows);
        render_option_text_(r, "WASD", x1, cy - 12.0f, !arrows);
        render_option_text_(r, "+ Enter", x1, cy + 17.0f, !arrows);
    }
    void render_option_text_(SDL_Renderer* r, const std::string& text, float x, float cy, bool selected) {
        const SDL_Color color = selected ? SDL_Color{255,255,255,255} : SDL_Color{160,160,160,255};
        render_texture_left_center_(r, text_(text, 28, color), x, cy);
    }
    void render_controls_(SDL_Renderer* r, float ww, float wh, float unit) {
        const float key = unit * 0.0665f;
        const float gap = unit * 0.017f;
        const float x = ww * 0.703f;
        const float y = wh * 0.321f;
        const float line_gap = unit * 0.139f;
        const float label_x = x + key * 2.0f + gap + unit * 0.032f;

        render_texture_scaled_(r, key_up_tex_, x, y, key, key);
        render_texture_scaled_(r, key_down_tex_, x + key + gap, y, key, key);
        render_texture_left_center_(r, select_option_tex_, label_x, y + key * 0.5f);

        render_texture_scaled_(r, key_left_tex_, x, y + line_gap, key, key);
        render_texture_scaled_(r, key_right_tex_, x + key + gap, y + line_gap, key, key);
        render_texture_left_center_(r, change_option_tex_, label_x, y + line_gap + key * 0.5f);

        render_texture_scaled_(r, key_escape_tex_, x, y + line_gap * 2.0f, key, key);
        render_texture_left_center_(r, return_title_tex_, label_x - key - gap, y + line_gap * 2.0f + key * 0.5f);
    }
    void render_saved_(SDL_Renderer* r, float ww, float wh, float unit) {
        if (!saved_visible_) return;
        const float x = ww * 0.020f;
        const float y = wh - unit * 0.058f;
        render_texture_left_center_(r, saved_tex_, x, y);
    }
    static void render_texture_left_top_(SDL_Renderer* r, SDL_Texture* tex, float x, float y) {
        if (!tex) return;
        float tw = 0.0f, th = 0.0f;
        SDL_GetTextureSize(tex, &tw, &th);
        SDL_FRect dst{ x, y, tw, th };
        SDL_RenderTexture(r, tex, nullptr, &dst);
    }
    static void render_texture_left_center_(SDL_Renderer* r, SDL_Texture* tex, float left, float cy) {
        if (!tex) return;
        float tw = 0.0f, th = 0.0f;
        SDL_GetTextureSize(tex, &tw, &th);
        SDL_FRect dst{ left, cy - th * 0.5f, tw, th };
        SDL_RenderTexture(r, tex, nullptr, &dst);
    }
    static void render_texture_right_center_(SDL_Renderer* r, SDL_Texture* tex, float right, float cy) {
        if (!tex) return;
        float tw = 0.0f, th = 0.0f;
        SDL_GetTextureSize(tex, &tw, &th);
        SDL_FRect dst{ right - tw, cy - th * 0.5f, tw, th };
        SDL_RenderTexture(r, tex, nullptr, &dst);
    }
    static void render_texture_scaled_(SDL_Renderer* r, SDL_Texture* tex, float x, float y, float w, float h) {
        if (!tex) return;
        SDL_FRect dst{ x, y, w, h };
        SDL_RenderTexture(r, tex, nullptr, &dst);
    }

    std::string font_path_;
    SDL_Texture* title_tex_ = nullptr;
    SDL_Texture* select_option_tex_ = nullptr;
    SDL_Texture* change_option_tex_ = nullptr;
    SDL_Texture* return_title_tex_ = nullptr;
    SDL_Texture* saved_tex_ = nullptr;
    SDL_Texture* key_up_tex_ = nullptr;
    SDL_Texture* key_down_tex_ = nullptr;
    SDL_Texture* key_left_tex_ = nullptr;
    SDL_Texture* key_right_tex_ = nullptr;
    SDL_Texture* key_escape_tex_ = nullptr;
    int selected_item_ = 0;
    bool saved_visible_ = false;
};

// ステージ選択シーン
class StageSelectScene final : public NeneNode {
public:
    explicit StageSelectScene(std::string name) : NeneNode(std::move(name)) {}
protected:
    void init_node() override {
        if (!asset_loader || !font_loader || !path_service || !blackboard) {
            nnthrow("services not ready (asset_loader/font_loader/path_service/blackboard)");
        }
        font_path_ = path_service->resolve("assets/fonts/NotoSansJP-Regular.ttf");
        sprite_tex_ = asset_loader->get_texture(path_service->resolve("assets/sprites/sprite.png"));
        lock_tex_ = asset_loader->get_texture(path_service->resolve("assets/ui/GoogleFontsIcons/lock_40dp_FFFFFF_FILL0_wght400_GRAD0_opsz40.png"));
        check_tex_ = asset_loader->get_texture(path_service->resolve("assets/ui/GoogleFontsIcons/check_40dp_FFFFFF_FILL0_wght400_GRAD0_opsz40.png"));
        key_left_tex_ = asset_loader->get_texture(path_service->resolve("assets/ui/kenney_input-prompts_1.5/Keyboard & Mouse/Default/keyboard_arrow_left.png"));
        key_right_tex_ = asset_loader->get_texture(path_service->resolve("assets/ui/kenney_input-prompts_1.5/Keyboard & Mouse/Default/keyboard_arrow_right.png"));
        key_space_tex_ = asset_loader->get_texture(path_service->resolve("assets/ui/kenney_input-prompts_1.5/Keyboard & Mouse/Default/keyboard_space_icon.png"));
        key_escape_tex_ = asset_loader->get_texture(path_service->resolve("assets/ui/kenney_input-prompts_1.5/Keyboard & Mouse/Default/keyboard_escape.png"));

        title_tex_ = font_loader->get_text_texture(font_path_, 40, "Stage Select", SDL_Color{255,255,255,255});
        select_tex_ = font_loader->get_text_texture(font_path_, 24, "Select", SDL_Color{255,255,255,255});
        confirm_tex_ = font_loader->get_text_texture(font_path_, 24, "Confirm", SDL_Color{255,255,255,255});
        return_tex_ = font_loader->get_text_texture(font_path_, 24, "Return to Title", SDL_Color{255,255,255,255});
        for (int i = 0; i < kStageCount; ++i) {
            stage_num_tex_[i] = font_loader->get_text_texture(
                font_path_, 44, std::to_string(i + 1), SDL_Color{255,255,255,255});
        }
        load_progress_();
        selected_stage_ = static_cast<int>(blackboard->getf("selected_stage", 1.0f));
        if (selected_stage_ < 1 || selected_stage_ > kStageCount) selected_stage_ = 1;
        if (!is_stage_unlocked_(selected_stage_)) selected_stage_ = first_unlocked_stage_();
        blackboard->setf("selected_stage", static_cast<float>(selected_stage_));
    }
    void render(SDL_Renderer* r) override {
        if (!r) return;
        int w = 0, h = 0;
        if (!SDL_GetRenderOutputSize(r, &w, &h)) return;

        const float ww = static_cast<float>(w);
        const float wh = static_cast<float>(h);
        const float unit = std::min(ww, wh);
        const float title_y = wh * 0.112f;
        const float line_y = wh * 0.542f;
        const float node_radius = unit * 0.0265f;
        const float node_xs[kStageCount] = { ww * 0.247f, ww * 0.502f, ww * 0.769f };

        render_texture_centered_(r, title_tex_, ww * 0.5f, title_y);

        SDL_SetRenderDrawColor(r, 220, 220, 220, 255);
        SDL_RenderLine(r, node_xs[0], line_y, node_xs[kStageCount - 1], line_y);

        for (int i = 0; i < kStageCount; ++i) {
            const int stage = i + 1;
            draw_filled_circle_(r, node_xs[i], line_y, node_radius, SDL_FColor{0.97f, 0.97f, 0.95f, 1.0f});

            const float number_y = line_y + unit * 0.092f;
            render_texture_centered_(r, stage_num_tex_[i], node_xs[i], number_y);
            if (cleared_[i]) {
                const float check_size = unit * 0.038f;
                render_texture_scaled_centered_(r, check_tex_, node_xs[i] + unit * 0.052f, number_y + 1.0f,
                                                check_size, check_size);
            }
            if (!is_stage_unlocked_(stage)) {
                const float lock_size = unit * 0.047f;
                render_texture_scaled_centered_(r, lock_tex_, node_xs[i],
                                                line_y - node_radius - lock_size * 0.95f,
                                                lock_size, lock_size);
            }
        }

        render_selected_dino_(r, node_xs[selected_stage_ - 1], line_y, node_radius, unit);
        render_controls_(r, ww, wh, unit);
    }
    void handle_sdl_event(const SDL_Event& ev) override {
        if (ev.type != SDL_EVENT_KEY_DOWN) return;
        if (ev.key.repeat) return;
        if (ev.key.key == SDLK_LEFT) {
            move_selection_(-1);
            return;
        }
        if (ev.key.key == SDLK_RIGHT) {
            move_selection_(1);
            return;
        }
        if (ev.key.key == SDLK_SPACE) {
            if (!is_stage_unlocked_(selected_stage_)) return;
            play_sound_effect(sound_loader, path_service, blackboard, kSeConfirmPath);
            if (blackboard) blackboard->setf("selected_stage", static_cast<float>(selected_stage_));
            send_mail(NeneMail("scene_switch", this->name, "switch_to", stage_scene_name(selected_stage_)));
            return;
        }
        if (ev.key.key == SDLK_ESCAPE) {
            play_sound_effect(sound_loader, path_service, blackboard, kSeConfirmPath);
            send_mail(NeneMail("scene_switch", this->name, "switch_to", "title_scene"));
        }
    }
private:
    bool is_stage_unlocked_(int stage) const {
        if (stage <= 1) return true;
        if (stage > kStageCount) return false;
        return cleared_[stage - 2];
    }
    int first_unlocked_stage_() const {
        for (int stage = 1; stage <= kStageCount; ++stage) {
            if (is_stage_unlocked_(stage)) return stage;
        }
        return 1;
    }
    void move_selection_(int delta) {
        const int before = selected_stage_;
        int next = selected_stage_;
        for (int i = 0; i < kStageCount; ++i) {
            next += delta;
            if (next < 1) next = kStageCount;
            if (next > kStageCount) next = 1;
            if (is_stage_unlocked_(next)) {
                selected_stage_ = next;
                if (blackboard) blackboard->setf("selected_stage", static_cast<float>(selected_stage_));
                if (selected_stage_ != before) {
                    play_sound_effect(sound_loader, path_service, blackboard, kSeCursorMovePath);
                }
                return;
            }
        }
    }
    void load_progress_() {
        for (int i = 0; i < kStageCount; ++i) cleared_[i] = false;
        if (!save_service) return;
        try {
            if (!save_service->slot_exists(kStageProgressSlot)) return;
            const NeneSaveDocument doc = save_service->load(kStageProgressSlot);
            const auto* record = doc.find_node(kStageProgressNode);
            if (!record) return;
            NeneSaveReader reader(*record);
            for (int i = 0; i < kStageCount; ++i) {
                cleared_[i] = reader.get_bool(stage_clear_key(i + 1), false);
            }
        } catch (const std::exception& e) {
            nnerr(std::string("failed to load stage progress: ") + e.what());
        }
    }
    void render_selected_dino_(SDL_Renderer* r, float cx, float line_y, float node_radius, float unit) const {
        if (!sprite_tex_) return;
        const SDL_FRect dino_src{ 1514.2f, 0.0f, 88.0f, 96.0f };
        const float dh = unit * 0.111f;
        const float dw = dh * (88.0f / 96.0f);
        const float bottom = line_y - node_radius - unit * 0.025f;
        SDL_FRect dst{ cx - dw * 0.5f, bottom - dh, dw, dh };
        SDL_RenderTexture(r, sprite_tex_, &dino_src, &dst);
    }
    void render_controls_(SDL_Renderer* r, float ww, float wh, float unit) const {
        const float key = unit * 0.0665f;
        const float y = wh - key - unit * 0.015f;
        const float label_y = y + key * 0.5f;
        const float gap = unit * 0.017f;

        const float select_x = ww * 0.243f;
        render_texture_scaled_(r, key_left_tex_, select_x, y, key, key);
        render_texture_scaled_(r, key_right_tex_, select_x + key + gap, y, key, key);
        render_texture_left_center_(r, select_tex_, select_x + (key + gap) * 2.0f + unit * 0.012f, label_y);

        const float confirm_x = ww * 0.467f;
        render_texture_scaled_(r, key_space_tex_, confirm_x, y, key, key);
        render_texture_left_center_(r, confirm_tex_, confirm_x + key + unit * 0.040f, label_y);

        const float return_x = ww * 0.671f;
        render_texture_scaled_(r, key_escape_tex_, return_x, y, key, key);
        render_texture_left_center_(r, return_tex_, return_x + key + unit * 0.040f, label_y);
    }
    static void draw_filled_circle_(SDL_Renderer* r, float cx, float cy, float radius, SDL_FColor color) {
        constexpr int kSegments = 48;
        std::vector<SDL_Vertex> vertices;
        std::vector<int> indices;
        vertices.reserve(kSegments + 1);
        indices.reserve(kSegments * 3);
        vertices.push_back(SDL_Vertex{ SDL_FPoint{cx, cy}, color, SDL_FPoint{0.0f, 0.0f} });
        constexpr float kTwoPi = 6.2831853071795864769f;
        for (int i = 0; i < kSegments; ++i) {
            const float a = (static_cast<float>(i) / static_cast<float>(kSegments)) * kTwoPi;
            vertices.push_back(SDL_Vertex{
                SDL_FPoint{cx + std::cos(a) * radius, cy + std::sin(a) * radius},
                color,
                SDL_FPoint{0.0f, 0.0f}
            });
        }
        for (int i = 1; i <= kSegments; ++i) {
            indices.push_back(0);
            indices.push_back(i);
            indices.push_back((i == kSegments) ? 1 : i + 1);
        }
        SDL_RenderGeometry(r, nullptr, vertices.data(), static_cast<int>(vertices.size()),
                           indices.data(), static_cast<int>(indices.size()));
    }
    static void draw_circle_outline_(SDL_Renderer* r, float cx, float cy, float radius, SDL_Color color) {
        constexpr int kSegments = 64;
        SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
        constexpr float kTwoPi = 6.2831853071795864769f;
        for (int pass = 0; pass < 2; ++pass) {
            const float rr = radius - static_cast<float>(pass);
            float px = cx + rr;
            float py = cy;
            for (int i = 1; i <= kSegments; ++i) {
                const float a = (static_cast<float>(i) / static_cast<float>(kSegments)) * kTwoPi;
                const float x = cx + std::cos(a) * rr;
                const float y = cy + std::sin(a) * rr;
                SDL_RenderLine(r, px, py, x, y);
                px = x;
                py = y;
            }
        }
    }
    static void render_texture_centered_(SDL_Renderer* r, SDL_Texture* tex, float cx, float cy) {
        if (!tex) return;
        float tw = 0.0f, th = 0.0f;
        SDL_GetTextureSize(tex, &tw, &th);
        SDL_FRect dst{ cx - tw * 0.5f, cy - th * 0.5f, tw, th };
        SDL_RenderTexture(r, tex, nullptr, &dst);
    }
    static void render_texture_left_center_(SDL_Renderer* r, SDL_Texture* tex, float left, float cy) {
        if (!tex) return;
        float tw = 0.0f, th = 0.0f;
        SDL_GetTextureSize(tex, &tw, &th);
        SDL_FRect dst{ left, cy - th * 0.5f, tw, th };
        SDL_RenderTexture(r, tex, nullptr, &dst);
    }
    static void render_texture_scaled_(SDL_Renderer* r, SDL_Texture* tex, float x, float y, float w, float h) {
        if (!tex) return;
        SDL_FRect dst{ x, y, w, h };
        SDL_RenderTexture(r, tex, nullptr, &dst);
    }
    static void render_texture_scaled_centered_(SDL_Renderer* r, SDL_Texture* tex, float cx, float cy, float w, float h) {
        if (!tex) return;
        SDL_FRect dst{ cx - w * 0.5f, cy - h * 0.5f, w, h };
        SDL_RenderTexture(r, tex, nullptr, &dst);
    }

    std::string font_path_;
    SDL_Texture* sprite_tex_ = nullptr;
    SDL_Texture* title_tex_ = nullptr;
    SDL_Texture* select_tex_ = nullptr;
    SDL_Texture* confirm_tex_ = nullptr;
    SDL_Texture* return_tex_ = nullptr;
    SDL_Texture* lock_tex_ = nullptr;
    SDL_Texture* check_tex_ = nullptr;
    SDL_Texture* key_left_tex_ = nullptr;
    SDL_Texture* key_right_tex_ = nullptr;
    SDL_Texture* key_space_tex_ = nullptr;
    SDL_Texture* key_escape_tex_ = nullptr;
    SDL_Texture* stage_num_tex_[kStageCount]{};
    bool cleared_[kStageCount]{};
    int selected_stage_ = 1;
};

// シーンスイッチ
class SceneSwitch final : public NeneSwitch {
public:
    explicit SceneSwitch(std::string name) : NeneSwitch(std::move(name)) {}
protected:
    void init_node() override {
        register_node("splash_scene", [] {
            return std::make_unique<SplashScene>("splash_scene");
        });
        register_node("title_scene", [] {
            return std::make_unique<TitleScene>("title_scene");
        });
        register_node("settings_scene", [] {
            return std::make_unique<SettingsScene>("settings_scene");
        });
        register_node("stage_select_scene", [] {
            return std::make_unique<StageSelectScene>("stage_select_scene");
        });
        for (int stage = 1; stage <= kStageCount; ++stage) {
            const std::string scene_name = stage_scene_name(stage);
            register_node(scene_name, [scene_name, stage] {
                return std::make_unique<PlayScene>(scene_name, stage);
            });
        }
        set_initial_node("splash_scene");
    }
};


// ルートノード
class Game final : public NeneRoot {
public:
    Game()
    : NeneRoot(
        "game",
        "SuperChromeDino",
        960, 540,
        SDL_WINDOW_RESIZABLE,
        100, 100,
        icon_path().c_str()
      )
    {
        configure_save_service("NeneEngineDemo", "SuperChromeDino");
    }
protected:
    void init_node() override {
        if (blackboard) {
            apply_game_settings(*blackboard);
            if (sound_loader) apply_sound_settings(*sound_loader, *blackboard);
        }
        // シーンスイッチを生成.
        add_child(std::make_unique<SceneSwitch>("scene_switch"));
    }
private:
    static const std::string& icon_path() {
        static std::string p = PathService::resolve_base("assets/T-Rex.svg");
        return p;
    }
};
std::unique_ptr<NeneRoot> create_game() {
    return std::make_unique<Game>();
}



