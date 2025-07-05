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
    explicit NeneNode(std::string); // →.cpp
    virtual ~NeneNode() = default;
protected:
    // ツリー初期化パルス
    void make_tree(); // →.cpp
    // イベントパルス
    void pulse_sdl_event(SDL_Event*); // →.cpp
    void pulse_time_lapse(float); // →.cpp
    void pulse_nene_event(NeneEvent*); // →.cpp
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
    virtual void handle_sdl_event(SDL_Event*) noexcept {};
    virtual void handle_time_lapse(float) noexcept {};
    virtual void handle_nene_event(NeneEvent*) noexcept {};
    // 親子付け
    virtual void add_child(std::unique_ptr<NeneNode>); // →.cpp
};

// ねねルート
class NeneRoot : public NeneNode {
public:
    int run(); // →.cpp
    // setupに引数を追加してウィンドウのサイズとかを設定できるようにする
    virtual void setup(); // →.cpp
    virtual void teardown(); // →.cpp
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