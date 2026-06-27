# 桌面版构建容器：Debian bookworm + Qt6 + CMake。
# 平台无关——由 build.sh 通过 --platform 指定 linux/amd64（开发/单测）或 linux/arm64（麒麟产物）。
# 注意：libqt6sql6-sqlite 是 Qt 的 SQLite 驱动插件，必装，否则 QSqlDatabase 打不开 QSQLITE。
FROM debian:bookworm

ARG DEBIAN_FRONTEND=noninteractive
# 换清华镜像（deb.debian.org 在国内连通性差，曾解析到 198.18.0.30 拒连）。
RUN rm -f /etc/apt/sources.list.d/debian.sources \
    && echo "deb http://mirrors.tuna.tsinghua.edu.cn/debian bookworm main" > /etc/apt/sources.list \
    && echo "deb http://mirrors.tuna.tsinghua.edu.cn/debian bookworm-updates main" >> /etc/apt/sources.list \
    && echo "deb http://mirrors.tuna.tsinghua.edu.cn/debian-security bookworm-security main" >> /etc/apt/sources.list

# Acquire::Retries：缓解镜像偶发 502/超时。
RUN apt-get update && apt-get install -y --no-install-recommends \
        -o Acquire::Retries=8 \
        -o Acquire::http::Timeout=30 \
        build-essential \
        cmake \
        qt6-base-dev \
        qt6-base-dev-tools \
        libqt6sql6-sqlite \
        file \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
CMD ["bash"]
