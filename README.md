<!-- ![ロゴ](figs/logo.png "ロゴ") -->
# NeneEngine
ねねエンジンへようこそ! ねねエンジンはC++でゲームを作るためのライブラリです.
## ねねエンジンの仕組み
![仕組み](figs/img1.png "仕組み")

###  ファイル構成
- NeneNode.cpp(.hpp)  
    基幹システム. NeneServerをinclude(唯一の依存関係)
- Neneserver.cpp(.hpp)  
    共有的に使うサービス. 親から子へ注入される
- NeneComponents.hpp  
    専有的に使うクラスや構造体. 状態を持つものだけ
- NeneUtilities.hpp  
    便利な関数. 計算だけする


### 木構造+パルス伝播+共有サービス注入

### ノード間通信: 黒板とメール

### 特殊なノード(随時追加)
- NeneRoot
- NeneSwitch

### 共有サービス(随時追加)
- NeneImageLoader
- NeneFontLoader
- NeneCollisionWorld

## NeneNodeGallery
ノードのテンプレートなどをまとめたHTML.

[NeneNodeGallery](./NeneNodeGallery/main.html)

↑ ソースコードが開く. ブラウザで表示してほしいのに...

## リリースノート

### ver. 1.0.0


## TODO
### ノード
- 物理演算オブジェクト `NenePhysicalObject` (重力と衝突判定を受ける)
- デバッグノード (コライダーの可視化など)
- インプットマネージャ (入力をNeneMailに翻訳してブロードキャストする)
### サーバ
- セーブサービス `NeneSave`
### コンポーネント
- アニメーション制御装置 `NeneAnimator`
- 時間管理装置 `NeneTimer`
- 
