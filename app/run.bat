@echo off
chcp 65001 >nul
title 文物保护工程管理系统
cd /d "%~dp0"

REM 入口位于 cmd/heritage（步骤8起）；若二进制缺失则自动构建。
if not exist heritage-mgmt.exe (
  echo 未找到 heritage-mgmt.exe，正在构建（需本机已安装 Go）...
  go build -o heritage-mgmt.exe ./cmd/heritage
  if errorlevel 1 (
    echo.
    echo 构建失败。请安装 Go 后手动执行：
    echo     go build -o heritage-mgmt.exe ./cmd/heritage
    echo.
    pause
    exit /b 1
  )
)

echo ============================================
echo   文物保护工程管理系统 启动中...
echo ============================================
echo.
echo 浏览器请访问: http://127.0.0.1:5000
echo 首次启动会自动导入 Basicdata 数据
echo 关闭本窗口即可停止服务
echo.
start "" http://127.0.0.1:5000
heritage-mgmt.exe
pause
