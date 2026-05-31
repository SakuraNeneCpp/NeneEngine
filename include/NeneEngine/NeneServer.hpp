#pragma once
#include <deque>
#include <memory>
#include <optional>
#include <string_view>
#include <string>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>
#include <exception>
#include <vector>
#include <cmath>
#include <cstdint>
#include <limits>
#include <functional>
#include <utility>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

struct MIX_Mixer;
struct MIX_Audio;
struct MIX_Track;

// NeneColorPolygon
// 凸多角形ヒットボックス
enum class NenePolygonColor : std::uint8_t {
    None = 0,
    Red,
    Blue,
    Green,
    Yellow,
    Purple,
    Cyan,
    White,
    Black,
};

static inline SDL_FColor nene_to_fcolor(NenePolygonColor c, float alpha) {
    switch (c) {
        case NenePolygonColor::Red:    return SDL_FColor{1.0f, 0.1f, 0.1f, alpha};
        case NenePolygonColor::Blue:   return SDL_FColor{0.1f, 0.4f, 1.0f, alpha};
        case NenePolygonColor::Green:  return SDL_FColor{0.1f, 1.0f, 0.2f, alpha};
        case NenePolygonColor::Yellow: return SDL_FColor{1.0f, 1.0f, 0.2f, alpha};
        case NenePolygonColor::Purple: return SDL_FColor{0.7f, 0.2f, 1.0f, alpha};
        case NenePolygonColor::Cyan:   return SDL_FColor{0.2f, 1.0f, 1.0f, alpha};
        case NenePolygonColor::White:  return SDL_FColor{1.0f, 1.0f, 1.0f, alpha};
        case NenePolygonColor::Black:  return SDL_FColor{0.0f, 0.0f, 0.0f, alpha};
        default:                       return SDL_FColor{1.0f, 1.0f, 1.0f, alpha};
    }
}

class NeneColorPolygon {
public:
    using ColliderId = std::uint32_t;

public:
    ColliderId id = 0;                    // world で採番
    std::string owner_name;               // たいてい管理しているノードの名前
    std::vector<SDL_FPoint> vertices;     // ローカル座標の頂点（凸を仮定）
    SDL_FPoint position{0.0f, 0.0f};      // ワールド座標の平行移動
    bool enabled = true;
    // 属性：色（＝接触時にダメージがあるかなどの属性に使うタグ）
    NenePolygonColor color = NenePolygonColor::None;
    // 将来フィルタしたくなったら使う
    std::uint32_t layer = 1;
    std::uint32_t mask  = 0xFFFFFFFFu;
    // コライダー可視化のスイッチ
    bool  debug_draw = false;     // true の時だけ描画
    float debug_alpha = 0.25f;    // 塗りの透明度（0..1）

public:
    // ワールド頂点を out に返す（local + position）
    void compute_world_vertices(std::vector<SDL_FPoint>& out) const {
        out.clear();
        out.reserve(vertices.size());
        for (const auto& v : vertices) {
            out.push_back(SDL_FPoint{ v.x + position.x, v.y + position.y });
        }
    }
    void debug_render_filled(SDL_Renderer* r) const {
        if (!r) return;
        if (!enabled) return;
        if (!debug_draw) return;
        const std::size_t n = vertices.size();
        if (n < 3) return;
        // ワールド座標へ
        std::vector<SDL_Vertex> vtx;
        vtx.resize(n);
        const SDL_FColor col = nene_to_fcolor(color, debug_alpha);
        for (std::size_t i = 0; i < n; ++i) {
            const float wx = vertices[i].x + position.x;
            const float wy = vertices[i].y + position.y;
            vtx[i].position = SDL_FPoint{ wx, wy };
            vtx[i].color    = col;
            vtx[i].tex_coord = SDL_FPoint{ 0.0f, 0.0f }; // texture=nullptr なので未使用
        }
        // 三角形ファン: (0, i, i+1)
        std::vector<int> idx;
        idx.reserve(static_cast<std::size_t>((n - 2) * 3));
        for (std::size_t i = 1; i + 1 < n; ++i) {
            idx.push_back(0);
            idx.push_back(static_cast<int>(i));
            idx.push_back(static_cast<int>(i + 1));
        }
        // ブレンド（透明描画）
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_RenderGeometry(r, nullptr, vtx.data(), static_cast<int>(vtx.size()),
                           idx.data(), static_cast<int>(idx.size()));
    }
};


// NeneCollisionWorld
// 衝突判定サービス(SAT方式)
class NeneCollisionWorld {
public:
    using ColliderId = NeneColorPolygon::ColliderId;
    using HitRef     = std::reference_wrapper<NeneColorPolygon>;
    using ConstHitRef= std::reference_wrapper<const NeneColorPolygon>;
    ColliderId add_collider(NeneColorPolygon collider) {
        collider.id = next_id_++;
        colliders_.push_back(std::move(collider));
        return colliders_.back().id;
    }
    bool remove_collider(ColliderId id) {
        for (std::size_t i = 0; i < colliders_.size(); ++i) {
            if (colliders_[i].id == id) {
                colliders_.erase(colliders_.begin() + static_cast<std::ptrdiff_t>(i));
                return true;
            }
        }
        return false;
    }
    void clear() {
        colliders_.clear();
        next_id_ = 1;
    }
    NeneColorPolygon* find(ColliderId id) {
        for (auto& c : colliders_) if (c.id == id) return &c;
        return nullptr;
    }
    const NeneColorPolygon* find(ColliderId id) const {
        for (const auto& c : colliders_) if (c.id == id) return &c;
        return nullptr;
    }
    bool set_position(ColliderId id, SDL_FPoint pos) {
        auto* c = find(id);
        if (!c) return false;
        c->position = pos;
        return true;
    }
    bool set_enabled(ColliderId id, bool v) {
        auto* c = find(id);
        if (!c) return false;
        c->enabled = v;
        return true;
    }
    // 1つでも当たれば「最初に見つかった相手」を返す
    // 当たらなければ std::nullopt
    std::optional<HitRef> detect_collision(NeneColorPolygon& target) {
        if (!target.enabled) return std::nullopt;
        // ターゲットのワールド頂点を準備
        target.compute_world_vertices(tmpA_);
        // 頂点が少なすぎるものは無視
        if (tmpA_.size() < 3) return std::nullopt;
        // 早期：ターゲットAABB（任意だけど軽くなる）
        const SDL_FRect aabbA = compute_aabb_(tmpA_);
        for (auto& other : colliders_) {
            if (!other.enabled) continue;
            if (other.id == target.id && target.id != 0) continue;
            // layer/mask フィルタ（不要なら削ってOK）
            if ((target.mask & other.layer) == 0) continue;
            if ((other.mask  & target.layer) == 0) continue;
            other.compute_world_vertices(tmpB_);
            if (tmpB_.size() < 3) continue;
            const SDL_FRect aabbB = compute_aabb_(tmpB_);
            if (!aabb_intersects_(aabbA, aabbB)) continue;
            if (sat_intersects_convex_(tmpA_, tmpB_)) {
                return HitRef{other};
            }
        }
        return std::nullopt;
    }
    std::optional<ConstHitRef> detect_collision(const NeneColorPolygon& target) const {
        if (!target.enabled) return std::nullopt;
        target.compute_world_vertices(tmpA_const_);
        if (tmpA_const_.size() < 3) return std::nullopt;
        const SDL_FRect aabbA = compute_aabb_(tmpA_const_);
        for (const auto& other : colliders_) {
            if (!other.enabled) continue;
            if (other.id == target.id && target.id != 0) continue;
            if ((target.mask & other.layer) == 0) continue;
            if ((other.mask  & target.layer) == 0) continue;
            other.compute_world_vertices(tmpB_const_);
            if (tmpB_const_.size() < 3) continue;
            const SDL_FRect aabbB = compute_aabb_(tmpB_const_);
            if (!aabb_intersects_(aabbA, aabbB)) continue;
            if (sat_intersects_convex_(tmpA_const_, tmpB_const_)) {
                return ConstHitRef{other};
            }
        }
        return std::nullopt;
    }
    const std::vector<NeneColorPolygon>& colliders() const { return colliders_; }
private:
    // geometry helpers
    static SDL_FRect compute_aabb_(const std::vector<SDL_FPoint>& v) {
        float minx =  std::numeric_limits<float>::infinity();
        float miny =  std::numeric_limits<float>::infinity();
        float maxx = -std::numeric_limits<float>::infinity();
        float maxy = -std::numeric_limits<float>::infinity();
        for (const auto& p : v) {
            if (p.x < minx) minx = p.x;
            if (p.y < miny) miny = p.y;
            if (p.x > maxx) maxx = p.x;
            if (p.y > maxy) maxy = p.y;
        }
        return SDL_FRect{ minx, miny, (maxx - minx), (maxy - miny) };
    }
    static bool aabb_intersects_(const SDL_FRect& a, const SDL_FRect& b) {
        const float ax0 = a.x, ay0 = a.y, ax1 = a.x + a.w, ay1 = a.y + a.h;
        const float bx0 = b.x, by0 = b.y, bx1 = b.x + b.w, by1 = b.y + b.h;
        if (ax1 <= bx0) return false;
        if (bx1 <= ax0) return false;
        if (ay1 <= by0) return false;
        if (by1 <= ay0) return false;
        return true;
    }
    static float dot_(const SDL_FPoint& a, const SDL_FPoint& b) {
        return a.x * b.x + a.y * b.y;
    }
    static SDL_FPoint sub_(const SDL_FPoint& a, const SDL_FPoint& b) {
        return SDL_FPoint{ a.x - b.x, a.y - b.y };
    }
    // 軸(axis)に射影した min/max を返す
    static void project_(const std::vector<SDL_FPoint>& poly, const SDL_FPoint& axis, float& outMin, float& outMax) {
        float mn = dot_(poly[0], axis);
        float mx = mn;
        for (std::size_t i = 1; i < poly.size(); ++i) {
            const float d = dot_(poly[i], axis);
            if (d < mn) mn = d;
            if (d > mx) mx = d;
        }
        outMin = mn;
        outMax = mx;
    }
    static bool overlap_1d_(float minA, float maxA, float minB, float maxB) {
        // 接触も「当たり」に含めるなら <= を < にする
        if (maxA <= minB) return false;
        if (maxB <= minA) return false;
        return true;
    }
    // SAT: 凸多角形同士の交差判定
    static bool sat_intersects_convex_(const std::vector<SDL_FPoint>& A, const std::vector<SDL_FPoint>& B) {
        // A のエッジ法線を軸に
        if (!sat_check_axes_(A, B)) return false;
        // B のエッジ法線を軸に
        if (!sat_check_axes_(B, A)) return false;
        return true;
    }
    static bool sat_check_axes_(const std::vector<SDL_FPoint>& P, const std::vector<SDL_FPoint>& Q) {
        const std::size_t n = P.size();
        for (std::size_t i = 0; i < n; ++i) {
            const SDL_FPoint p0 = P[i];
            const SDL_FPoint p1 = P[(i + 1) % n];
            const SDL_FPoint e  = sub_(p1, p0);
            // 法線
            const SDL_FPoint axis{ -e.y, e.x };
            float minP, maxP, minQ, maxQ;
            project_(P, axis, minP, maxP);
            project_(Q, axis, minQ, maxQ);
            if (!overlap_1d_(minP, maxP, minQ, maxQ)) {
                return false; // 分離軸あり
            }
        }
        return true;
    }
private:
    std::vector<NeneColorPolygon> colliders_;
    ColliderId next_id_ = 1;
    // 一時バッファ（毎回確保しない）
    std::vector<SDL_FPoint> tmpA_;
    std::vector<SDL_FPoint> tmpB_;
    // const版用（mutable にしても良いけど、ここでは分ける）
    mutable std::vector<SDL_FPoint> tmpA_const_;
    mutable std::vector<SDL_FPoint> tmpB_const_;
};

enum class PlayMode : std::uint8_t {
    Debug,
    Release
};

// NeneInput
enum class NeneInputPhase : std::uint8_t {
    Pressed,
    Released,
    Repeated,
    Changed
};

enum class NeneInputDevice : std::uint8_t {
    Unknown,
    Keyboard,
    Mouse,
    Gamepad
};

enum class NeneInputControl : std::uint8_t {
    Button,
    Axis
};

class NeneInput {
public:
    // 送り主: 通訳ノード名など
    std::string from;
    // 意味論的な入力名: "jump", "decide", "left" など
    std::string action;
    NeneInputPhase phase = NeneInputPhase::Pressed;
    NeneInputDevice device = NeneInputDevice::Unknown;
    float value = 1.0f;
    int player = 0;

    NeneInput() = default;

    NeneInput(std::string from_, std::string action_, NeneInputPhase phase_,
              NeneInputDevice device_ = NeneInputDevice::Unknown,
              float value_ = 1.0f, int player_ = 0)
        : from(std::move(from_)), action(std::move(action_)),
          phase(phase_), device(device_), value(value_), player(player_) {}
};

class NeneInputBinding {
public:
    NeneInputDevice device = NeneInputDevice::Keyboard;
    NeneInputControl control = NeneInputControl::Button;
    int code = 0;
    std::string action;
    // Buttonでは押下時の値、Axisでは方向を表す符号付き倍率として使う
    float scale = 1.0f;
    int player = 0;
    float dead_zone = 0.35f;

    NeneInputBinding() = default;

    NeneInputBinding(NeneInputDevice device_, NeneInputControl control_,
                     int code_, std::string action_, float scale_ = 1.0f,
                     int player_ = 0, float dead_zone_ = 0.35f)
        : device(device_), control(control_), code(code_),
          action(std::move(action_)), scale(scale_),
          player(player_), dead_zone(dead_zone_) {}
};

// NeneInputServer
// 意味論的な入力イベントのキューと、現在フレームの入力状態を持つ
class NeneInputServer {
public:
    void begin_frame() {
        pressed_.clear();
        released_.clear();
        changed_.clear();
    }

    void push(const NeneInput& input) { input_queue_.push_back(input); }
    void push(NeneInput&& input) { input_queue_.push_back(std::move(input)); }

    bool pull(NeneInput& out) {
        if (input_queue_.empty()) return false;
        out = std::move(input_queue_.front());
        input_queue_.pop_front();
        apply_state_(out);
        return true;
    }

    bool empty() const { return input_queue_.empty(); }
    std::size_t size() const { return input_queue_.size(); }

    bool is_down(std::string_view action, int player = 0) const {
        const auto it = states_.find(key_(action, player));
        return it != states_.end() && it->second.down;
    }

    bool was_pressed(std::string_view action, int player = 0) const {
        return pressed_.find(key_(action, player)) != pressed_.end();
    }

    bool was_released(std::string_view action, int player = 0) const {
        return released_.find(key_(action, player)) != released_.end();
    }

    bool was_changed(std::string_view action, int player = 0) const {
        return changed_.find(key_(action, player)) != changed_.end();
    }

    float value(std::string_view action, int player = 0) const {
        const auto it = states_.find(key_(action, player));
        return (it == states_.end()) ? 0.0f : it->second.value;
    }

private:
    class State {
    public:
        bool down = false;
        float value = 0.0f;
    };

    static std::string key_(std::string_view action, int player) {
        return std::to_string(player) + ":" + std::string(action);
    }

    void apply_state_(const NeneInput& input) {
        if (input.action.empty()) return;
        const std::string key = key_(input.action, input.player);
        auto& state = states_[key];

        switch (input.phase) {
            case NeneInputPhase::Pressed:
                state.down = true;
                state.value = input.value;
                pressed_[key] = true;
                changed_[key] = true;
                break;
            case NeneInputPhase::Released:
                state.down = false;
                state.value = 0.0f;
                released_[key] = true;
                changed_[key] = true;
                break;
            case NeneInputPhase::Repeated:
                state.down = true;
                state.value = input.value;
                break;
            case NeneInputPhase::Changed:
                state.value = input.value;
                state.down = (std::fabs(input.value) > 0.0001f);
                changed_[key] = true;
                break;
        }
    }

private:
    std::deque<NeneInput> input_queue_;
    std::unordered_map<std::string, State> states_;
    std::unordered_map<std::string, bool> pressed_;
    std::unordered_map<std::string, bool> released_;
    std::unordered_map<std::string, bool> changed_;
};

class NeneSaveDocument;

// ノード間データ共有サービス
class NeneBlackboard {
public:
    // --- デフォルト項目 ---
    // プレイモード(デバック/本番)
    PlayMode play_mode = PlayMode::Debug;
    // FPS
    int fps = 60;
    // ルートノードの名前
    std::string root_name;
    // ウィンドウ（論理）設定
    int window_x = 100;
    int window_y = 100;
    int window_w = 960;
    int window_h = 540;
    // ゲーム共通値 (ユーザーが上書きする前提だけど適当に初期値を入れとく)
    float ground_y = window_h - 120.0f;   // 地面の高さ（ピクセル）
    float gravity  = 2400.0f;             // 重力（px/s^2）
    float scroll_speed = 420.0f;          // 横スクロール速度（px/s）
    // キーコンフィグ
    std::unordered_map<std::string, std::vector<NeneInputBinding>> input_maps;
    void bind_input(std::string map_name, NeneInputBinding binding) {
        input_maps[std::move(map_name)].push_back(std::move(binding));
    }
    bool has_input_map(std::string_view map_name) const {
        return input_maps.find(std::string(map_name)) != input_maps.end();
    }
    bool set_default_input_map(std::string map_name, std::vector<NeneInputBinding> bindings) {
        if (input_maps.find(map_name) != input_maps.end()) return false;
        input_maps.emplace(std::move(map_name), std::move(bindings));
        return true;
    }
    void bind_key(std::string map_name, SDL_Keycode key, std::string action, int player = 0) {
        bind_input(std::move(map_name),
                   NeneInputBinding(NeneInputDevice::Keyboard, NeneInputControl::Button,
                                    static_cast<int>(key), std::move(action), 1.0f, player));
    }
    void bind_gamepad_button(std::string map_name, SDL_GamepadButton button,
                             std::string action, int player = 0) {
        bind_input(std::move(map_name),
                   NeneInputBinding(NeneInputDevice::Gamepad, NeneInputControl::Button,
                                    static_cast<int>(button), std::move(action), 1.0f, player));
    }
    void bind_gamepad_axis(std::string map_name, SDL_GamepadAxis axis,
                           std::string action, float scale = 1.0f,
                           int player = 0, float dead_zone = 0.35f) {
        bind_input(std::move(map_name),
                   NeneInputBinding(NeneInputDevice::Gamepad, NeneInputControl::Axis,
                                    static_cast<int>(axis), std::move(action),
                                    scale, player, dead_zone));
    }
    void clear_input_map(const std::string& map_name) {
        input_maps.erase(map_name);
    }
    // --- ユーザー拡張（float）---
    std::unordered_map<std::string, float> user_floats;
    // API
    void setf(const std::string& key, float v) { user_floats[key] = v; }
    void set_persistentf(const std::string& key, float v) {
        setf(key, v);
        mark_persistentf(key);
    }
    // 無い値をgetしようとするとデフォルトが返る
    float getf(const std::string& key, float default_value = 0.0f) const {
        auto it = user_floats.find(key);
        return (it == user_floats.end()) ? default_value : it->second;
    }
    bool hasf(const std::string& key) const {
        return user_floats.find(key) != user_floats.end();
    }
    // 無ければ作って返す（初期値 default_value）

    float& ensuref(const std::string& key, float default_value = 0.0f) {
        auto [it, inserted] = user_floats.emplace(key, default_value);
        return it->second;
    }
    float& ensure_persistentf(const std::string& key, float default_value = 0.0f) {
        mark_persistentf(key);
        return ensuref(key, default_value);
    }
    void mark_persistentf(const std::string& key) {
        if (!key.empty()) persistent_float_keys_.insert(key);
    }
    void unmark_persistentf(const std::string& key) {
        persistent_float_keys_.erase(key);
    }
    bool is_persistentf(std::string_view key) const {
        return persistent_float_keys_.find(std::string(key)) != persistent_float_keys_.end();
    }
    void clear_persistentf() {
        persistent_float_keys_.clear();
    }
    const std::unordered_set<std::string>& persistent_float_keys() const {
        return persistent_float_keys_;
    }
    void save_settings(NeneSaveDocument& doc) const;
    void load_settings(const NeneSaveDocument& doc);
private:
    std::unordered_set<std::string> persistent_float_keys_;
};



// PathService
// パス解決サービス
class PathService {
public:
    explicit PathService(std::string assets_dir = "assets/")
        : assets_dir_(std::move(assets_dir)) {
        // SDL3: SDL_GetBasePath() は const char* を返し、SDL が内部でキャッシュする
        // 戻り値は SDL_free() しない
        const char* base = SDL_GetBasePath();
        base_path_ = base ? base : "";

    }
    // exe のベースパスからの相対を解決（"assets/..." を渡す想定）
    std::string resolve(std::string_view rel_from_base) const {
        if (base_path_.empty()) return std::string(rel_from_base);
        return base_path_ + std::string(rel_from_base);
    }
    // assets ルートからの相対を解決（"sprites/..." を渡す想定）
    std::string asset(std::string_view rel_from_assets) const {
        return resolve(assets_dir_ + std::string(rel_from_assets));
    }
    const std::string& base_path() const { return base_path_; }
    // 互換用: 「ベースパス + 相対」をその場で作る（必要なら使う）
    // ※ Game の ctor 引数など、PathService インスタンスがまだ無い場面用
    static std::string resolve_base(std::string_view rel_from_base) {
        const char* base = SDL_GetBasePath();
        if (!base) return std::string(rel_from_base);
        return std::string(base) + std::string(rel_from_base);
    }
private:
    std::string base_path_;
    std::string assets_dir_;
};

// NeneSave
// ノード部分木の「再構築可能な状態」だけを保存するための軽量データ形式。
class NeneSaveNodeRecord {
public:
    std::string type;
    std::map<std::string, std::string> values;
};

class NeneSaveWriter {
public:
    explicit NeneSaveWriter(NeneSaveNodeRecord& record);
    void set(std::string key, std::string value);
    void set(std::string key, const char* value);
    void set_bool(std::string key, bool value);
    void set_int(std::string key, int value);
    void set_uint(std::string key, std::uint32_t value);
    void set_float(std::string key, float value);
    void set_double(std::string key, double value);
private:
    NeneSaveNodeRecord* record_;
};

class NeneSaveReader {
public:
    explicit NeneSaveReader(const NeneSaveNodeRecord& record);
    bool has(std::string_view key) const;
    std::string get_string(std::string_view key, std::string default_value = "") const;
    bool get_bool(std::string_view key, bool default_value = false) const;
    int get_int(std::string_view key, int default_value = 0) const;
    std::uint32_t get_uint(std::string_view key, std::uint32_t default_value = 0) const;
    float get_float(std::string_view key, float default_value = 0.0f) const;
    double get_double(std::string_view key, double default_value = 0.0) const;
    const NeneSaveNodeRecord& record() const { return *record_; }
private:
    const NeneSaveNodeRecord* record_;
};

class NeneSaveDocument {
public:
    static constexpr std::uint32_t current_format_version = 1;
    std::uint32_t format_version = current_format_version;
    std::map<std::string, std::string> metadata;
    std::map<std::string, NeneSaveNodeRecord> nodes;

    void clear();
    void set_metadata(std::string key, std::string value);
    bool has_metadata(std::string_view key) const;
    std::string get_metadata(std::string_view key, std::string default_value = "") const;
    NeneSaveNodeRecord& node(std::string path);
    const NeneSaveNodeRecord* find_node(std::string_view path) const;
    bool has_node(std::string_view path) const;
    std::string serialize() const;
    static NeneSaveDocument parse(std::string_view text);
};

class NeneSaveService {
public:
    NeneSaveService(std::string org, std::string app,
                    std::string save_dir = "saves",
                    std::string extension = ".nnsave");
    const std::string& base_path() const { return base_path_; }
    const std::string& save_dir_path() const { return save_dir_path_; }
    const std::string& extension() const { return extension_; }
    std::string slot_path(std::string_view slot_name) const;
    bool slot_exists(std::string_view slot_name) const;
    void save(std::string_view slot_name, const NeneSaveDocument& doc) const;
    NeneSaveDocument load(std::string_view slot_name) const;
    bool remove(std::string_view slot_name) const;
    std::vector<std::string> list_slots() const;
private:
    std::string base_path_;
    std::string save_dir_path_;
    std::string extension_;
    static std::string checked_slot_name_(std::string_view slot_name);
};


// NeneMail
class NeneMail {
public:
    // 宛先: nullopt ならブロードキャスト
    std::optional<std::string> to;
    // 送り主: ノード名
    std::string from;
    // 件名
    std::string subject;
    // 本文
    std::string body;
    NeneMail() = default;
    // broadcast
    NeneMail(std::string from_, std::string subject_, std::string body_)
        : to(std::nullopt), from(std::move(from_)),
          subject(std::move(subject_)), body(std::move(body_)) {}
    // directed
    NeneMail(std::string to_, std::string from_, std::string subject_, std::string body_)
        : to(std::move(to_)), from(std::move(from_)),
          subject(std::move(subject_)), body(std::move(body_)) {}
    bool is_broadcast() const { return !to.has_value(); }
};


// NeneMailServer
// ノード間通信(内部イベント伝播)サービス
class NeneMailServer {
public:
    void push(const NeneMail& mail) { mail_queue_.push_back(mail); }
    void push(NeneMail&& mail) { mail_queue_.push_back(std::move(mail)); }
    // キュー先頭を取り出す。空なら false。
    bool pull(NeneMail& out) {
        if (mail_queue_.empty()) return false;
        out = std::move(mail_queue_.front());
        mail_queue_.pop_front();
        return true;
    }
    bool empty() const { return mail_queue_.empty(); }
    std::size_t size() const { return mail_queue_.size(); }
private:
    std::deque<NeneMail> mail_queue_;
};

// NeneTask
// メインループを止めたくない重い処理をワーカースレッドへ逃がし、
// 結果の反映だけをメインスレッドで少しずつ実行する。
enum class NeneTaskStatus : std::uint8_t {
    Queued,
    Running,
    WaitingCommit,
    Succeeded,
    Failed,
    Canceled,
};

const char* nene_task_status_name(NeneTaskStatus status);

class NeneTaskCanceled : public std::exception {
public:
    const char* what() const noexcept override { return "NeneTask canceled"; }
};

class NeneTaskState;

class NeneTaskContext {
public:
    bool stop_requested() const;
    void throw_if_stop_requested() const;
    void set_progress(float progress, std::string message = {});
private:
    friend class NeneTaskServer;
    NeneTaskContext(std::stop_token stop_token, std::shared_ptr<NeneTaskState> state);

    std::stop_token stop_token_;
    std::weak_ptr<NeneTaskState> state_;
};

class NeneTaskHandle {
public:
    NeneTaskHandle() = default;

    bool valid() const { return state_ != nullptr; }
    std::uint64_t id() const;
    std::string name() const;
    NeneTaskStatus status() const;
    float progress() const;
    std::string message() const;
    std::string error() const;
    bool stop_requested() const;
    bool done() const;
    bool succeeded() const;
    bool failed() const;
    bool canceled() const;
    void cancel() const;
private:
    friend class NeneTaskServer;
    explicit NeneTaskHandle(std::shared_ptr<NeneTaskState> state);

    std::shared_ptr<NeneTaskState> state_;
};

class NeneTaskServer {
public:
    using Worker = std::function<void(NeneTaskContext&)>;
    using MainThreadCommit = std::function<void()>;

    explicit NeneTaskServer(std::size_t worker_count = 0);
    ~NeneTaskServer();
    NeneTaskServer(const NeneTaskServer&) = delete;
    NeneTaskServer& operator=(const NeneTaskServer&) = delete;

    NeneTaskHandle submit(std::string name, Worker worker,
                          MainThreadCommit commit = {});

    template <class Result, class WorkerFn, class CommitFn>
    NeneTaskHandle submit_result(std::string name, WorkerFn&& worker,
                                 CommitFn&& commit) {
        auto result = std::make_shared<std::optional<Result>>();
        return submit(
            std::move(name),
            [result, worker_fn = std::forward<WorkerFn>(worker)](NeneTaskContext& ctx) mutable {
                result->emplace(worker_fn(ctx));
            },
            [result, commit_fn = std::forward<CommitFn>(commit)]() mutable {
                if (!result->has_value()) {
                    throw std::runtime_error("NeneTaskServer: result is empty");
                }
                commit_fn(std::move(result->value()));
                result->reset();
            });
    }

    std::size_t pump_main_thread(double budget_ms = 2.0);
    void request_stop_all();
    void wait_worker_idle();
    std::size_t queued_count() const;
    std::size_t running_count() const;
    std::size_t ready_count() const;
    std::size_t unfinished_count() const;
    bool has_unfinished_tasks() const { return unfinished_count() > 0; }
private:
    class Job {
    public:
        std::shared_ptr<NeneTaskState> state;
        Worker worker;
        MainThreadCommit commit;
    };

    static std::size_t default_worker_count_();
    void worker_loop_(std::stop_token stop_token);
    void mark_canceled_(const std::shared_ptr<NeneTaskState>& state);
    void mark_failed_(const std::shared_ptr<NeneTaskState>& state, std::string error);
    void mark_succeeded_(const std::shared_ptr<NeneTaskState>& state);
    void mark_waiting_commit_(const std::shared_ptr<NeneTaskState>& state);
    bool should_cancel_(const std::shared_ptr<NeneTaskState>& state) const;

    std::uint64_t next_id_ = 1;
    bool stopping_ = false;
    std::size_t running_count_ = 0;
    std::deque<Job> queued_;
    std::deque<Job> ready_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable finished_cv_;
    std::vector<std::jthread> workers_;
};

class NeneTaskGroup {
public:
    ~NeneTaskGroup();
    void add(NeneTaskHandle handle);
    void clear();
    void cancel_all();
    bool empty() const { return handles_.empty(); }
    bool all_done() const;
    bool any_failed() const;
    bool all_succeeded() const;
    float progress() const;
    std::string first_error() const;
    std::size_t size() const { return handles_.size(); }
private:
    std::vector<NeneTaskHandle> handles_;
};


// NeneImageLoader
class NeneImageLoader {
public:
    explicit NeneImageLoader(SDL_Renderer* renderer)
        : renderer_(renderer) {
        if (!renderer_) {
            throw std::runtime_error("NeneImageLoader: renderer is null");
        }
        // SDL3_image では IMG_Init / IMG_Quit は不要
    }
    ~NeneImageLoader() {
        for (auto& [path, tex] : cache_) {
            if (tex) SDL_DestroyTexture(tex);
        }
        cache_.clear();
        // SDL3_image では IMG_Quit も不要
    }
    SDL_Texture* get_texture(const std::string& path) {
        auto it = cache_.find(path);
        if (it != cache_.end()) return it->second;
        SDL_Texture* tex = IMG_LoadTexture(renderer_, path.c_str());
        if (!tex) {
            throw std::runtime_error(std::string("[NeneImageLoader] IMG_LoadTexture failed '")
                                     + path + "': " + SDL_GetError());
        }
        cache_.emplace(path, tex);
        return tex;
    }

private:
    SDL_Renderer* renderer_;
    std::unordered_map<std::string, SDL_Texture*> cache_;
};


// NeneFontLoader
struct FontKey {
    std::string text;
    int fontSize;
    SDL_Color color;
    bool operator==(FontKey const& o) const {
        return text == o.text
            && fontSize == o.fontSize
            && color.r == o.color.r
            && color.g == o.color.g
            && color.b == o.color.b
            && color.a == o.color.a;
    }
};

struct FontKeyHash {
    std::size_t operator()(FontKey const& k) const {
        std::size_t h1 = std::hash<std::string>()(k.text);
        std::size_t h2 = std::hash<int>()(k.fontSize);
        std::size_t h3 = (static_cast<std::size_t>(k.color.r) << 24)
                       | (static_cast<std::size_t>(k.color.g) << 16)
                       | (static_cast<std::size_t>(k.color.b) << 8)
                       |  static_cast<std::size_t>(k.color.a);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

class NeneFontLoader {
public:
    explicit NeneFontLoader(SDL_Renderer* renderer);
    ~NeneFontLoader();
    TTF_Font* get_font(const std::string& fontPath, int fontSize);
    SDL_Texture* get_text_texture(const std::string& fontPath, int fontSize,
                                  const std::string& text, SDL_Color color);
private:
    SDL_Renderer* renderer_;
    std::unordered_map<std::string, TTF_Font*> fontCache_;
    std::unordered_map<FontKey, SDL_Texture*, FontKeyHash> textCache_;
};


// NeneSoundLoader
class NeneSoundLoader {
public:
    explicit NeneSoundLoader(int se_track_count = 16);
    ~NeneSoundLoader();
    NeneSoundLoader(const NeneSoundLoader&) = delete;
    NeneSoundLoader& operator=(const NeneSoundLoader&) = delete;
    void preload(const std::string& path);
    void play_se(const std::string& path, float gain = 1.0f);
    void play_bgm(const std::string& path, float gain = 1.0f);
    void stop_bgm();
    void set_se_volume(float volume);
    void set_bgm_volume(float volume);
    float se_volume() const { return se_volume_; }
    float bgm_volume() const { return bgm_volume_; }
private:
    MIX_Audio* get_audio_(const std::string& path);
    MIX_Audio* load_audio_(const std::string& path);
    MIX_Track* acquire_se_track_();
    static float clamp_gain_(float gain);
    MIX_Mixer* mixer_ = nullptr;
    std::unordered_map<std::string, MIX_Audio*> cache_;
    std::vector<MIX_Track*> se_tracks_;
    std::vector<float> se_track_gains_;
    std::size_t next_se_track_ = 0;
    MIX_Track* bgm_track_ = nullptr;
    std::string bgm_path_;
    float se_volume_ = 1.0f;
    float bgm_volume_ = 1.0f;
    float bgm_gain_ = 1.0f;
    mutable std::mutex cache_mutex_;
    bool mixer_initialized_ = false;
#ifdef _WIN32
    bool media_foundation_started_ = false;
    bool com_initialized_ = false;
#endif
};
