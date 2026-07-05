# ============================================================
# 五子棋客户端打包脚本
# 用途：一键构建 Release 版本并生成可分发压缩包
# 依赖：Qt 5.15.2 MinGW 64-bit
# 用法：在 PowerShell 中运行：.\打包脚本.ps1
# ============================================================

# 路径配置（根据实际安装路径修改）
$Qt_qmake   = "F:\QT\5.15.2\mingw81_64\bin\qmake.exe"
$Qt_windeploy = "F:\QT\5.15.2\mingw81_64\bin\windeployqt.exe"
$Qt_make    = "F:\QT\Tools\mingw810_64\bin\mingw32-make.exe"
$SrcDir     = "F:\QLPj\fu"
$BuildDir   = "F:\QLPj\fu\build"
$DistDir    = "F:\QLPj\fu\GomokuClient-v1.0"
$ProFile    = "$SrcDir\GomokuClient.pro"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  五子棋客户端 v1.0 打包脚本" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

# ---------- 1. 检查工具链 ----------
if (!(Test-Path $Qt_qmake)) {
    Write-Host "[错误] qmake 未找到: $Qt_qmake" -ForegroundColor Red
    Write-Host "请确认 Qt 5.15.2 MinGW 64-bit 已正确安装" -ForegroundColor Yellow
    exit 1
}
Write-Host "[OK] qmake: $Qt_qmake" -ForegroundColor Green

# ---------- 2. 清理旧构建 ----------
if (Test-Path $BuildDir) {
    Write-Host "正在清理旧构建目录..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}
New-Item -ItemType Directory -Path $BuildDir | Out-Null

# ---------- 3. 设置 PATH ----------
$env:Path = "F:\QT\Tools\mingw810_64\bin;F:\QT\5.15.2\mingw81_64\bin;" + $env:Path

# ---------- 4. qmake 生成 Makefile ----------
Write-Host "正在运行 qmake..." -ForegroundColor Yellow
Set-Location $BuildDir
& $Qt_qmake $ProFile -spec win32-g++ "CONFIG+=release"

# ---------- 5. 编译 ----------
Write-Host "正在编译（Release 模式）..." -ForegroundColor Yellow
& $Qt_make -j4
if ($LASTEXITCODE -ne 0) {
    Write-Host "[错误] 编译失败，退出码: $LASTEXITCODE" -ForegroundColor Red
    exit 1
}
Write-Host "[OK] 编译成功" -ForegroundColor Green

# ---------- 6. windeployqt 打包 ----------
Write-Host "正在打包 Qt 依赖..." -ForegroundColor Yellow
Set-Location "$BuildDir\release"
& $Qt_windeploy GomokuClient.exe

# ---------- 7. 清理中间文件 ----------
Write-Host "正在清理编译中间文件..." -ForegroundColor Yellow
Get-ChildItem "$BuildDir\release" -Include *.o,*.cpp,moc_*.cpp,moc_*.h,Makefile*,*.mak -Recurse | Remove-Item -Force -ErrorAction SilentlyContinue

# ---------- 8. 复制到分发目录 ----------
if (Test-Path $DistDir) { Remove-Item -Recurse -Force $DistDir }
New-Item -ItemType Directory -Path $DistDir | Out-Null
Copy-Item "$BuildDir\release\*" "$DistDir\" -Recurse

# 重命名为中文
Get-ChildItem "$DistDir\GomokuClient.exe" | Rename-Item -NewName "五子棋客户端.exe"

# ---------- 9. 输出统计 ----------
$files = Get-ChildItem $DistDir -Recurse -File
$total = ($files | Measure-Object -Property Length -Sum).Sum / 1MB
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  打包完成！" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "分发目录: $DistDir" -ForegroundColor White
Write-Host "文件数量: $($files.Count) 个" -ForegroundColor White
Write-Host "总 大 小: $([math]::Round($total, 1)) MB" -ForegroundColor White
Write-Host ""
Write-Host "可直接将 GomokuClient-v1.0 目录分享给用户使用。" -ForegroundColor Cyan
Write-Host "双击目录内的「五子棋客户端.exe」即可运行。" -ForegroundColor Cyan
Write-Host ""
Write-Host "提示：建议用 7-Zip 压缩为 zip 减小体积后再上传/分享。" -ForegroundColor Yellow
