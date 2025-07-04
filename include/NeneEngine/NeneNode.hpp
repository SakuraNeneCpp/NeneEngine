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
private:
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
    // ねねサーバ
    std::shared_ptr<NeneServer> nene_server;
};

// ねねルート
class NeneRoot : public NeneNode{
public:
    // セットアップ
        // sdl初期化
        // ねねサーバ立ち上げ
        // ツリー生成パルス送信
    virtual void setup(); // →.cpp
    // メインループ
        // イベントパルス送信
        // レンダラークリア, ビューワールド撮影, スーパー合成, 出力
    int run(); // →.cpp
};

// ねねスイッチ(子を一つしか持たない)
class NeneSwitch : public NeneNode{
    void add_child(std::unique_ptr<NeneNode>) override final; // →.cpp
};

// ねねグループ(複数の子を持つ)
class NeneGroup : public NeneNode{

};

// ねねリーフ
// 末端のノード
class NeneLeaf : public NeneNode{
private:
    void add_child(std::unique_ptr<NeneNode>) override final; // →.cppで空実装
};