#include <iostream>
#include <NeneNode.hpp>

// NeneNode
NeneNode::NeneNode(std::string s)
    : name(std::move(s)) {}

void NeneNode::make_tree() {
    init_node();
    for (auto &kv : children) {
        if (kv.second) kv.second->make_tree();
    }
}

void NeneNode::pulse_sdl_event(SDL_Event* ev) {
    if (!valve_opening_sdl_event) return;
    handle_sdl_event(ev);
    for (auto &kv : children) {
        if (kv.second) kv.second->pulse_sdl_event(ev);
    }
}

void NeneNode::pulse_time_lapse(float dt) {
    if (!valve_opening_sdl_event) return;
    handle_time_lapse(dt);
    for (auto &kv : children) {
        if (kv.second) kv.second->pulse_time_lapse(dt);
    }
}

// NeneRoot
void NeneRoot::setup(const char *title, int x, int y, int w, int h, Uint32 flags) {
    // 1. sdl初期化
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL初期化失敗: " << SDL_GetError() << "\n";
    }
    // ウィンドウ生成
    window = SDL_CreateWindow(title, w, h, flags);
    SDL_SetWindowPosition(window, x, y);
    // 2. ねねサーバ立ち上げ
    this->nene_server = std::make_shared<NeneServer>();
    // 3. ツリー生成パルス送信
    make_tree();
}

void NeneRoot::teardown() {
    // 1. sdl後処理
}

int NeneRoot::run() {
    // メインループ
    running = true;
    while(running) {
        // イベントパルス送信
        SDL_Event ev;
        while(SDL_PollEvent(&ev)) {
            pulse_sdl_event(&ev);
        }
        // ビューワールド撮影
        // スーパー合成
        // 出力

        // リフレッシュレート調整(後でちゃんと調整する)
        SDL_Delay(16);
    }
    // teardown
}

// NeneRoot::handle_sdl_event(SDL_Event ev) {
//         if (ev.type == SDL_EVENT_QUIT) {
//         running = false;
//     }
// }



void NeneLeaf::add_child(std::unique_ptr<NeneNode>)
{
    std::cerr << "！["+this->name+"] ねねリーフには子を追加できません。\n";
}