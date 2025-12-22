// 依存性グループA: NeneNode
#pragma once
#include <memory>
#include <string>
#include <map>
#include <SDL3/SDL.h>
#include <NeneServer.hpp>

// ねねノード(基底クラス)
class NeneNode {
public:
    std::string name;
    explicit NeneNode(std::string);
    virtual ~NeneNode() = default;
protected:
    // ツリー初期化パルス
    void make_tree(); // →.cpp
    // イベントパルス
    // 読み取り専用参照渡し(const T&)
    void pulse_sdl_event(const SDL_Event&); // →.cpp
    void pulse_time_lapse(const float&); // →.cpp
    void pulse_nene_mail(const NeneMail&); // →.cpp
    void pulse_render(SDL_Renderer*);
    // 水門(パルスを遮断する)
    bool valve_opening_sdl_event = true;
    bool valve_opening_time_lapse = true;
    bool valve_opening_nene_event = true;
    // ねねサーバ共有
    std::shared_ptr<MailServer> mail_server;
    std::shared_ptr<AssetLoader> asset_loader;
    std::shared_ptr<FontLoader> font_loader;
    // 子ノード
    std::map<std::string, std::unique_ptr<NeneNode>> children; // アルファベット順に並ぶ
    // ツリー生成パルスの前方フック
    // サービス取得, キャラクターの配置など, 子ノード生成・親子付け
    virtual void init_node() {};
    // イベントパルスの前方フック
    virtual void handle_sdl_event(const SDL_Event&) {};
    virtual void handle_time_lapse(const float&) {};
    virtual void handle_nene_event(const NeneMail&) {};
    // 親子付け
    virtual void add_child(std::unique_ptr<NeneNode>); // →.cpp
};

// ねねルート
class NeneRoot : public NeneNode {
public:
    // ねねノードとしての名前
    // ウィンドウの名前
    // ウィンドウの横幅
    // ウィンドウの縦幅
    // ウィンドウのリサイズ設定
    // ウィンドウ出現位置のx座標
    // ウィンドウ出現位置のy座標
    // ウィンドウのアイコン
    explicit NeneRoot(std::string, const char*, int, int, Uint32, int, int, const char*); // →.cpp
    ~NeneRoot();
    int run(); // →.cpp
private:
    SDL_Window* window;
    SDL_Renderer* renderer;
    bool running;
    // イベントパルスの前方フック
    virtual void handle_sdl_event(const SDL_Event&) override;
};