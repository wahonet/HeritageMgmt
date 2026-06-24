@echo off
chcp 65001 >nul
title 文物保护工程管理系统
cd /d "%~dp0"
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
