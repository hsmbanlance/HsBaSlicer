# syntax=docker/dockerfile:1
# ---------------------------------------------------------------------------
# HsBaSlicerRestServer 容器镜像（Linux/amd64）
#
# 本地构建（build 上下文必须是仓库根目录；BuildKit 下自动应用同目录的
# Dockerfile.dockerignore）：
#   docker build -f docker/Dockerfile -t hsba-rest-api:latest .
# 运行：
#   docker run -d -p 8080:8080 -e HSBA_REST_AUTH_TOKEN=<token> hsba-rest-api:latest
#
# 构建投入（ARG）：
#   HSBA_FEATURES  默认 "rest-api"（MIT 许可最小构建，CI 可接受）。
#                  设为 "rest-api;copyleft" 启用 CGAL/OCCT 内核，但镜像层需
#                  从源码编译 OCCT/CGAL 且分发即触发 GPL-3.0-or-later convey
#                  义务。本文件强制 VCPKG_MANIFEST_NO_DEFAULT_FEATURES=1，
#                  copyleft 默认特性仅由 HSBA_FEATURES 显式开关（依赖清单
#                  本身不改动）。
#   VCPKG_BASELINE 可选 vcpkg git 基线覆盖；缺省沿用 vcpkg-configuration.json
#                  中已钉定的 default-registry baseline。
#
# 冷构建需从源码编译 opencv/openvdb/protobuf 等重型依赖，耗时以小时计；
# vcpkg 下载与 binary caching 已挂 BuildKit cache，跨构建复用。
# CI 验证入口：.github/workflows/docker-rest-api.yml
# ---------------------------------------------------------------------------

# ===========================================================================
# Stage 1: builder — vcpkg 依赖编译 + CMake 构建 + 安装到 /opt/hsba-install
# ===========================================================================
# 不钉住 ubuntu 版本：跟随 ubuntu:latest，避免本地/CI 重复拉取已被 digest 解析钉死的基础镜像；
# PowerShell 用与 Ubuntu 大版本无关的通用 deb 包安装（见 Microsoft 安装文档）
FROM ubuntu AS builder

ARG VCPKG_BASELINE=""
ARG HSBA_FEATURES="rest-api"
ARG JOBS=""
# 可选 apt 镜像源（如 https://mirrors.tuna.tsinghua.edu.cn/ubuntu/），默认官方源；
# 仅当传入时重写 deb822 sources，适用于 Docker Hub/apt 大索引断流的受限网络环境
ARG APT_MIRROR=""
# PowerShell 按 Microsoft 安装文档的"手动下载通用 deb 包"方式安装：从 GitHub
# releases 直取，不依赖 packages.microsoft.com 按版本仓库（受限网络下该源索引
# 经常刷不出来，apt 会静默跳过导致 Unable to locate package powershell）。
# LTS 版本号可直接覆盖
ARG POWERSHELL_DEB_URL="https://github.com/PowerShell/PowerShell/releases/download/v7.6.6/powershell_7.6.6-1.deb_amd64.deb"

ENV DEBIAN_FRONTEND=noninteractive

# 系统依赖清单与 .github/workflows/build-linux.yml 逐项对齐：那是同一 vcpkg 依赖图
# 长期验证过的集合。gperf/gtk/opencv 等 port 走 autoreconf/meson 路径，最小镜像
# 缺包会 BUILD_FAILED 并连锁打断 project()（伪装成 Ninja/编译器缺失的次生错误）。
# PowerShell 为顶层 version/CMakeLists.txt 配置阶段的硬性要求（Generator_Version.ps1）。
# 可选 apt 镜像源重写（deb822：archive/security 均指向镜像）；留空则不改动
RUN if [ -n "${APT_MIRROR}" ]; then \
        sed -i "s|http://archive.ubuntu.com/ubuntu/|${APT_MIRROR}|g; s|http://security.ubuntu.com/ubuntu/|${APT_MIRROR}|g" \
            /etc/apt/sources.list.d/ubuntu.sources; \
    fi \
    && apt-get update -o Acquire::Retries=10 && apt-get install -y -o Acquire::Retries=10 \
        git curl wget zip unzip tar ca-certificates file at \
        cmake ninja-build pkg-config \
        build-essential g++ gfortran gperf \
        python3 python3-pip python3-venv python3-setuptools python3-mako \
        python3-jinja2 python-is-python3 \
        autoconf automake libtool libtool-bin libltdl-dev dh-autoreconf \
        autoconf-archive xutils-dev patch bison flex libbison-dev \
        meson nasm yasm ruby-full nodejs \
        libncurses-dev libncurses5-dev libtirpc-dev libudev-dev libkrb5-dev \
        libdbus-1-dev libfontconfig1-dev \
        libx11-dev libx11-xcb-dev libxext-dev libxfixes-dev libxrender-dev \
        libxrandr-dev libxtst-dev libxi-dev libxxf86vm-dev libxcursor-dev \
        libxinerama-dev libxdamage-dev libxkbcommon-dev libxkbcommon-x11-dev \
        libwayland-dev wayland-protocols libxt-dev libxaw7-dev \
        libgl1-mesa-dev libglu1-mesa-dev libgles2-mesa-dev libegl1-mesa-dev \
        mesa-common-dev \
        libxcb1-dev libxcb-dri3-dev libxcb-present-dev libxcb-glx0-dev \
        libxcb-util0-dev libxcb-keysyms1-dev libxcb-image0-dev libxcb-shm0-dev \
        libxcb-icccm4-dev libxcb-sync-dev libxcb-xfixes0-dev libxcb-shape0-dev \
        libxcb-randr0-dev libxcb-render-util0-dev libxcb-xinerama0-dev \
        libxcb-xkb-dev libxcb-xinput-dev libxcb-cursor-dev libxcb-res0-dev \
        libxcb-record0-dev \
        libnuma1 libnuma-dev cifs-utils guile-2.2-dev haskell-stack golang-go \
        libbluetooth-dev \
        apt-transport-https software-properties-common

# PowerShell 独立成层：本层失败重跑只重做本层，不触发上面几百个系统包重新下载。
# apt 索引保留到本层末尾再统一清理，供 fix-broken 解析 deb 的依赖
RUN wget -q --tries=10 "${POWERSHELL_DEB_URL}" \
    && ( dpkg -i powershell_*.deb || apt-get install -y -o Acquire::Retries=10 -f ) \
    && rm -f powershell_*.deb \
    && rm -rf /var/lib/apt/lists/*

# vcpkg：bootstrap 后按需 checkout 基线（镜像内不需要 git 历史，深度 1 即可）。
# 注意 vcpkg 基线是 commit SHA 而非 tag，GitHub 支持按完整 SHA 直取对象。
RUN git clone --depth 1 https://github.com/microsoft/vcpkg.git /opt/vcpkg \
    && /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics
# vcpkg 下载缓存默认在 VCPKG_ROOT/downloads（不在任何 cache mount 内），显式指向
# BuildKit cache：工具链 tar 包与端口源码归档跨构建/断点重跑可复用
ENV VCPKG_ROOT=/opt/vcpkg \
    VCPKG_DOWNLOADS=/root/.cache/vcpkg/downloads

# rest-api 与 CMake 的 HSBA_BUILD_REST_API=ON 双门控缺一不可（顶层 CMakeLists
# 会校验 ENV 或 CMake 变量中的 rest-api 特性）。
# 注意：vcpkg.cmake 工具链只读 CMake 变量 VCPKG_MANIFEST_FEATURES /
# VCPKG_MANIFEST_NO_DEFAULT_FEATURES（拼成 --x-feature/--x-no-default-features），
# 不读同名环境变量；ENV 仅满足顶层门控，特性开关必须靠下方 -D 传入，
# 否则 default-features(copyleft) 照装而 rest-api(cpp-httplib) 缺失。
ENV VCPKG_MANIFEST_FEATURES=${HSBA_FEATURES}
ENV VCPKG_MANIFEST_NO_DEFAULT_FEATURES=1

WORKDIR /src

# 先只拷贝清单文件，依赖安装层可在源码改动后复用。
# overlay-ports/ 与 triplets/ 被 vcpkg-configuration.json 引用，随 COPY . . 进入
COPY vcpkg.json vcpkg-configuration.json CMakeLists.txt CMakePresets.json ./

RUN if [ -n "${VCPKG_BASELINE}" ]; then \
        git -C /opt/vcpkg fetch --depth 1 origin "${VCPKG_BASELINE}" \
        && git -C /opt/vcpkg checkout "${VCPKG_BASELINE}"; \
    fi

# 预热 vcpkg-tool 自带的 cmake/ninja：vcpkg 内置下载器对连接失败不重试（curl
# error 7 直接判定非瞬态错误），这里先用系统 wget（--tries 重试 + -c 断点续传）
# 拉到 downloads 缓存；vcpkg 检测到缓存文件后直接使用并自行 sha512 校验（完整性
# 不绕过）。版本从 checkout 后的 vcpkg-tool-metadata.txt 读取，基线变化后本层
# 自然重跑；预热失败不阻断构建，由下方 configure 重试循环兜底。
RUN --mount=type=cache,target=/root/.cache/vcpkg/downloads \
    meta=/opt/vcpkg/scripts/vcpkg-tool-metadata.txt; \
    cmake_ver=$(awk -F= '/^CMAKE_VERSION=/{print $2; exit}' "$meta"); \
    ninja_ver=$(awk -F= '/^NINJA_VERSION=/{print $2; exit}' "$meta"); \
    echo ">>> pre-warming vcpkg tools: cmake ${cmake_ver}, ninja ${ninja_ver}"; \
    wget -c -q --tries=15 --timeout=60 \
        -O "${VCPKG_DOWNLOADS}/cmake-${cmake_ver}-linux-x86_64.tar.gz.part" \
        "https://github.com/Kitware/CMake/releases/download/v${cmake_ver}/cmake-${cmake_ver}-linux-x86_64.tar.gz" \
    && mv "${VCPKG_DOWNLOADS}/cmake-${cmake_ver}-linux-x86_64.tar.gz.part" \
          "${VCPKG_DOWNLOADS}/cmake-${cmake_ver}-linux-x86_64.tar.gz" \
    || echo "WARNING: cmake pre-warm failed; configure retries will re-attempt"; \
    wget -c -q --tries=15 --timeout=60 \
        -O "${VCPKG_DOWNLOADS}/ninja-linux.zip.part" \
        "https://github.com/ninja-build/ninja/releases/download/v${ninja_ver}/ninja-linux.zip" \
    && mv "${VCPKG_DOWNLOADS}/ninja-linux.zip.part" \
          "${VCPKG_DOWNLOADS}/ninja-linux.zip" \
    || echo "WARNING: ninja pre-warm failed; configure retries will re-attempt"

COPY . .

# 配置（工具链在 cmake 调用期间触发 vcpkg 依赖编译，即本镜像最慢的一步）。
# downloads 与 binary caching 均挂到 BuildKit 缓存，跨构建复用。
# vcpkg 对连接失败不重试（curl error 7 判为非瞬态错误），外层 shell 最多重试
# 5 次，重试间清理二进制目录避免陈旧 CMakeCache 条目干扰。
# 注意：VCPKG_BINARY_SOURCES 环境变量中同一 source 的各字段用逗号分隔，
# 多个 source 之间才用分号（与 --binarysource 命令行全分号不同）。
RUN --mount=type=cache,target=/root/.cache/vcpkg/downloads \
    --mount=type=cache,target=/root/.cache/vcpkg/installed \
    export VCPKG_BINARY_SOURCES="clear;files,/root/.cache/vcpkg/installed,readwrite" \
    && ok=0 \
    && for attempt in 1 2 3 4 5; do \
        echo ">>> CMake configure attempt ${attempt}/5"; \
        if cmake --preset linux-release \
            -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake \
            -DCMAKE_INSTALL_PREFIX=/opt/hsba-install \
            -DVCPKG_MANIFEST_FEATURES="${HSBA_FEATURES}" \
            -DVCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON \
            -DHSBA_BUILD_REST_API=ON \
            -DHSBA_SLICER_BUILD_SAMPLES=OFF \
            -DHSBA_SLICER_USE_TESTS=OFF \
            -DHSBA_SLICER_USE_STATIC_TESTS=OFF \
            -DHSBA_SLICER_BUILD_DOCS=OFF; \
        then ok=1; break; fi; \
        rm -rf out/build/linux-release; \
        [ "${attempt}" = 5 ] || { echo ">>> configure attempt ${attempt} failed; retrying in 30s"; sleep 30; }; \
    done; \
    [ "${ok}" = 1 ] || { echo "ERROR: CMake configure failed after 5 attempts"; exit 1; }; \
    cmake --build --preset linux-release \
        --target HsBaSlicerRestServer \
        --parallel "${JOBS:-$(nproc)}" \
    && cmake --install out/build/linux-release

# ===========================================================================
# Stage 2: runtime — 仅安装产物 + 少量系统运行库
# ===========================================================================
FROM ubuntu AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# DllHsBaSlicer 在 Linux 下为 SHARED，vcpkg 静态库已链入其中；
# 这里只补齐动态端口的运行时依赖（ssl/crypto/z 随镜像架构，其余显式安装）
# 运行库包名跟随当前 ubuntu:latest（26.04 resolute：libssl3t64/libmysqlclient24）；
# 基础镜像大版本升级时需复查这几个 ABI 包名
RUN if [ -n "${APT_MIRROR}" ]; then \
        sed -i "s|http://archive.ubuntu.com/ubuntu/|${APT_MIRROR}|g; s|http://security.ubuntu.com/ubuntu/|${APT_MIRROR}|g" \
            /etc/apt/sources.list.d/ubuntu.sources; \
    fi \
    && apt-get update -o Acquire::Retries=10 && apt-get install -y -o Acquire::Retries=10 --no-install-recommends \
        ca-certificates curl \
        libssl3t64 zlib1g libpng16-16t64 libfontconfig1 libfreetype6 \
        libmysqlclient24 libpq5 sqlite3 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --create-home --shell /usr/sbin/nologin hsba

# 安装产物：bin/（HsBaSlicerRestServer + libDllHsBaSlicer.so，CMake install
# 已修正 RPATH）、lib/、etc/HsBaSlicer/restapi.ini 及公开头文件
COPY --from=builder /opt/hsba-install/ /usr/local/

WORKDIR /app
# 服务端按当前工作目录读取 restapi.ini 与 logcfg.ini；缺失虽有内置默认值，
# 但默认 host=127.0.0.1 无法从容器外访问，故覆盖为 0.0.0.0
COPY --from=builder /src/restapi/restapi.ini ./restapi.ini
COPY --from=builder /src/utils/logcfg.ini ./logcfg.ini
RUN sed -i 's/^host = 127.0.0.1/host = 0.0.0.0/' ./restapi.ini \
    && mkdir -p /app/data && chown -R hsba:hsba /app
USER hsba

# 作业/上传数据目录（SQLite 参数库、upload 池均落在工作目录），
# 生产部署建议挂载卷：-v hsba-data:/app/data
ENV HSBA_REST_HOST=0.0.0.0 \
    HSBA_REST_PORT=8080 \
    PATH=/usr/local/bin:${PATH} \
    LD_LIBRARY_PATH=/usr/local/lib

EXPOSE 8080

# 环回绑定按设计跳过鉴权，健康探针无需 token（见 restapi.ini [auth] 注释）
HEALTHCHECK --interval=30s --timeout=5s --start-period=60s --retries=3 \
    CMD curl -fsS http://127.0.0.1:${HSBA_REST_PORT}/api/health || exit 1

# --host/--port 命令行优先级高于 ini；HSBA_REST_AUTH_TOKEN /
# HSBA_REST_TLS_CERT / HSBA_REST_TLS_KEY 由 main.cpp 环境回退读取，
# 机密不落入镜像
ENTRYPOINT ["/bin/sh", "-c", \
    "exec /usr/local/bin/HsBaSlicerRestServer --config ./restapi.ini --host \"${HSBA_REST_HOST}\" --port \"${HSBA_REST_PORT}\" $@"]
