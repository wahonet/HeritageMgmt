#!/usr/bin/env bash
# 把构建产物连同 Qt 运行依赖打包成单目录，便于拷到麒麟机双击/终端运行。
# 用法: ./docker/package.sh [linux/arm64]
#
# 打包内容：二进制 + 全部传递依赖 .so（ldd） + Qt 平台/SQLite 插件（dlopen，ldd 抓不到）
#           + run.sh（设置 QT_PLUGIN_PATH / LD_LIBRARY_PATH）。
set -euo pipefail

PLATFORM="${1:-linux/arm64}"
ARCH="$(echo "$PLATFORM" | cut -d/ -f2)"          # amd64 | arm64
BUILD_DIR="build-${ARCH}"
TAG="heritage-builder:$(echo "$PLATFORM" | tr '/' '-')"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DESKTOP="$(cd "$HERE/.." && pwd)"                  # desktop/

HOST_DESKTOP="$DESKTOP"
if command -v cygpath >/dev/null 2>&1; then
    HOST_DESKTOP="$(cygpath -w "$DESKTOP")"
fi

# 1) 确保 builder 镜像与二进制就绪
if ! docker image inspect "$TAG" >/dev/null 2>&1; then
    echo ">> 构建 builder 镜像 ($PLATFORM)"
    docker build --platform "$PLATFORM" -t "$TAG" "$HERE"
fi
if [[ ! -f "$DESKTOP/${BUILD_DIR}/heritage-desktop" ]]; then
    echo ">> 容器内构建二进制 ($PLATFORM)"
    MSYS_NO_PATHCONV=1 docker run --rm --platform "$PLATFORM" \
        -e BUILD_DIR="$BUILD_DIR" \
        -v "${HOST_DESKTOP}:/src" -w /src "$TAG" \
        bash -lc 'cmake -B "$BUILD_DIR" && cmake --build "$BUILD_DIR" -j"$(nproc)"'
fi

PKG_DIR="dist/heritage-desktop-${ARCH}"

# 2) 容器内收集依赖打包
echo ">> 打包到 $PKG_DIR"
MSYS_NO_PATHCONV=1 docker run --rm --platform "$PLATFORM" \
    -e PKG_DIR="$PKG_DIR" -e BUILD_DIR="$BUILD_DIR" \
    -v "${HOST_DESKTOP}:/src" -w /src "$TAG" bash -lc '
        set -e
        BIN="$BUILD_DIR/heritage-desktop"
        DEST="$PKG_DIR"
        rm -rf "$DEST"; mkdir -p "$DEST/platforms" "$DEST/sqldrivers"
        cp "$BIN" "$DEST/heritage-desktop"

        # 传递依赖（ldd 已解析全部）
        ldd "$BIN" | grep -oE "/[^ ]+\.so[^ ]*" | sort -u | while read -r lib; do
            [ -f "$lib" ] && cp -L "$lib" "$DEST/" 2>/dev/null || true
        done

        # Qt 插件目录（平台无关查找）
        PLUG=""
        for d in /usr/lib/*/qt6/plugins /usr/lib/qt6/plugins /usr/lib/*/qt5/plugins /usr/lib/qt5/plugins; do
            [ -d "$d" ] && { PLUG="$d"; break; }
        done
        if [ -n "$PLUG" ]; then
            cp -L "$PLUG"/platforms/*.so "$DEST/platforms/" 2>/dev/null || true
            cp -L "$PLUG"/sqldrivers/*.so "$DEST/sqldrivers/" 2>/dev/null || true
            # TLS 后端插件：LLM(HTTPS) 必需
            [ -d "$PLUG/tls" ] && { mkdir -p "$DEST/tls"; cp -L "$PLUG"/tls/*.so "$DEST/tls/" 2>/dev/null || true; }
            # wayland 平台（麒麟可能走 Wayland）
            [ -d "$PLUG/wayland-platforms" ] && cp -Lr "$PLUG"/wayland-platforms "$DEST/" 2>/dev/null || true
            [ -d "$PLUG/wayland-decoration-client" ] && cp -Lr "$PLUG"/wayland-decoration-client "$DEST/" 2>/dev/null || true
            [ -d "$PLUG/wayland-shell-integration" ] && cp -Lr "$PLUG"/wayland-shell-integration "$DEST/" 2>/dev/null || true
            [ -d "$PLUG/platformthemes" ] && { mkdir -p "$DEST/platformthemes"; cp -L "$PLUG"/platformthemes/*.so "$DEST/platformthemes/" 2>/dev/null || true; }
        fi

        # 默认配置随包(config/)：磁盘优先于内嵌资源，且便于现场覆盖（与 Windows 包一致）
        mkdir -p "$DEST/config"
        cp /src/resources/config/*.json "$DEST/config/"
        cp /src/resources/style.qss "$DEST/" 2>/dev/null || true

        # 运行脚本
        cat > "$DEST/run.sh" <<"RUN"
#!/bin/bash
HERE="$(dirname "$(readlink -f "$0")")"
export QT_PLUGIN_PATH="$HERE"
export LD_LIBRARY_PATH="$HERE:${LD_LIBRARY_PATH:-}"
export QT_QPA_PLATFORM_PLUGIN_PATH="$HERE/platforms"
exec "$HERE/heritage-desktop" "$@"
RUN
        chmod +x "$DEST/run.sh"
        echo ">> 打包完成：$DEST"
        echo "-- 二进制与依赖 .so：$(ls -1 "$DEST"/*.so* 2>/dev/null | wc -l) 个 + heritage-desktop"
        echo "-- 平台插件：$(ls -1 "$DEST/platforms" 2>/dev/null | wc -l) 个"
        echo "-- SQLite 驱动：$(ls -1 "$DEST/sqldrivers" 2>/dev/null | wc -l) 个"
    '
