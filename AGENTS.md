# AGENTS.md
このC++ライブラリ「NeneEngine」はゲームを支援するためのライブラリで, NeneNodeをベースとしたフレームワークと, その他の各種ツール群によって構成されています. NeneNodeはゲーム全体に木構造を与え, ゲーム制作における量的な困難の軽減やスケーラビリティの提供などを行います. また, (Unityなどとことなり)ゲーム制作をテキストエディタ上で完結させることで, AIを最大限に活用することを可能にします. 具体的な運用方法はデモとして制作したChromeDinoのソースコードを確認してください. 本プロジェクトでは, 引き続きこのライブラリの改善を行っていきます.

## 開発環境
- OS: Windows11
- プログラミング言語: C++
- 外部ライブラリ: SDL3  
    (注意! SDL2とSDL3は仕様が異なるので, 必ずSDL3の仕様を確認すること)
- エディタ: VS Code, Codex app.
- コンパイラ: MSVC
- パッケージ管理: vcpkg
- ビルドシステム: CMake
- バージョン管理: git
- ターミナル: pwsh

## 構成
ライブラリ本体は以下の4×2=8個によって構成されています. 特段の指示がない限り, ファイルを増やさないでください.
- NeneNode.hpp(.cpp)  
    基幹フレームワーク `NeneNode` と, それを継承した特殊なノード.
- NeneServer.hpp(.cpp)  
    ノードが共有的に利用するクラス.
- NeneComponents.hpp(.cpp)  
    ノードが専有的に利用するクラス.
- NeneUtilities.hpp(.cpp)  
    便利関数.

## テスト
ライブラリを変更したら, デモをコンパイルして, 成功することを確認してください. (note/cmd.md にあるコマンド. コマンドが間違っていたら指摘してください)

## from agent to agent
チャットやプロジェクトを横断する視野を持たないエージェントのために, 作業内で新たに確立した編集方針をここに追記してください.

### NeneInput
- 意味論的な入力は, 可能な限り各ゲームオブジェクトで直接 `SDL_Event` を見ず, `NeneInput` / `handle_nene_input()` で扱う.
- 物理入力から意味論的入力への変換は `NeneInputInterpreter` に集約する. 入力マップは `NeneBlackboard` の `input_maps` / `bind_key()` などで設定する.
- シーンごとに入力の意味が変わる場合は, そのシーン直下に `NeneInputInterpreter` を置き, シーン破棄・停止と一緒に入力解釈も止まるようにする.
- パルス順は `SDL event -> NeneInput -> TimeLapse -> NeneMail -> render` を維持する. 入力を `NeneMail` に置き換えると `TimeLapse` より後ろに回り, 1フレーム遅れる可能性がある.

### Windows / CMake
- このリポジトリのテキストファイルはCRLF前提のものが多い. 編集後に mixed EOL になると VS Code の IntelliSense が大量の偽エラーを出すことがあるため, 既存の改行種別に揃える.
- Markdown (`.md`) の行末空白はビュワーによって改行として扱われることがあるため, 自動整形や改行統一の際にも削除しない.
- vcpkg の `applocal.ps1` が WindowsApps の壊れた `pwsh.exe` shim を拾う環境がある. `CMakeLists.txt` では実体のある Windows PowerShell を `Z_VCPKG_POWERSHELL_PATH` に固定しているので, この回避策を不用意に削らない.
- ライブラリ変更後は `cmake --build build --config Debug` で `ChromeDino.exe` までビルドできることを確認する.
