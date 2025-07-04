# 開発環境構築のための備忘録

## C++開発環境構築
後で書く.

## 外部ライブラリインストール
後で書く.

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