#include <iostream>
#include <SDL3_image/SDL_image.h>
#include <NeneNode.hpp>

// NeneNode
NeneNode::NeneNode(std::string node_name)
    : name(std::move(node_name)) {
    }

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
NeneRoot::NeneRoot(std::string node_name, const char* title, int w, int h, Uint32 flags, int x, int y, const char* icon_path)
    // [C++Tips] メンバ初期化リストの必要性
    // クラスAを継承しているクラスBを生成するとき, 以下が順に実行される.
    // Aのコンストラクタを実行 → Bのメンバを初期化 → Bのコンストラクタを実行.
    // もしAにデフォルトコンストラクタが無い場合, Bで明示的にAのコンストラクタを呼び出す必要がある.
    // このとき, Bのコンストラクタよりも前にAを初期化する必要があるため, メンバ初期化リストでしかこれを行えない.
    : NeneNode(node_name) {
    // sdl初期化
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "!!!["+this->name+"] SDL初期化失敗: " << SDL_GetError() << "\n";
        return;
    }
    // ウィンドウ生成
    window = SDL_CreateWindow(title, w, h, flags);
    // ウィンドウ出現位置調整
    SDL_SetWindowPosition(window, x, y);
    // アイコン画像読み込み (.pngだと失敗したので.svgにするとよい)
    SDL_Surface* iconSurf = IMG_Load(icon_path);
    if (!iconSurf) {
        std::cerr << "!["+this->name+"] アイコン画像読み込み失敗: " << SDL_GetError() << "\n";
    }
    // アイコン設定
    SDL_SetWindowIcon(window, iconSurf);
    // アイコンサーフェイス破棄
    SDL_DestroySurface(iconSurf);
    // レンダラー生成
    renderer = SDL_CreateRenderer(window, nullptr);
    // ねねサーバ立ち上げ
    this->nene_server = std::make_shared<NeneServer>();
    // ツリー生成パルス送信
    make_tree();
}

NeneRoot::~NeneRoot() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

int NeneRoot::run() {
    // メインループ
    running = true;
    std::cout << "#["+this->name+"] メインループ開始.\n";
    while(running) {
        // イベントパルス送信
        SDL_Event ev;
        while(SDL_PollEvent(&ev)) {
            pulse_sdl_event(&ev);
        }
        SDL_RenderClear(renderer);
        // ビューワールド撮影
        // スーパー合成
        SDL_RenderPresent(renderer);

        // リフレッシュレート調整(後でちゃんと調整する)
        SDL_Delay(16);
    }
    std::cout << "#["+this->name+"] メインループ終了.\n";
    return 0;
}

// NeneRoot::handle_sdl_event(SDL_Event ev) {
//         if (ev.type == SDL_EVENT_QUIT) {
//         running = false;
//     }
// }



void NeneLeaf::add_child(std::unique_ptr<NeneNode>) {
    std::cerr << "！["+this->name+"] ねねリーフには子を追加できません.\n";
}