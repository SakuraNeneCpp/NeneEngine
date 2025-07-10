# 開発環境構築のための備忘録

## C++開発環境構築
#### 前提
- Windows 11 (x64)
- Visual Studio Code

#### 手順
1. **Visual Studio Build Tools(VSBT)**
    Visual Studioのエディターはこの世で最も醜いものの一つなので使わず, 中にあるBuild Toolsのみを使います. Visual Studioを入れずにこれだけをインストールできます.
    1. 公式サイト([https://visualstudio.microsoft.com/downloads/](https://visualstudio.microsoft.com/downloads/))の下の方からダウンロード(Tools for Visual Studio → Build Tools for Visual Studio 2022).
    2. C++によるデスクトップ開発にチェックを入れる.
2. **vcpkg**
    パッケージマネージャ. VSBTの中にもvcpkgがありますが, 偽物なので使いません. vcpkgはユーザーディレクトリなどに入れるのが一般的らしいです. (OSの直下にフォルダを作って開発用のツールをまとめる人もいるらしい)
    1. インストールしたい場所で以下を順に実行する.
        ```bash
        # インストール
        git clone https://github.com/microsoft/vcpkg.git

        # bootstrap
        .\bootstrap-vcpkg.bat

        # Visual Studioと統合
        .\vcpkg integrate install
        ```
    2. システム環境変数のPATHに追加
        vcpkg.exeを持っているフォルダのパスをコピーしてPATHに追加します. 最も外側のvcpkgだと思います.
3. **CMake**
    メタビルドシステム. 公式サイト([https://cmake.org/](https://cmake.org/))からインストールします.
    (「コンパイル」というのはソースコードを機械語に翻訳することであり, これはファイルごとに行われるので, 複数のソースコードからなるゲームを作るにはそれらを結合する「リンク」という作業が必要になります. これらをまとめて「ビルド」といい, ビルドを自動で行うツールを「ビルドツール」といいます. ビルドツールには複数あり, それらの仕様ごとに異なる形式のキャッシュを生成するのですが, これでは共同開発などで不便なことが多かったため, 選択的にビルドツールを生成してビルドを行う「メタビルドシステム」というものが誕生しました.)

4. **母国語を取り戻す**
    現状ではC++のコード内で日本語(UTF-8)を書くとコンパイルに失敗します. そこでWindowsの設定を見直す必要があります.
    1. 設定 → 時刻と言語 → 言語と地域 → 管理用の言語の設定 → システム ロケールの変更 → 「ベータ: ワールドワイド言語サポートでUnicode UTF-8を仕様」にチェックを入れる.
    2. PCを再起動する.
5. **VS Code拡張機能**
    - C/C++
    - CMake Tools

    あたりがあれば十分だと思います. あとはお好みで.

#### サンプルコード
プロジェクトフォルダの直下にCMakeLists.txtを作り, 以下のように書きます. (このファイル名にするとVSCode上でアイコンがMになると思います.)

```cmake
# 00_HelloCpp/CMakeLists.txt

# CMakeの最低バージョンを指定
cmake_minimum_required(VERSION 3.15)

# プロジェクトを定義(HelloCppという名前のCppで書かれたプロジェクト)
# このプロジェクト名はフォルダ名と同じである必要はない
project(HelloCpp LANGUAGES CXX)

# ビルドを実行(main.cppをコンパイルしてhello.exeを作る)
add_executable(hello main.cpp)

```

main.cppを適当に書きます.
```cpp
// 00_HelloCpp/main.cpp

#include <iostream>
#include <vector>
#include <string>

int main() {
  std::cout << "Hello, C++!" << std::endl;
  std::vector<std::string> Zoo = {"dog", "cat", "bird"};
	for (const auto& animal : Zoo){
		std::cout << animal << ' ';
	}
  std::cout << "" << std::endl;
  std::cout << "はろーわーるど！" << std::endl;
  return 0;
}
```

#### ビルドと実行

プロジェクトフォルダの直下で以下を順に実行します.
```bash
# buildフォルダを作成・移動
mkdir build && cd build

# キャッシュファイル作成
cmake ..

# Debugフォルダの中に実行ファイルを生成
# 以降, コードを変更したらここからやり直す
cmake --build . --config Debug

# 実行
./Debug/hello.exe
```

## 外部ライブラリインストール
vcpkgを使えば一瞬です. 今,
- **sdl3**
    SDLの本体.
- **sdl3-image[core,png]**
    画像のロードを行う. (SDL3ではこう書かないとpngが読み込めないっぽい(?))
- **sdl3-ttf**
    フォントのロードを行う.

のwindows(x64)版をインストールしたいとします. ターミナルで以下を実行してください.
```bash
vcpkg install sdl3:x64-windows sdl3-image[core,png]:x64-windows sdl3-ttf:x64-windows
```

## VSCodeからGitHubのリポジトリを操作する方法
### 1. クローン: GitHubのリポジトリを手元の端末にコピーする
1. Gitをインストールしてない場合はインストールする. `git --version`でインストールしているか確認できる. GitHubのアカウントも作ってない場合は作る.
2. VSCodeを開いて, 左下の「Accounts」から「Sign in with GitHub...」を選択.
3. パスワードを入力してサインイン. VSCodeを開く.
4. コマンドパレット(`Ctrl+Shift+P` / `⌘⇧P`)で`Git: Clone`.
5. リポジトリの管理者の場合, 一覧にあるリポジトリからクローンしたいリポジトリを選択. リポジトリの管理者でない場合, クローンしたいリポジトリのURLをペースト.
6. クローン先のフォルダを選択.

### 2. 編集
手元のVSCodeで好きなように編集する. いつでも編集前に戻れるので, 気にせず書きまくってよい(そのためのgit).

### 3. ステージング: どの変更をリポジトリに反映させるか選別する

### 4. コミット
(ここまでローカルでのGitの操作)

### 5. プッシュ

## Git / GitHub FAQ
### 1. 自分のリポジトリなのにプッシュできない
古いGitHubアカウントが残っている可能性があります. 「コントロールパネル」→「ユーザーアカウント」→「資格情報マネージャー」→「Windowsの資格情報」から古いGitHubアカウントを削除し, VSCodeを再起動してください.


### 2. 編集者の名義をPCのユーザー名から変更したい
Gitではコミットの “Author/Committer” に user.name と user.email が使われます. 未設定のときはOSのユーザー名が自動で入ります. これはGitHub上でコードの編集者として表示されます. 変更するときは以下を実行してください.

``` bash
# すべてのリポジトリで共通にしたい場合
git config --global user.name  "Your Name"
git config --global user.email "you@example.com"

# 今のリポジトリだけ変えたい場合
git config --local  user.name  "Your Name"
git config --local  user.email "you@example.com"
```