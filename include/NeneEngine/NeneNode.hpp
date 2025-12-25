#pragma once
#include <memory>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <map>
#include <unordered_map>
#include <SDL3/SDL.h>
#include <NeneEngine/NeneServer.hpp>

// ねねノード(基底クラス)
class NeneNode {
public:
    std::string name;
    explicit NeneNode(std::string);
    virtual ~NeneNode() = default;
    void set_valve_sdl_event(bool v)   { valve_sdl_event = v; }
    void set_valve_time_lapse(bool v)  { valve_time_lapse = v; }
    void set_valve_nene_mail(bool v)   { valve_nene_mail = v; }
    void set_valve_render(bool v)      { valve_render = v; }
    void set_active(bool v) {
        set_valve_sdl_event(v);
        set_valve_time_lapse(v);
        set_valve_nene_mail(v);
        set_valve_render(v);
    }
protected:
    void show_tree(std::ostream& os = std::cout) const;
    // イベントパルス
    void pulse_sdl_event(const SDL_Event&);   // →.cpp
    void pulse_time_lapse(const float&);      // →.cpp
    void pulse_nene_mail(const NeneMail&);    // →.cpp
    void pulse_render(SDL_Renderer*);         // →.cpp（幅優先）
    // 水門(パルスを遮断する)
    bool valve_sdl_event = true;
    bool valve_time_lapse = true;
    bool valve_nene_mail = true;
    bool valve_render = true;
    // ねねサーバ共有
    std::shared_ptr<NeneMailServer> mail_server;
    std::shared_ptr<NeneImageLoader> asset_loader;
    std::shared_ptr<NeneFontLoader> font_loader;
    std::shared_ptr<PathService> path_service;
    std::shared_ptr<NeneGlobalSettings> global_settings;
    std::shared_ptr<NeneCollisionWorld> collision_world;
    // 子ノード
    std::map<std::string, std::unique_ptr<NeneNode>> children; // アルファベット順
    // ノード初期化パルスの前方フック
    virtual void init_node() {}
    // イベントパルスの前方フック
    virtual void handle_sdl_event(const SDL_Event&) {}
    virtual void handle_time_lapse(const float&) {}
    virtual void handle_nene_mail(const NeneMail&) {}
    virtual void render(SDL_Renderer*) {}
    // 親子付け
    virtual void add_child(std::unique_ptr<NeneNode>); // →.cpp
    bool remove_child(const std::string& name) {
        return children.erase(name) > 0;
    }
    void clear_children() {
        children.clear();
    }
    // ノードからメール送信
    void send_mail(const NeneMail& mail) {
        if (mail_server) mail_server->push(mail);
    }
    void send_mail(NeneMail&& mail) {
        if (mail_server) mail_server->push(std::move(mail));
    }
    // ターミナル出力
    void nnlog(std::string_view msg) const; // →.cpp
    void nnerr(std::string_view msg) const; // →.cpp
    void nnthrow(std::string_view msg) const; // →.cpp
private:
    void dump_tree_impl(std::ostream& os, const std::string& prefix, bool is_last) const;
};

// ねねルート
class NeneRoot : public NeneNode {
public:
    explicit NeneRoot(std::string, const char*, int, int, Uint32, int, int, const char*); // →.cpp
    ~NeneRoot();
    int run(); // →.cpp
private:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool running = false;
    virtual void handle_sdl_event(const SDL_Event&) override;
    virtual void handle_nene_mail(const NeneMail& mail) override;
    bool tree_built = false;
};

// ねねスイッチ
class NeneSwitch : public NeneNode {
public:
    using Factory = std::function<std::unique_ptr<NeneNode>()>;
    explicit NeneSwitch(std::string name) : NeneNode(std::move(name)) {}
    // ノード登録
    void register_node(std::string node_name, Factory factory) {
        if (!factory) nnthrow("register_node: factory is null");
        factories_.emplace(std::move(node_name), std::move(factory));
    }
    // 現在のノード名（無ければ空）
    const std::string& current_node() const { return current_node_; }
    // 切替（破棄→生成→build）
    void switch_to(std::string_view node_name, bool force = false, bool initial = false) {
        const std::string key(node_name);
        if (!force && current_node_ == key) return;
        auto it = factories_.find(key);
        if (it == factories_.end()) nnthrow("switch_to: unknown target: " + key);
        // 今のノード破棄
        clear_children();
        // 新しいノード生成
        auto node = (it->second)();
        if (!node) nnthrow("switch_to: factory returned null: " + key);
        add_child(std::move(node));
        current_node_ = node_name;
        if(!initial) { // 最初のツリー生成は切り替えではないのでログとツリーの表示はしない
            nnlog(std::string("switched to ") + current_node_);
            send_mail(NeneMail(this->global_settings->root_name, this->name, "show_all", ""));
        }
    }
    // 初期ノード指定（init_node内で呼ぶ用）
    void set_initial_node(std::string_view node_name) {
        switch_to(node_name, true, true);
    }
protected:
    void handle_nene_mail(const NeneMail& mail) override {
        if (mail.to != this->name) return;
        if (mail.subject != "switch_to") return;
        if (mail.body.empty()) return;
        const bool force = (mail.body == current_node());
        switch_to(mail.body, force);
    }

private:
    std::unordered_map<std::string, Factory> factories_;
    std::string current_node_;
    std::string mail_subject_ = "switch_to";
};