<!-- ![ロゴ](figs/logo.png "ロゴ") -->
# NeneEngine
ねねエンジンへようこそ! ねねエンジンはC++でゲームを作るためのライブラリです.
## ねねエンジンの仕組み
![仕組み](figs/img1.png "仕組み")

### 木構造+パルス伝播+共有サービス注入

### ノード間通信: 黒板とメール

### 特殊なノード(随時追加)
- NeneRoot
- NeneSwitch

### 各種サービス(随時追加)
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
### その他
- アニメーション実装を簡単にしたい