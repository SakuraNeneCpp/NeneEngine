#include <iostream>
#include <queue>
#include <SDL3_image/SDL_image.h>
#include <NeneEngine/NeneNode.hpp>


// NeneNode
NeneNode::NeneNode(std::string node_name)
    : name(std::move(node_name)) {}

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
    if (!valve_time_lapse) return; // ★修正（sdl_event を見ていた）

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
    const std::string key = child->name;
    if (children.find(key) != children.end()) {
        throw std::runtime_error("[NeneNode] duplicated child name: " + key);
    }
    children.emplace(key, std::move(child));
}

// NeneRoot
NeneRoot::NeneRoot(std::string node_name, const char* title, int w, int h,
                   Uint32 flags, int x, int y, const char* icon_path)
    : NeneNode(std::move(node_name)) {

    // SDL 初期化
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "!!![" << this->name << "] SDL_Init failed: " << SDL_GetError() << "\n";
        return;
    }

    // ウィンドウ生成
    window = SDL_CreateWindow(title, w, h, flags);
    if (!window) {
        std::cerr << "!!![" << this->name << "] SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        return;
    }
    SDL_SetWindowPosition(window, x, y);

    // レンダラー生成
    renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::cerr << "!!![" << this->name << "] SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
        return;
    }

    // アイコン
    if (icon_path && icon_path[0] != '\0') {
        SDL_Surface* iconSurf = IMG_Load(icon_path);
        if (!iconSurf) {
            std::cerr << "![" << this->name << "] icon load failed: " << SDL_GetError() << "\n";
        } else {
            SDL_SetWindowIcon(window, iconSurf);
            SDL_DestroySurface(iconSurf);
        }
    }

    // ねねサーバ立ち上げ
    this->mail_server  = std::make_shared<MailServer>();
    this->asset_loader = std::make_shared<AssetLoader>(renderer);
    this->font_loader  = std::make_shared<FontLoader>(renderer);

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
        std::cout << "#[" << this->name << "] game tree initialized" << std::endl;
    }

    running = true;
    std::cout << "#[" << this->name << "] main loop start\n";

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

    std::cout << "#[" << this->name << "] main loop end\n";
    return 0;
}

void NeneRoot::handle_sdl_event(const SDL_Event& ev) {
    if (ev.type == SDL_EVENT_QUIT) {
        running = false;
    }
}
