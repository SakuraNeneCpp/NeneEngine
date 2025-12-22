// #include <iostream>
// #include <SDL3/SDL.h>
// #include <SDL3_image/SDL_image.h>
// #include <NeneServer.hpp>



// FontLoader::FontLoader(SDL_Renderer* renderer)
//     : renderer_(renderer) {
//     if (!TTF_Init()) {
//         throw std::runtime_error(std::string("TTF初期化失敗") + SDL_GetError());
//     }
// }

// FontLoader::~FontLoader() {
//     // フォントキャッシュをすべて解放
//     for (auto& [key, font] : fontCache_) {
//         if (font) TTF_CloseFont(font);
//     }
//     fontCache_.clear();
//     // テキストテクスチャキャッシュをすべて解放
//     for (auto& [fontKey, tex] : textCache_) {
//         if (tex) SDL_DestroyTexture(tex);
//     }
//     textCache_.clear();
//     // TTF_Quit
//     TTF_Quit();
// }

// TTF_Font* FontLoader::get_font(const std::string& fontPath, int fontSize) {
//     std::string key = fontPath + "#" + std::to_string(fontSize);
//     auto it = fontCache_.find(key);
//     if (it != fontCache_.end()) {
//         return it->second;
//     }
//     TTF_Font* font = TTF_OpenFont(fontPath.c_str(), fontSize);
//     if (!font) {
//         throw std::runtime_error(std::string("TTF_OpenFont failed: ") + SDL_GetError());
//     }
//     fontCache_[key] = font;
//     return font;
// }

// SDL_Texture* FontLoader::get_text_texture(const std::string& fontPath, int fontSize, const std::string& text, SDL_Color color) {
//     FontKey fk{ text, fontSize, color };
//     auto it = textCache_.find(fk);
//     if (it != textCache_.end()) {
//         return it->second;
//     }
//     // フォントを取得
//     TTF_Font* font = get_font(fontPath, fontSize);
//     if (!font) {
//         throw std::runtime_error("Failed to get font for text rendering");
//     }
//     // 文字列を SDL_Surface にレンダー（ブレンドあり）
//     SDL_Surface* surf = TTF_RenderText_Blended(font, text.c_str(), 0, color);
//     if (!surf) {
//         throw std::runtime_error(std::string("TTF_RenderUTF8_Blended failed: ") + SDL_GetError());
//     }
//     // Surface → Texture
//     SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
//     SDL_DestroySurface(surf);
//     if (!tex) {
//         throw std::runtime_error(std::string("CreateTextureFromSurface failed: ") + SDL_GetError());
//     }
//     // キャッシュに保存して返却
//     textCache_[fk] = tex;
//     return tex;
// }

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <NeneServer.hpp>

FontLoader::FontLoader(SDL_Renderer* renderer)
    : renderer_(renderer) {
    if (!renderer_) {
        throw std::runtime_error("FontLoader: renderer is null");
    }
    if (!TTF_Init()) {
        throw std::runtime_error(std::string("TTF_Init failed: ") + SDL_GetError());
    }
}

FontLoader::~FontLoader() {
    for (auto& [key, font] : fontCache_) {
        if (font) TTF_CloseFont(font);
    }
    fontCache_.clear();

    for (auto& [fontKey, tex] : textCache_) {
        if (tex) SDL_DestroyTexture(tex);
    }
    textCache_.clear();

    TTF_Quit();
}

TTF_Font* FontLoader::get_font(const std::string& fontPath, int fontSize) {
    std::string key = fontPath + "#" + std::to_string(fontSize);
    auto it = fontCache_.find(key);
    if (it != fontCache_.end()) return it->second;

    TTF_Font* font = TTF_OpenFont(fontPath.c_str(), fontSize);
    if (!font) {
        throw std::runtime_error(std::string("TTF_OpenFont failed: ") + SDL_GetError());
    }
    fontCache_[key] = font;
    return font;
}

SDL_Texture* FontLoader::get_text_texture(const std::string& fontPath, int fontSize,
                                         const std::string& text, SDL_Color color) {
    FontKey fk{ text, fontSize, color };
    auto it = textCache_.find(fk);
    if (it != textCache_.end()) return it->second;

    TTF_Font* font = get_font(fontPath, fontSize);

    SDL_Surface* surf = TTF_RenderText_Blended(font, text.c_str(), 0, color);
    if (!surf) {
        throw std::runtime_error(std::string("TTF_RenderText_Blended failed: ") + SDL_GetError());
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
    SDL_DestroySurface(surf);

    if (!tex) {
        throw std::runtime_error(std::string("SDL_CreateTextureFromSurface failed: ") + SDL_GetError());
    }

    textCache_[fk] = tex;
    return tex;
}
