#!/usr/bin/env bash
# 在 Docker 容器内构建 desktop/（CMake + Qt6 + 单测），并打印产物架构。
# 用法:
#   ./docker/build.sh              # 默认 linux/amd64（快速构建 + 跑单测，开发迭代用）
#   ./docker/build.sh linux/arm64  # 产出麒麟 arm64 二进制（经 qemu，较慢）
#   ./docker/build.sh linux/arm64 nobuild  # 仅重建镜像不跑构建（调试用）
set -euo pipefail

PLATFORM="${1:-linux/amd64}"
RUN_BUILD="${2:-build}"
ARCH="$(echo "$PLATFORM" | cut -d/ -f2)"          # amd64 | arm64
BUILD_DIR="build-${ARCH}"                           # 按架构分目录，避免缓存跨架构污染
TAG="heritage-builder:$(echo "$PLATFORM" | tr '/' '-')"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DESKTOP="$(cd "$HERE/.." && pwd)"   # desktop/ 根

echo ">> 构建 builder 镜像 ($PLATFORM)"
docker build --platform "$PLATFORM" -t "$TAG" "$HERE"

if [[ "$RUN_BUILD" == "nobuild" ]]; then
    exit 0
fi

# Windows/Git Bash 下把宿主路径转 Windows 形式，并禁用 MSYS 路径转换，避免 :/src 被改写。
HOST_DESKTOP="$DESKTOP"
if command -v cygpath >/dev/null 2>&1; then
    HOST_DESKTOP="$(cygpath -w "$DESKTOP")"
fi

echo ">> 容器内 cmake 构建 + 单测 ($PLATFORM)"
MSYS_NO_PATHCONV=1 docker run --rm --platform "$PLATFORM" \
    -e BUILD_DIR="$BUILD_DIR" \
    -v "${HOST_DESKTOP}:/src" -w /src "$TAG" \
    bash -lc 'set -e; cmake -B "$BUILD_DIR"; cmake --build "$BUILD_DIR" -j"$(nproc)"; ctest --test-dir "$BUILD_DIR" --output-on-failure'

echo ">> 产物架构（heritage-desktop）:"
MSYS_NO_PATHCONV=1 docker run --rm --platform "$PLATFORM" \
    -e BUILD_DIR="$BUILD_DIR" \
    -v "${HOST_DESKTOP}:/src" -w /src "$TAG" \
    file "$BUILD_DIR/heritage-desktop"
