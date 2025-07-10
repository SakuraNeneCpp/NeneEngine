// 依存性グループA: NeneNode
#pragma once
#include <memory>
#include <string>
#include <map>
#include <SDL3/SDL.h>
#include <NeneServer.hpp>
#include <NeneObject.hpp>

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
    void pulse_nene_event(const NeneEvent&); // →.cpp
    // イベントパルスの水門
    bool valve_opening_sdl_event = true;
    bool valve_opening_time_lapse = true;
    bool valve_opening_nene_event = true;
    // ねねサーバ
    std::shared_ptr<NeneServer> nene_server;
    // 子ノード
    std::map<std::string, std::unique_ptr<NeneNode>> children; // アルファベット順に並ぶ
    // ツリー生成パルスの前方フック
    // サービス取得, キャラクターの配置など, 子ノード生成・親子付け
    virtual void init_node() noexcept {};
    // イベントパルスの前方フック
    virtual void handle_sdl_event(const SDL_Event&) noexcept {};
    virtual void handle_time_lapse(const float&) noexcept {};
    virtual void handle_nene_event(const NeneEvent&) noexcept {};
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
    // [C++Tips] 不完全型(opaque struct)
    // 前方宣言によって, サイズやメンバが不定のまま定義された型.
    // メモリ領域をどの程度確保すればいいのか不明なので, 実体(値)を定義できない.
    // クロスプラットフォームを実現するためにSDL_WindowやSDL_Rendererは不完全型で定義されている.
    // ポインタや参照は扱える.
    bool running;
};

// ねねスイッチ(子を一つしか持たない)
class NeneSwitch : public NeneNode {
    void add_child(std::unique_ptr<NeneNode>) override final; // →.cpp
};

// ねねグループ(複数の子を持つ. 子の水門を制御する)
class NeneGroup : public NeneNode {
    void valve_close_sdl_event(std::string);
    void valve_close_time_lapse(std::string);
    void valve_close_nene_event(std::string);
    void valve_reset(); // すべての子のすべての水門を開放する
};

// ねねリーフ
// 末端のノード
class NeneLeaf : public NeneNode {
protected:
    void add_child(std::unique_ptr<NeneNode>) override final; // →.cppで空実装
};