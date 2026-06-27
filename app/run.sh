#!/usr/bin/env bash
# 国产系统(统信UOS/麒麟/Deepin) 一键启动
# 用法: bash run.sh   (需先编译对应架构二进制,见 README)
cd "$(dirname "$0")"
echo "============================================"
echo "  文物保护工程管理系统 启动中..."
echo "============================================"
echo "浏览器访问: http://127.0.0.1:5000"
echo "首次启动会自动导入 Basicdata 数据"
echo "按 Ctrl+C 停止服务"
# 自动选择当前架构的二进制
ARCH=$(uname -m)
BIN=""
[ -x "./heritage-mgmt" ] && BIN="./heritage-mgmt"
[ -z "$BIN" ] && [ "$ARCH" = "x86_64" ]   && [ -x "./heritage-mgmt-linux-amd64" ] && BIN="./heritage-mgmt-linux-amd64"
[ -z "$BIN" ] && [ "$ARCH" = "aarch64" ] && [ -x "./heritage-mgmt-linux-arm64" ]  && BIN="./heritage-mgmt-linux-arm64"
if [ -z "$BIN" ]; then
  echo "未找到对应架构的可执行文件(当前架构: $ARCH)。"
  echo "请在 Windows 开发机执行(入口在 cmd/heritage):"
  echo "  GOOS=linux GOARCH=amd64 go build -o heritage-mgmt-linux-amd64 ./cmd/heritage"
  echo "  GOOS=linux GOARCH=arm64 go build -o heritage-mgmt-linux-arm64  ./cmd/heritage"
  exit 1
fi
( xdg-open http://127.0.0.1:5000 >/dev/null 2>&1 & )
exec "$BIN"
