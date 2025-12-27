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
#include <type_traits>
#include <utility>
#include <variant>
#include <SDL3_image/SDL_image.h>

// ステートマシン
template <class StateId, class Context, class Event = std::monostate>
class NeneStateMachine {
public:
    using Transition = std::optional<StateId>;
    struct State {
        // enter/exit は副作用のみ（遷移させたい場合は外側で transition_to を呼ぶか、
        // update/dispatch で返す）
        std::function<void(Context&)> on_enter;
        std::function<void(Context&)> on_exit;
        // 遷移したいときは next を返す（返さなければ同じ状態を継続）
        std::function<Transition(Context&, float)> on_update;
        // イベントで遷移したいとき
        std::function<Transition(Context&, const Event&)> on_event;
    };
public:
    NeneStateMachine() {
        static_assert(std::is_copy_constructible_v<StateId>,
                      "StateId must be copy-constructible (enum class recommended).");
    }
    // 状態登録
    void add_state(StateId id, State st) {
        states_[id] = std::move(st);
    }
    bool has_state(StateId id) const {
        return states_.find(id) != states_.end();
    }
    // 初期化（enter も呼ぶ）
    void set_initial(StateId id, Context& ctx) {
        ensure_state_(id);
        current_ = id;
        started_ = true;
        call_enter_(id, ctx);
    }
    bool started() const { return started_; }
    std::optional<StateId> current() const { return current_; }
    // 強制遷移（exit->enter）
    void transition_to(StateId next, Context& ctx) {
        ensure_state_(next);
        if (!started_) {
            set_initial(next, ctx);
            return;
        }
        if (current_.has_value() && current_.value() == next) return;
        // 遷移中にさらに遷移要求が来たときの安全策：キューに積む
        pending_ = next;
        if (in_transition_) return;
        in_transition_ = true;
        while (pending_.has_value()) {
            const StateId target = pending_.value();
            pending_.reset();
            if (current_.has_value()) {
                call_exit_(current_.value(), ctx);
            }
            current_ = target;
            call_enter_(target, ctx);
        }
        in_transition_ = false;
    }
    // 更新
    void update(Context& ctx, float dt) {
        if (!started_ || !current_.has_value()) return;
        const State& st = state_ref_(current_.value());
        if (!st.on_update) return;

        if (auto next = st.on_update(ctx, dt)) {
            transition_to(*next, ctx);
        }
    }
    // イベント投入（Event を使う場合）
    void dispatch(Context& ctx, const Event& ev) {
        if (!started_ || !current_.has_value()) return;
        const State& st = state_ref_(current_.value());
        if (!st.on_event) return;

        if (auto next = st.on_event(ctx, ev)) {
            transition_to(*next, ctx);
        }
    }
    // 便利：イベント型を使わない場合でも呼べるように
    // (Event=std::monostate のときはこれを呼べばOK)
    void dispatch(Context& ctx) {
        dispatch(ctx, Event{});
    }
private:
    void ensure_state_(StateId id) const {
        if (!has_state(id)) {
            throw std::runtime_error("NeneStateMachine: unknown state");
        }
    }
    const State& state_ref_(StateId id) const {
        auto it = states_.find(id);
        if (it == states_.end()) {
            throw std::runtime_error("NeneStateMachine: state not found");
        }
        return it->second;
    }
    State& state_ref_(StateId id) {
        auto it = states_.find(id);
        if (it == states_.end()) {
            throw std::runtime_error("NeneStateMachine: state not found");
        }
        return it->second;
    }
    void call_enter_(StateId id, Context& ctx) {
        State& st = state_ref_(id);
        if (st.on_enter) st.on_enter(ctx);
    }
    void call_exit_(StateId id, Context& ctx) {
        State& st = state_ref_(id);
        if (st.on_exit) st.on_exit(ctx);
    }
private:
    std::unordered_map<StateId, State> states_;
    std::optional<StateId> current_;
    bool started_ = false;
    // 遷移の再入対策
    bool in_transition_ = false;
    std::optional<StateId> pending_;
};

// アニメーション制御装置
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
