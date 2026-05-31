#pragma once
#include <memory>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <map>
#include <unordered_map>
#include <utility>
#include <SDL3/SDL.h>
#include <NeneEngine/NeneServer.hpp>

// ねねノード(基底クラス)
class NeneNode {
public:
    std::string name;
    explicit NeneNode(std::string);
    virtual ~NeneNode() = default;
    void set_valve_sdl_event(bool v)   { valve_sdl_event = v; }
    void set_valve_nene_input(bool v)  { valve_nene_input = v; }
    void set_valve_time_lapse(bool v)  { valve_time_lapse = v; }
    void set_valve_nene_mail(bool v)   { valve_nene_mail = v; }
    void set_valve_render(bool v)      { valve_render = v; }
    void set_active(bool v) {
        set_valve_sdl_event(v);
        set_valve_nene_input(v);
        set_valve_time_lapse(v);
        set_valve_nene_mail(v);
        set_valve_render(v);
    }
    // dirty伝播
    void set_render_z(int z) {
        if (render_z == z) return;
        render_z = z;
        mark_render_dirty();
    }
    int  get_render_z() const { return render_z; }
    void set_save_service(std::shared_ptr<NeneSaveService> service);
    std::shared_ptr<NeneSaveService> get_save_service() const { return save_service; }
    std::shared_ptr<NeneTaskServer> get_task_server() const { return task_server; }
    void save_subtree(NeneSaveDocument& doc, std::string_view path = "") const;
    void load_subtree(const NeneSaveDocument& doc, std::string_view path = "");
protected:
    void show_tree(std::ostream& os = std::cout) const;
    // イベントパルス
    void pulse_sdl_event(const SDL_Event&);   // →.cpp
    void pulse_nene_input(const NeneInput&);  // →.cpp
    void pulse_time_lapse(const float&);      // →.cpp
    void pulse_nene_mail(const NeneMail&);    // →.cpp
    void pulse_render(SDL_Renderer*);         // →.cpp
    // 水門(パルスを遮断する)
    bool valve_sdl_event = true;
    bool valve_nene_input = true;
    bool valve_time_lapse = true;
    bool valve_nene_mail = true;
    bool valve_render = true;
    // z座標. 低いほど先に描画 (奥)、高いほど後に描画 (手前)
    int render_z = 0;
    // ねねサーバ共有
    std::shared_ptr<NeneMailServer> mail_server;
    std::shared_ptr<NeneInputServer> input_server;
    std::shared_ptr<NeneImageLoader> asset_loader;
    std::shared_ptr<NeneFontLoader> font_loader;
    std::shared_ptr<NeneSoundLoader> sound_loader;
    std::shared_ptr<PathService> path_service;
    std::shared_ptr<NeneTaskServer> task_server;
    std::shared_ptr<NeneSaveService> save_service;
    std::shared_ptr<NeneBlackboard> blackboard;
    std::shared_ptr<NeneCollisionWorld> collision_world;
    // 親ノード
    NeneNode* parent = nullptr;
    // 子ノード
    std::map<std::string, std::unique_ptr<NeneNode>> children; // アルファベット順
    // ノード初期化パルスの前方フック
    virtual void init_node() {}
    // イベントパルスの前方フック
    virtual void handle_sdl_event(const SDL_Event&) {}
    virtual void handle_nene_input(const NeneInput&) {}
    virtual void handle_time_lapse(const float&) {}
    virtual void handle_nene_mail(const NeneMail&) {}
    virtual void render(SDL_Renderer*) {}
    virtual std::string save_type() const { return ""; }
    virtual void save_state(NeneSaveWriter&) const {}
    virtual void load_state(const NeneSaveReader&) {}
    // dirty伝播
    void mark_render_dirty() {
        render_cache_dirty_ = true;
        if (parent) parent->mark_render_dirty();
    }
    // 親子付け
    virtual void add_child(std::unique_ptr<NeneNode>); // →.cpp
    bool remove_child(const std::string& name); // →.cpp
    void clear_children(); // →.cpp
    // ノードからメール送信
    void send_mail(const NeneMail& mail) {
        if (mail_server) mail_server->push(mail);
    }
    void send_mail(NeneMail&& mail) {
        if (mail_server) mail_server->push(std::move(mail));
    }
    // ノードから入力送信
    void send_input(const NeneInput& input) {
        if (input_server) input_server->push(input);
    }
    void send_input(NeneInput&& input) {
        if (input_server) input_server->push(std::move(input));
    }
    // ターミナル出力
    void nnlog(std::string_view msg) const; // →.cpp
    void nnerr(std::string_view msg) const; // →.cpp
    void nnthrow(std::string_view msg) const; // →.cpp
private:
    void dump_tree_impl(std::ostream& os, const std::string& prefix, bool is_last) const;
    mutable bool render_cache_dirty_ = true;
    mutable std::vector<NeneNode*> render_cache_;
    void rebuild_render_cache_() const;
};

// ねねルート
class NeneRoot : public NeneNode {
public:
    explicit NeneRoot(std::string, const char*, int, int, Uint32, int, int, const char*); // →.cpp
    ~NeneRoot();
    int run(); // →.cpp
    void configure_save_service(std::string org, std::string app,
                                std::string save_dir = "saves",
                                std::string extension = ".nnsave");
    void save_tree_to_slot(std::string_view slot_name) const;
    void load_tree_from_slot(std::string_view slot_name);
    void set_auto_blackboard_settings(bool enabled) {
        auto_blackboard_settings_ = enabled;
    }
    void set_blackboard_settings_slot(std::string slot_name) {
        blackboard_settings_slot_ = std::move(slot_name);
    }
    void set_task_commit_budget_ms(double budget_ms) {
        task_commit_budget_ms_ = budget_ms;
    }
    void save_blackboard_settings_to_slot(std::string_view slot_name = "_settings") const;
    bool load_blackboard_settings_from_slot(std::string_view slot_name = "_settings");
private:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool running = false;
    virtual void handle_sdl_event(const SDL_Event&) override;
    virtual void handle_nene_mail(const NeneMail& mail) override;
    bool tree_built = false;
    bool auto_blackboard_settings_ = true;
    std::string blackboard_settings_slot_ = "_settings";
    double task_commit_budget_ms_ = 2.0;
    void apply_blackboard_window_settings_();
};

// ねね入力通訳
class NeneInputInterpreter : public NeneNode {
public:
    explicit NeneInputInterpreter(std::string name, std::string map_name)
        : NeneNode(std::move(name)), map_name_(std::move(map_name)) {}
    void set_input_map(std::string_view map_name) {
        map_name_ = std::string(map_name);
    }
    const std::string& input_map() const { return map_name_; }
protected:
    void handle_sdl_event(const SDL_Event& ev) override;
private:
    std::string map_name_;
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
        if(!force && !initial) { // 最初のツリー生成とリフレッシュはツリーの表示はしない
            nnlog(std::string("switched to ") + current_node_);
            send_mail(NeneMail(this->blackboard->root_name, this->name, "show_all", ""));
        }
        if(force && !initial) {
            nnlog(std::string("refreshed ") + current_node_);
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

// ねねファクトリ (子ノードを生成/破棄するノード. 敵の生成などで使う)
class NeneFactory : public NeneNode {
public:
    // type, instance_name, arg を受け取ってノードを作る
    using Factory = std::function<std::unique_ptr<NeneNode>(std::string instance_name, std::string_view arg)>;
    explicit NeneFactory(std::string name) : NeneNode(std::move(name)) {}
    void register_type(std::string type, Factory factory) {
        if (!factory) nnthrow("register_type: factory is null");
        factories_.emplace(std::move(type), std::move(factory));
    }
protected:
    void handle_nene_mail(const NeneMail& mail) override;
private:
    std::unordered_map<std::string, Factory> factories_;
    std::unordered_map<std::string, int> seq_; // 自動命名用
};
