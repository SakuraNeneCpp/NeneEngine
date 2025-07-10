// 依存性グループB: NeneServer
#pragma once
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <NeneObject.hpp>

class NeneServer {
public:
    NeneServer(SDL_Renderer*);
    ~NeneServer();
    NeneViewWorld view_world;
    NeneCamera camera;
};

class NeneViewWorld {
public:
    void addObject(NeneViewObject&);
    // z軸の順
    std::map<int, NeneViewObject&> objects;
};

class NeneCamera {
public:
    NeneCamera(NeneViewWorld&, SDL_Renderer&);
    SDL_Renderer& film;
    NeneViewWorld& view;
    int x = 0;
    int y = 0;
    float scale = 1.0;
    float rotate = 0;
    // viewのオブジェクトうち, 領域が描画領域に重なっているものをfilmに登録する
    void shoot();
};

class AssetLoader {
public:
    explicit AssetLoader(SDL_Renderer* renderer)
     : renderer_(renderer) {
        if (!renderer_) {
            throw std::runtime_error("AssetLoader: renderer is null");
        }
    }
    ~AssetLoader() {
        // キャッシュ内のテクスチャをすべて破棄
        for (auto& [path, tex] : cache_) {
            if (tex) {
                SDL_DestroyTexture(tex);
            }
        }
        cache_.clear();
    }
    // 指定パスのテクスチャを返す. キャッシュにあれば再利用し,
    // なければ IMG_LoadTexture で読み込んでキャッシュに登録する.
    SDL_Texture* get_texture(const std::string& path) {
        // 1. キャッシュに存在するかチェック
        auto it = cache_.find(path);
        if (it != cache_.end()) {
            return it->second;
        }
        // 2. キャッシュにない場合はディスクからロード
        SDL_Texture* tex = IMG_LoadTexture(renderer_, path.c_str());
        if (!tex) {
            throw std::runtime_error(std::string("![AssetLoader] 画像の読み込みに失敗しました '") + path + "': " + SDL_GetError());
        }
        // 3. キャッシュに登録して返却
        cache_.emplace(path, tex);
        return tex;
    }
    // 必要に応じて、SDL_Surface* をテクスチャに変換して返すメソッドなどを追加してもよい
private:
    SDL_Renderer* renderer_;
    std::unordered_map<std::string, SDL_Texture*> cache_; // キャッシュ. パスをキーとして, 読み込み済みテクスチャを値に持つ辞書
};

struct FontKey {
    std::string text;
    int fontSize;
    SDL_Color color;
    // 等号演算子オーバーロード
    // 文字列, サイズ, 色がすべて等しいとき, これらのテキストテクスチャは等しいとする.
    bool operator==(FontKey const& o) const {
        return text == o.text
            && fontSize == o.fontSize
            && color.r == o.color.r
            && color.g == o.color.g
            && color.b == o.color.b
            && color.a == o.color.a;
    }
};

// unordered_map 用のハッシュ関数
struct FontKeyHash {
    std::size_t operator()(FontKey const& k) const {
        // シンプルに文字列ハッシュ＋サイズ＋カラー成分をミックス
        std::size_t h1 = std::hash<std::string>()(k.text);
        std::size_t h2 = std::hash<int>()(k.fontSize);
        std::size_t h3 = (k.color.r << 24) | (k.color.g << 16) | (k.color.b << 8) | k.color.a;
        return h1 ^ (h2 << 1) ^ (std::hash<int>()(static_cast<int>(h3)) << 2);
    }
};

class FontLoader {
public:
    // コンストラクタで SDL_Renderer と TTF_Init を行う
    explicit FontLoader(SDL_Renderer* renderer);
    ~FontLoader();
    // 指定パスのフォントファイルを指定サイズでロードし TTF_Font* を返す
    // (すでに同じパス・サイズでロード済みなら、キャッシュを返す)
    TTF_Font* get_font(const std::string& fontPath, int fontSize);
    // 指定文字列をテクスチャ化して返す (フォントパス＋サイズ＋文字列＋色)
    SDL_Texture* get_text_texture(const std::string& fontPath, int fontSize, const std::string& text, SDL_Color color);
private:
    SDL_Renderer* renderer_;
    // フォントキャッシュ
    std::unordered_map<std::string, TTF_Font*> fontCache_;
    // 文字列テクスチャキャッシュ
    std::unordered_map<FontKey, SDL_Texture*, FontKeyHash> textCache_;
};
