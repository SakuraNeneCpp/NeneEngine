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
#include <SDL3_image/SDL_image.h>

struct NeneAnimFrame {
    SDL_FRect src{};
    float duration = 0.1f; // sec
};

struct NeneAnimClip {
    std::vector<NeneAnimFrame> frames;
    bool loop = true;
    std::string next = "";
};

class NeneAnimator {
public:
    void add_clip(std::string name, NeneAnimClip clip) {
        clips_.emplace(std::move(name), std::move(clip));
    }
    void play(std::string name, bool restart=false) {
        if (!restart && current_ == name) return;
        current_ = std::move(name);
        idx_ = 0;
        t_ = 0.0f;
        finished_ = false;
    }
    void set_speed(float s) { speed_ = s; } // 0で停止も可
    void update(float dt) {
        if (current_.empty()) return;
        auto it = clips_.find(current_);
        if (it == clips_.end()) return;
        auto& clip = it->second;
        if (clip.frames.empty()) return;
        if (finished_) return;
        float adv = dt * speed_;
        // dt が大きい時にフレーム飛びしても破綻しないよう while で進める
        while (adv > 0.0f && !finished_) {
            float dur = clip.frames[idx_].duration;
            float remain = dur - t_;
            if (adv < remain) {
                t_ += adv;
                adv = 0.0f;
            } else {
                adv -= remain;
                t_ = 0.0f;
                if (idx_ + 1 < (int)clip.frames.size()) {
                    idx_++;
                } else {
                    if (clip.loop) {
                        idx_ = 0;
                    } else {
                        finished_ = true;
                        if (!clip.next.empty()) play(clip.next, true);
                    }
                }
            }
        }
    }
    const SDL_FRect* src() const {
        auto it = clips_.find(current_);
        if (it == clips_.end()) return nullptr;
        auto& clip = it->second;
        if (clip.frames.empty()) return nullptr;
        return &clip.frames[idx_].src;
    }
    bool finished() const { return finished_; }
    const std::string& current() const { return current_; }
private:
    std::unordered_map<std::string, NeneAnimClip> clips_;
    std::string current_;
    int idx_ = 0;
    float t_ = 0.0f;
    float speed_ = 1.0f;
    bool finished_ = false;
};
