#!/usr/bin/env bash
# 交叉编译 Windows 版：linux/amd64 容器(mingw + Qt6) → heritage-desktop.exe，
# 并收集 Qt6/插件 DLL 打包成 dist/heritage-desktop-win/，供本机或它机直接双击运行。
# 不在宿主机装任何东西。
set -euo pipefail

QT_VERSION="${QT_VERSION:-6.7.3}"
TAG="heritage-win-cross"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DESKTOP="$(cd "$HERE/.." && pwd)"

HOST_DESKTOP="$DESKTOP"
if command -v cygpath >/dev/null 2>&1; then
    HOST_DESKTOP="$(cygpath -w "$DESKTOP")"
fi

echo ">> 构建 win-cross 镜像 (Qt ${QT_VERSION})"
docker build --platform linux/amd64 -f "$HERE/Dockerfile.win-cross" -t "$TAG" \
    --build-arg QT_VERSION="$QT_VERSION" "$HERE"

echo ">> 容器内交叉编译 + 打包"
MSYS_NO_PATHCONV=1 docker run --rm --platform linux/amd64 \
    -e QT_VERSION="$QT_VERSION" \
    -v "${HOST_DESKTOP}:/src" -w /src "$TAG" bash -lc '
        set -e
        QTBIN=/opt/qt/$QT_VERSION/mingw_64/bin

        # 交叉编译关键：Qt6 因宿主(6.4.2)/目标(6.7.3)版本不一致，会让 AUTOMOC 去跑
        # 目标里的 Windows moc.exe（Linux 跑不了）。把目标的 moc/rcc/uic.exe 软链到
        # Debian 宿主版的 Linux 二进制——AUTOMOC 记录的 .exe 路径就能正常执行了。
        for t in moc rcc uic; do
            [ -f /usr/lib/qt6/libexec/$t ] && ln -sf /usr/lib/qt6/libexec/$t "$QTBIN/$t.exe"
        done

        cmake -B build-win \
            -DCMAKE_TOOLCHAIN_FILE=/src/docker/toolchain-mingw.cmake \
            -DCMAKE_PREFIX_PATH=/opt/qt/$QT_VERSION/mingw_64 \
            -DQT_HOST_PATH=/opt/qt-host \
            -DHERITAGE_BUILD_TESTS=OFF \
            -DCMAKE_BUILD_TYPE=Release
        cmake --build build-win

        echo ">> exe 架构:"; file build-win/heritage-desktop.exe 2>/dev/null || echo "PE32+ (Windows x86_64) — file 工具未装，跳过确认"

        # 打包：exe + Qt6 核心 DLL + mingw 运行时 DLL + 平台/SQLite 插件
        DEST=dist/heritage-desktop-win
        rm -rf "$DEST"; mkdir -p "$DEST/platforms" "$DEST/sqldrivers"
        cp build-win/heritage-desktop.exe "$DEST/"

        # Qt6 核心 DLL（Network=LLM, PrintSupport=QPdfWriter 报告，均由 heritage_core 引入）
        for d in Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Network Qt6PrintSupport; do
            [ -f "$QTBIN/$d.dll" ] && cp "$QTBIN/$d.dll" "$DEST/"
        done
        # mingw 运行时（Qt 官方 mingw 包自带；若缺则回退 Debian mingw 工具链）
        for rt in libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll; do
            if [ -f "$QTBIN/$rt" ]; then cp "$QTBIN/$rt" "$DEST/"
            else
                f=$(find /usr -name "$rt" 2>/dev/null | head -1)
                [ -n "$f" ] && cp "$f" "$DEST/"
            fi
        done
        # Qt 插件目录（dlopen 加载，必须随包）
        PLUG=""
        for p in /opt/qt/$QT_VERSION/mingw_64/plugins /opt/qt/$QT_VERSION/mingw_64/share/qt/plugins; do
            [ -d "$p" ] && { PLUG="$p"; break; }
        done
        [ -n "$PLUG" ] && {
            cp "$PLUG"/platforms/qwindows.dll "$DEST/platforms/" 2>/dev/null || true
            cp "$PLUG"/sqldrivers/qsqlite.dll "$DEST/sqldrivers/" 2>/dev/null || true
            # TLS 后端插件：HTTPS 必需。qschannelbackend 用 Windows 原生 Schannel（无需 OpenSSL DLL）
            mkdir -p "$DEST/tls"
            cp "$PLUG"/tls/*.dll "$DEST/tls/" 2>/dev/null || true
        }
        # 默认配置随包(config/)：磁盘优先于内嵌资源。
        # （Windows 交叉编译用宿主 rcc 6.4 生成、目标运行时 6.7 读取，rcc 数据格式不一致
        #   会导致内嵌资源损坏→JSON 解析失败；故 Windows 包必须带磁盘 config/）
        mkdir -p "$DEST/config"
        cp /src/resources/config/*.json "$DEST/config/"
        cp /src/resources/style.qss "$DEST/" 2>/dev/null || true
        echo ">> 打包完成：$DEST"
        ls -1 "$DEST" "$DEST/platforms" "$DEST/sqldrivers" "$DEST/config"
    '
