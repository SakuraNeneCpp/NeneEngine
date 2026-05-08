ソースコードをテキスト化
```bash
$dest = ".\for_ai"
New-Item -ItemType Directory -Force $dest | Out-Null
Get-ChildItem -File -Recurse -Path ".\include\NeneEngine", ".\src", ".\ChromeDino" -Include *.cpp,*.hpp | ForEach-Object {Copy-Item -Force $_.FullName (Join-Path $dest ($_.Name + ".txt"))}
```

```bash
# buildフォルダを作成・移動
mkdir build && cd build

# キャッシュファイル作成
# パスは自分がvcpkgを入れた場所に従って変更してください.
cmake ..  -G "Visual Studio 17 2022"  -A x64  -DCMAKE_TOOLCHAIN_FILE=C:/Users/**/vcpkg/scripts/buildsystems/vcpkg.cmake

# Debugフォルダの中に実行ファイルを生成
# 以降, コードを変更したらここからやり直す
cmake --build . --config Debug

# 実行
./Debug/ChromeDino.exe
```