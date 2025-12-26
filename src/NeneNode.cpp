#include <iostream>
#include <ostream>
#include <queue>
#include <stdexcept>
#include <algorithm>
#include <SDL3_image/SDL_image.h>
#include <NeneEngine/NeneNode.hpp>


// NeneNode
NeneNode::NeneNode(std::string node_name)
    : name(std::move(node_name)) {}

// ターミナル出力
void NeneNode::nnlog(std::string_view msg) const {
    std::cout << "[" << this->name << "] " << msg << "\n";
}
void NeneNode::nnerr(std::string_view msg) const {
    std::cerr << "[" << this->name << "] " << msg << "\n";
}
[[noreturn]] void NeneNode::nnthrow(std::string_view msg) const {
    throw std::runtime_error("[" + this->name + "] " + std::string(msg));
}

// ツリー可視化
void NeneNode::show_tree(std::ostream& os) const {
    os << "\n"
       << "  "
       << "[" << name << "]\n";
    const std::size_t n = children.size();
    std::size_t i = 0;
    for (auto const& kv : children) {
        ++i;
        const bool last = (i == n);
        if (kv.second) {
            kv.second->dump_tree_impl(os, "", last);
        }
    }
    os << "\n";
}

void NeneNode::dump_tree_impl(std::ostream& os, const std::string& prefix, bool is_last) const {
    os << "  "
       << prefix
       << (is_last ? "└─ " : "├─ ")
       << "[" << name << "]";
    if (!valve_sdl_event && !valve_time_lapse && !valve_nene_mail && !valve_render) os << " (inactive)";
    else if (!valve_render) os << " (render off)";
    os << "\n";
    const std::string next_prefix = prefix + (is_last ? "   " : "│  ");
    const std::size_t n = children.size();
    std::size_t i = 0;
    for (auto const& kv : children) {
        ++i;
        const bool last = (i == n);
        if (kv.second) {
            kv.second->dump_tree_impl(os, next_prefix, last);
        }
    }
}

void NeneNode::pulse_sdl_event(const SDL_Event& ev) {
    if (!valve_sdl_event) return;
    handle_sdl_event(ev);
    for (auto& kv : children) {
        if (kv.second) kv.second->pulse_sdl_event(ev);
    }
}

void NeneNode::pulse_time_lapse(const float& dt) {
    if (!valve_time_lapse) return;
    handle_time_lapse(dt);
    for (auto& kv : children) {
        if (kv.second) kv.second->pulse_time_lapse(dt);
    }
}

// void NeneNode::pulse_nene_mail(const NeneMail& mail) {
//     if (!valve_nene_mail) return;
//     // 宛先が無い（ブロードキャスト）か、自分宛なら処理
//     if (!mail.to.has_value() || mail.to.value() == this->name) {
//         handle_nene_mail(mail);
//     }
//     // 宛先がどこかの子かもしれないので、常に下へ伝播
//     for (auto& kv : children) {
//         if (kv.second) kv.second->pulse_nene_mail(mail);
//     }
// }

void NeneNode::pulse_nene_mail(const NeneMail& mail) {
    if (!valve_nene_mail) return;
    // 宛先が無い（ブロードキャスト）か、自分宛なら処理
    if (!mail.to.has_value() || mail.to.value() == this->name) {
        handle_nene_mail(mail); // ここで children が増減してもOK
    }
    // children を直接イテレートしない（keyスナップショット方式）
    std::vector<std::string> keys;
    keys.reserve(children.size());
    for (const auto& kv : children) keys.push_back(kv.first);
    for (const auto& key : keys) {
        auto it = children.find(key);
        if (it == children.end()) continue;  // 消えてたらスキップ
        if (!it->second) continue;
        it->second->pulse_nene_mail(mail);
    }
}

// render_zの順でrender命令を実行
void NeneNode::pulse_render(SDL_Renderer* renderer) {
    if (!renderer) return;
    if (render_cache_dirty_) {
        rebuild_render_cache_();
        render_cache_dirty_ = false;
    }
    // キャッシュ順に描画（valve_renderがOFFなら飛ばす）
    for (NeneNode* n : render_cache_) {
        if (!n) continue;
        if (!n->valve_render) continue;
        n->render(renderer);
    }
}

// ここのソートを毎フレームやりたくないのでdirtyフラグを使う
void NeneNode::rebuild_render_cache_() const {
    struct Item {
        int z;
        std::size_t seq;   // 同じzのとき順序を安定させる
        NeneNode* node;
    };
    std::vector<Item> items;
    items.reserve(128);
    std::queue<NeneNode*> q;
    q.push(const_cast<NeneNode*>(this));
    std::size_t seq = 0;
    // ツリーを走査して収集（ここはBFS/DFSどちらでもOK）
    while (!q.empty()) {
        NeneNode* n = q.front();
        q.pop();
        if (!n) continue;
        items.push_back(Item{ n->render_z, seq++, n });
        for (auto& kv : n->children) {
            if (kv.second) q.push(kv.second.get());
        }
    }
    std::stable_sort(items.begin(), items.end(),
        [](const Item& a, const Item& b) {
            if (a.z != b.z) return a.z < b.z;
            return a.seq < b.seq;
        });
    render_cache_.clear();
    render_cache_.reserve(items.size());
    for (const auto& it : items) {
        render_cache_.push_back(it.node);
    }
}


void NeneNode::add_child(std::unique_ptr<NeneNode> child) {
    if (!child) return;
    // 共有サービスを親から引き継ぐ
    child->mail_server = this->mail_server;
    child->asset_loader = this->asset_loader;
    child->font_loader  = this->font_loader;
    child->path_service = this->path_service;
    child->blackboard = this->blackboard;
    child->collision_world = this->collision_world;
    // 親を設定
    child->parent = this;
    // 同名の兄弟は区別できないのでthrow
    const std::string key = child->name;
    if (children.find(key) != children.end()) nnthrow("duplicated child name: " + key);
    // emplace は1回だけ
    auto [it, inserted] = children.emplace(key, std::move(child));
    if (!inserted) nnthrow("failed to emplace child: " + key);
    // 木構造が変化するのでrender順を再ソート
    mark_render_dirty();
    // 子を初期化（失敗したらロールバック）
    try {
        it->second->init_node();
    } catch (...) {
        children.erase(it);
        mark_render_dirty(); // 念のため
        throw;
    }
}

bool NeneNode::remove_child(const std::string& name) {
    auto it = children.find(name);
    if (it == children.end()) return false;
    if (it->second) it->second->parent = nullptr;
    children.erase(it);
    // 描画順キャッシュを更新する必要がある
    mark_render_dirty();
    return true;
}

void NeneNode::clear_children() {
    for (auto& kv : children) {
        if (kv.second) kv.second->parent = nullptr;
    }
    children.clear();
    mark_render_dirty();
}


// NeneRoot
NeneRoot::NeneRoot(std::string node_name, const char* title, int w, int h, Uint32 flags, int x, int y, const char* icon_path)
    : NeneNode(std::move(node_name)) {
    // SDL 初期化
    if (!SDL_Init(SDL_INIT_VIDEO)) { nnthrow("SDL_Init failed"); }
    // ウィンドウ生成
    window = SDL_CreateWindow(title, w, h, flags);
    if (!window) { nnthrow("SDL_CreateWindow failed"); }
    // ウィンドウ配置
    SDL_SetWindowPosition(window, x, y);
    // レンダラー生成
    renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) { nnthrow("SDL_CreateRenderer failed"); }
    // アイコン
    if (icon_path && icon_path[0] != '\0') {
        SDL_Surface* iconSurf = IMG_Load(icon_path);
        if (!iconSurf) { nnerr("icon load faile"); }
        else {
            SDL_SetWindowIcon(window, iconSurf);
            SDL_DestroySurface(iconSurf);
        }
    }
    // ねねサーバ立ち上げ
    this->mail_server     = std::make_shared<NeneMailServer>();
    this->asset_loader    = std::make_shared<NeneImageLoader>(renderer);
    this->font_loader     = std::make_shared<NeneFontLoader>(renderer);
    this->path_service    = std::make_shared<PathService>();
    this->blackboard = std::make_shared<NeneBlackboard>();
    if (this->blackboard) {
        this->blackboard->root_name = this->name;
        this->blackboard->window_x = x;
        this->blackboard->window_y = y;
        this->blackboard->window_w = w;
        this->blackboard->window_h = h;
    }
    this->collision_world = std::make_shared<NeneCollisionWorld>();
}

NeneRoot::~NeneRoot() {
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

int NeneRoot::run() {
    if (!tree_built) {
        init_node();
        tree_built = true;
        nnlog("game tree initialized");
        show_tree();
    }
    running = true;
    nnlog("main loop start");
    Uint64 prev_ticks = SDL_GetTicks();
    while (running) {
        // SDLイベント
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            pulse_sdl_event(ev);
        }
        // 時間経過
        Uint64 now_ticks = SDL_GetTicks();
        float dt = static_cast<float>(now_ticks - prev_ticks) / 1000.0f;
        prev_ticks = now_ticks;
        pulse_time_lapse(dt);
        // NeneMail
        if (mail_server) {
            NeneMail mail;
            while (mail_server->pull(mail)) {
                pulse_nene_mail(mail);
            }
        }
        // render (ここだけ幅優先)
        SDL_RenderClear(renderer);
        pulse_render(renderer);
        SDL_RenderPresent(renderer);
        // ウェイト（後でFPS制御に置換する）
        SDL_Delay(16);
    }
    nnlog("main loop end");
    return 0;
}

void NeneRoot::handle_sdl_event(const SDL_Event& ev) {
    if (ev.type == SDL_EVENT_QUIT) {
        running = false;
        return;
    }
    if (blackboard) {
        if (ev.type == SDL_EVENT_WINDOW_MOVED) {
            blackboard->window_x = ev.window.data1;
            blackboard->window_y = ev.window.data2;
        } else if (ev.type == SDL_EVENT_WINDOW_RESIZED) {
            blackboard->window_w = ev.window.data1;
            blackboard->window_h = ev.window.data2;
            blackboard->ground_y = ev.window.data2 - 120.0f;
        }
    }
}

void NeneRoot::handle_nene_mail(const NeneMail& mail) {
    if (mail.subject == "show_all" && mail.body.empty()) {
        show_tree();
    }
}

// ねねファクトリ
static std::vector<std::string> split_n(const std::string& s, char delim, int max_parts = -1) {
    std::vector<std::string> out;
    std::string cur;
    for (char ch : s) {
        if (ch == delim && (max_parts < 0 || (int)out.size() + 1 < max_parts)) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(ch);
        }
    }
    out.push_back(cur);
    return out;
}

void NeneFactory::handle_nene_mail(const NeneMail& mail) {
    if (mail.to != this->name) return;
    // spawn: body = "type|name|arg" （name/arg は省略可）
    if (mail.subject == "spawn") {
        if (mail.body.empty()) return;
        auto parts = split_n(mail.body, '|', 3);
        const std::string type = (parts.size() >= 1) ? parts[0] : "";
        std::string name       = (parts.size() >= 2) ? parts[1] : "";
        const std::string arg  = (parts.size() >= 3) ? parts[2] : "";
        if (type.empty()) return;
        auto it = factories_.find(type);
        if (it == factories_.end()) nnthrow("NeneFactory: unknown type: " + type);
        // name が無い場合は自動命名
        if (name.empty()) {
            int& n = seq_[type];
            name = type + "_" + std::to_string(n++);
        }
        auto node = (it->second)(name, arg);
        if (!node) nnthrow("NeneFactory: factory returned null: " + type);
        add_child(std::move(node));
        return;
    }
    // despawn: body = "child_name"
    if (mail.subject == "despawn") {
        if (mail.body.empty()) return;
        remove_child(mail.body);
        return;
    }
}
