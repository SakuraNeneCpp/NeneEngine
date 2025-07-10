// 依存性グループC: NeneObject
#pragma once
#include <string>

class NeneEvent {
};

class NeneViewObject {
public:
    std::string name;
    std::string file;
    // スプライトシートにおける位置とサイズ
    int sprite_x;
    int sprite_y;
    int sprite_w;
    int sprite_h;
    // ゲームワールドにおける位置とサイズ
    int world_x;
    int world_y;
    int world_z; // この値の順に描画する
    int world_w;
    int world_h;
    // 表示/非表示
    bool visible;
};