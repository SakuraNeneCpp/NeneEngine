#include <iostream>
#include <ostream>
#include <queue>
#include <stdexcept>
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

void NeneNode::make_tree() {
    init_node();
    for (auto& kv : children) {
        if (kv.second) kv.second->make_tree();
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

void NeneNode::pulse_nene_mail(const NeneMail& mail) {
    if (!valve_nene_mail) return;
    // 宛先が無い（ブロードキャスト）か、自分宛なら処理
    if (!mail.to.has_value() || mail.to.value() == this->name) {
        handle_nene_mail(mail);
    }
    // 宛先がどこかの子かもしれないので、常に下へ伝播
    for (auto& kv : children) {
        if (kv.second) kv.second->pulse_nene_mail(mail);
    }
}

// 幅優先で render 命令を伝播
void NeneNode::pulse_render(SDL_Renderer* r) {
    if (!r) return;
    std::queue<NeneNode*> q;
    q.push(this);
    while (!q.empty()) {
        NeneNode* node = q.front();
        q.pop();
        if (!node->valve_render) continue;
        node->render(r);
        for (auto& kv : node->children) {
            if (kv.second) q.push(kv.second.get());
        }
    }
}

void NeneNode::add_child(std::unique_ptr<NeneNode> child) {
    if (!child) return;
    // 共有サービスを親から引き継ぐ
    child->mail_server = this->mail_server;
    child->asset_loader = this->asset_loader;
    child->font_loader  = this->font_loader;
    child->path_service = this->path_service;
    child->global_settings = this->global_settings;
    child->collision_world_aabb_n = this->collision_world_aabb_n;
    const std::string key = child->name;
    if (children.find(key) != children.end()) nnthrow("duplicated child name: " + key);
    children.emplace(key, std::move(child));
}

// NeneRoot
NeneRoot::NeneRoot(std::string node_name, const char* title, int w, int h,
                   Uint32 flags, int x, int y, const char* icon_path)
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
    this->mail_server     = std::make_shared<MailServer>();
    this->asset_loader    = std::make_shared<AssetLoader>(renderer);
    this->font_loader     = std::make_shared<FontLoader>(renderer);
    this->path_service    = std::make_shared<PathService>();
    this->global_settings = std::make_shared<NeneGlobalSettings>();
    if (this->global_settings) {
        this->global_settings->root_name = this->name;
        this->global_settings->window_x = x;
        this->global_settings->window_y = y;
        this->global_settings->window_w = w;
        this->global_settings->window_h = h;
    }
    this->collision_world_aabb_n = std::make_shared<NeneCollisionWorld2D_AABB_N>();
}

NeneRoot::~NeneRoot() {
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

int NeneRoot::run() {
    if (!tree_built) {
        make_tree();
        tree_built = true;
        nnlog("game tree initialized");
        show_tree();
    }

    running = true;
    nnlog("main loop start");

    Uint64 prev_ticks = SDL_GetTicks();

    while (running) {
        // (1) SDLイベント
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            pulse_sdl_event(ev);
        }

        // (2) 時間経過
        Uint64 now_ticks = SDL_GetTicks();
        float dt = static_cast<float>(now_ticks - prev_ticks) / 1000.0f;
        prev_ticks = now_ticks;
        pulse_time_lapse(dt);

        // (3) NeneMail
        if (mail_server) {
            NeneMail mail;
            while (mail_server->pull(mail)) {
                pulse_nene_mail(mail);
            }
        }

        // (4) render (ここだけ幅優先)
        SDL_RenderClear(renderer);
        pulse_render(renderer);
        SDL_RenderPresent(renderer);

        // 仮のウェイト（後でFPS制御に置換する）
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
    if (global_settings) {
        if (ev.type == SDL_EVENT_WINDOW_MOVED) {
            global_settings->window_x = ev.window.data1;
            global_settings->window_y = ev.window.data2;
        } else if (ev.type == SDL_EVENT_WINDOW_RESIZED) {
            global_settings->window_w = ev.window.data1;
            global_settings->window_h = ev.window.data2;
        }
    }
}

void NeneRoot::handle_nene_mail(const NeneMail& mail) {
    if (mail.subject == "show_all" && mail.body.empty()) {
        show_tree();
    }
}