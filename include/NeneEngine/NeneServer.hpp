#pragma once
#include <deque>
#include <memory>
#include <optional>
#include <string_view>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <vector>
#include <cmath>
#include <cstdint>
#include <limits>
#include <functional>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

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

// ノード間データ共有サービス
class NeneBlackboard {
public:
    // --- デフォルト項目 ---
    // プレイモード(デバック/本番)
    PlayMode play_mode;
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
    // --- ユーザー拡張（float）---
    std::unordered_map<std::string, float> user_floats;
    // API
    void setf(const std::string& key, float v) { user_floats[key] = v; }
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
