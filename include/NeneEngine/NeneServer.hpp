#pragma once
#include <deque>
#include <memory>
#include <optional>
#include <string_view>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

class NeneCollisionWorld2D_AABB_N {
public:
    using ColliderId = std::uint32_t;

    struct ColliderDesc {
        std::string owner_name;     // "dino", "cactus_12" 等
        SDL_FRect   aabb{};         // world座標のAABB
        std::uint32_t layer = 1;    // 自分の属するレイヤ
        std::uint32_t mask  = 0xFFFFFFFFu; // 衝突したい相手レイヤのビット集合
        bool enabled = true;
        bool is_trigger = true;     // 今回は検出だけなのでデフォルトtrue扱い
    };

    struct Hit {
        ColliderId other_id = 0;
        std::string other_owner;
        SDL_FRect overlap{};        // 重なり領域（欲しければ）
    };

public:
    NeneCollisionWorld2D_AABB_N() = default;

    // 追加・削除
    ColliderId create_collider(const ColliderDesc& desc) {
        const ColliderId id = next_id_++;
        Entry e;
        e.owner_name = desc.owner_name;
        e.aabb = desc.aabb;
        e.layer = desc.layer;
        e.mask  = desc.mask;
        e.enabled = desc.enabled;
        e.is_trigger = desc.is_trigger;
        colliders_.emplace(id, std::move(e));
        return id;
    }

    void destroy_collider(ColliderId id) {
        colliders_.erase(id);
        if (target_id_ == id) target_id_ = kInvalid;
    }

    // 更新
    bool set_aabb(ColliderId id, const SDL_FRect& aabb) {
        auto it = colliders_.find(id);
        if (it == colliders_.end()) return false;
        it->second.aabb = aabb;
        return true;
    }

    bool set_enabled(ColliderId id, bool v) {
        auto it = colliders_.find(id);
        if (it == colliders_.end()) return false;
        it->second.enabled = v;
        return true;
    }

    // ターゲット指定
    bool set_target(ColliderId id) {
        if (colliders_.find(id) == colliders_.end()) return false;
        target_id_ = id;
        return true;
    }

    ColliderId target() const { return target_id_; }

    // 1フレーム分：ターゲットが何かと当たっているかを検出（O(N)）
    // 返り値: 衝突相手の一覧（複数ヒットも返せる）
    const std::vector<Hit>& step() {
        hits_.clear();
        if (target_id_ == kInvalid) return hits_;
        auto itT = colliders_.find(target_id_);
        if (itT == colliders_.end()) return hits_;
        const Entry& t = itT->second;
        if (!t.enabled) return hits_;
        for (const auto& kv : colliders_) {
            const ColliderId other_id = kv.first;
            if (other_id == target_id_) continue;
            const Entry& o = kv.second;
            if (!o.enabled) continue;
            // layer/mask フィルタ（必要なければ消してOK）
            if ( (t.mask & o.layer) == 0 ) continue;
            if ( (o.mask & t.layer) == 0 ) continue;
            SDL_FRect overlap{};
            if (intersects_aabb_(t.aabb, o.aabb, overlap)) {
                Hit h;
                h.other_id = other_id;
                h.other_owner = o.owner_name;
                h.overlap = overlap;
                hits_.push_back(std::move(h));
            }
        }
        return hits_;
    }
    const std::vector<Hit>& hits() const { return hits_; }
    // デバッグ/参照用
    std::optional<std::string> owner_name(ColliderId id) const {
        auto it = colliders_.find(id);
        if (it == colliders_.end()) return std::nullopt;
        return it->second.owner_name;
    }

private:
    struct Entry {
        std::string owner_name;
        SDL_FRect aabb{};
        std::uint32_t layer = 1;
        std::uint32_t mask  = 0xFFFFFFFFu;
        bool enabled = true;
        bool is_trigger = true;
    };
    static bool intersects_aabb_(const SDL_FRect& a, const SDL_FRect& b, SDL_FRect& overlap_out) {
        // min/max
        const float ax0 = a.x;
        const float ay0 = a.y;
        const float ax1 = a.x + a.w;
        const float ay1 = a.y + a.h;
        const float bx0 = b.x;
        const float by0 = b.y;
        const float bx1 = b.x + b.w;
        const float by1 = b.y + b.h;
        // 分離判定（接触を「当たり」に含めたいなら < を <= に変える）
        if (ax1 <= bx0) return false;
        if (bx1 <= ax0) return false;
        if (ay1 <= by0) return false;
        if (by1 <= ay0) return false;
        const float ox0 = (ax0 > bx0) ? ax0 : bx0;
        const float oy0 = (ay0 > by0) ? ay0 : by0;
        const float ox1 = (ax1 < bx1) ? ax1 : bx1;
        const float oy1 = (ay1 < by1) ? ay1 : by1;
        overlap_out = SDL_FRect{ ox0, oy0, (ox1 - ox0), (oy1 - oy0) };
        return true;
    }

private:
    static constexpr ColliderId kInvalid = 0;
    std::unordered_map<ColliderId, Entry> colliders_;
    ColliderId next_id_ = 1;
    ColliderId target_id_ = kInvalid;
    std::vector<Hit> hits_;
};

class NeneGlobalSettings {
public:
    // ---- 必須項目（例）----
    std::string root_name;
    // ウィンドウ（論理）設定
    int window_x = 100;
    int window_y = 100;
    int window_w = 960;
    int window_h = 540;
    // ゲーム共通値（例）
    float ground_y = 420.0f;   // 地面の高さ（ピクセル）
    float gravity  = 2400.0f;  // 重力（px/s^2）
    float scroll_speed = 420.0f; // 横スクロール速度（px/s）
    // ---- ユーザー拡張（float）----
    std::unordered_map<std::string, float> user_floats;
    // 便利API
    void setf(const std::string& key, float v) { user_floats[key] = v; }
    // 無ければ default を返す
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


// --------------------
// PathService
//  - SDL_GetBasePath() の結果を1回だけ取得して保持
//  - SDL_GetBasePath() が返すバッファは SDL_free() で解放する（リーク防止）
// --------------------
class PathService {
public:
    explicit PathService(std::string assets_dir = "assets/")
        : assets_dir_(std::move(assets_dir)) {
        // SDL3: SDL_GetBasePath() は const char* を返し、SDL が内部でキャッシュする
        // 戻り値は SDL_free() しない :contentReference[oaicite:1]{index=1}
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


// MailServer
class MailServer {
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


// AssetLoader
class AssetLoader {
public:
    explicit AssetLoader(SDL_Renderer* renderer)
        : renderer_(renderer) {
        if (!renderer_) {
            throw std::runtime_error("AssetLoader: renderer is null");
        }
        // SDL3_image では IMG_Init / IMG_Quit は不要
    }

    ~AssetLoader() {
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
            throw std::runtime_error(std::string("[AssetLoader] IMG_LoadTexture failed '")
                                     + path + "': " + SDL_GetError());
        }

        cache_.emplace(path, tex);
        return tex;
    }

private:
    SDL_Renderer* renderer_;
    std::unordered_map<std::string, SDL_Texture*> cache_;
};


// FontLoader
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

class FontLoader {
public:
    explicit FontLoader(SDL_Renderer* renderer);
    ~FontLoader();

    TTF_Font* get_font(const std::string& fontPath, int fontSize);
    SDL_Texture* get_text_texture(const std::string& fontPath, int fontSize,
                                  const std::string& text, SDL_Color color);

private:
    SDL_Renderer* renderer_;
    std::unordered_map<std::string, TTF_Font*> fontCache_;
    std::unordered_map<FontKey, SDL_Texture*, FontKeyHash> textCache_;
};
