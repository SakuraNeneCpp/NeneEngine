ソースコードをテキスト化
```bash
$dest = ".\for_ai"
New-Item -ItemType Directory -Force $dest | Out-Null
Get-ChildItem -File -Recurse -Path ".\include\NeneEngine", ".\src", ".\ChromeDino" -Include *.cpp,*.hpp | ForEach-Object {Copy-Item -Force $_.FullName (Join-Path $dest ($_.Name + ".txt"))}
```