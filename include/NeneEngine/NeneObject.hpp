// 依存性グループC: NeneObject
#pragma once
#include <string>

class NeneEvent {
};

class NeneViewObject {
public:
    NeneViewObject(); // 作る
    std::string name;
    const char* file_path;
    // スプライトシートにおける位置とサイズ
    int src_x;
    int src_y;
    int src_w;
    int src_h;
    // ゲームワールドにおける位置とサイズ
    float dst_x;
    float dst_y;
    float dst_z; // この値の順に描画する
    float dst_w;
    float dst_h;

    SDL_Texture* tex;
    // メンバ変数の変化に対応するために毎フレーム計算する
    SDL_FRect* src_frect();
    SDL_FRect* dst_frect();
};