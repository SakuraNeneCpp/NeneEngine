#pragma once
#include <memory>
#include <string>
#include <map>
#include <SDL3/SDL.h>
#include <NeneEngine/NeneServer.hpp>

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
    void pulse_sdl_event(const SDL_Event&);   // →.cpp
    void pulse_time_lapse(const float&);      // →.cpp
    void pulse_nene_mail(const NeneMail&);    // →.cpp
    void pulse_render(SDL_Renderer*);         // →.cpp（幅優先）

    // 水門(パルスを遮断する)
    bool valve_opening_sdl_event = true;
    bool valve_opening_time_lapse = true;
    bool valve_opening_nene_event = true;
    bool valve_opening_render = true;

    // ねねサーバ共有
    std::shared_ptr<MailServer> mail_server;
    std::shared_ptr<AssetLoader> asset_loader;
    std::shared_ptr<FontLoader> font_loader;

    // 子ノード
    std::map<std::string, std::unique_ptr<NeneNode>> children; // アルファベット順

    // ツリー生成パルスの前方フック
    virtual void init_node() {}

    // イベントパルスの前方フック
    virtual void handle_sdl_event(const SDL_Event&) {}
    virtual void handle_time_lapse(const float&) {}
    virtual void handle_nene_mail(const NeneMail&) {}
    virtual void render(SDL_Renderer*) {}

    // 親子付け
    virtual void add_child(std::unique_ptr<NeneNode>); // →.cpp

    // 便利：ノードからメール送信
    void send_mail(const NeneMail& mail) {
        if (mail_server) mail_server->push(mail);
    }
    void send_mail(NeneMail&& mail) {
        if (mail_server) mail_server->push(std::move(mail));
    }
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

    bool tree_built = false;
};
