#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <NeneEngine/NeneServer.hpp>

NeneFontLoader::NeneFontLoader(SDL_Renderer* renderer)
    : renderer_(renderer) {
    if (!renderer_) {
        throw std::runtime_error("NeneFontLoader: renderer is null");
    }
    if (!TTF_Init()) {
        throw std::runtime_error(std::string("TTF_Init failed: ") + SDL_GetError());
    }
}

NeneFontLoader::~NeneFontLoader() {
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

TTF_Font* NeneFontLoader::get_font(const std::string& fontPath, int fontSize) {
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

SDL_Texture* NeneFontLoader::get_text_texture(const std::string& fontPath, int fontSize,
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
