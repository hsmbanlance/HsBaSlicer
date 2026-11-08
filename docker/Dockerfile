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
FROM ubuntu:24.04 AS builder

ARG VCPKG_BASELINE=""
ARG HSBA_FEATURES="rest-api"
ARG JOBS=""

ENV DEBIAN_FRONTEND=noninteractive

# 系统依赖清单与 .github/workflows/build-linux.yml 逐项对齐：那是同一 vcpkg 依赖图
# 长期验证过的集合。gperf/gtk/opencv 等 port 走 autoreconf/meson 路径，最小镜像
# 缺包会 BUILD_FAILED 并连锁打断 project()（伪装成 Ninja/编译器缺失的次生错误）。
# PowerShell 为顶层 version/CMakeLists.txt 配置阶段的硬性要求（Generator_Version.ps1）。
RUN apt-get update && apt-get install -y \
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
        apt-transport-https software-properties-common \
    && wget -q https://packages.microsoft.com/config/ubuntu/24.04/packages-microsoft-prod.deb \
    && dpkg -i packages-microsoft-prod.deb && rm packages-microsoft-prod.deb \
    && apt-get update && apt-get install -y powershell \
    && rm -rf /var/lib/apt/lists/*

# vcpkg：bootstrap 后按需 checkout 基线（镜像内不需要 git 历史，深度 1 即可）。
# 注意 vcpkg 基线是 commit SHA 而非 tag，GitHub 支持按完整 SHA 直取对象。
RUN git clone --depth 1 https://github.com/microsoft/vcpkg.git /opt/vcpkg \
    && /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics
ENV VCPKG_ROOT=/opt/vcpkg

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

COPY . .

# 配置（工具链在 cmake 调用期间触发 vcpkg 依赖编译，即本镜像最慢的一步）。
# downloads 与 binary caching 均挂到 BuildKit 缓存，跨构建复用。
# 注意：VCPKG_BINARY_SOURCES 环境变量中同一 source 的各字段用逗号分隔，
# 多个 source 之间才用分号（与 --binarysource 命令行全分号不同）。
RUN --mount=type=cache,target=/root/.cache/vcpkg/downloads \
    --mount=type=cache,target=/root/.cache/vcpkg/installed \
    export VCPKG_BINARY_SOURCES="clear;files,/root/.cache/vcpkg/installed,readwrite" \
    && cmake --preset linux-release \
        -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake \
        -DCMAKE_INSTALL_PREFIX=/opt/hsba-install \
        -DVCPKG_MANIFEST_FEATURES="${HSBA_FEATURES}" \
        -DVCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON \
        -DHSBA_BUILD_REST_API=ON \
        -DHSBA_SLICER_BUILD_SAMPLES=OFF \
        -DHSBA_SLICER_USE_TESTS=OFF \
        -DHSBA_SLICER_USE_STATIC_TESTS=OFF \
        -DHSBA_SLICER_BUILD_DOCS=OFF \
    && cmake --build --preset linux-release \
        --target HsBaSlicerRestServer \
        --parallel "${JOBS:-0}" \
    && cmake --install out/build/linux-release

# ===========================================================================
# Stage 2: runtime — 仅安装产物 + 少量系统运行库
# ===========================================================================
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# DllHsBaSlicer 在 Linux 下为 SHARED，vcpkg 静态库已链入其中；
# 这里只补齐动态端口的运行时依赖（ssl/crypto/z 随镜像架构，其余显式安装）
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl \
        libssl3 zlib1g libpng16-16t64 libfontconfig1 libfreetype6 \
        libmysqlclient21 libpq5 sqlite3 \
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
