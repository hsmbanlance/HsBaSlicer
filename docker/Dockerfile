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

# cmake 3.28 满足根 CMakeLists 的最低版本要求；python3 供部分 vcpkg port 使用
RUN apt-get update && apt-get install -y --no-install-recommends \
        git curl ca-certificates unzip tar zip file \
        build-essential ninja-build pkg-config \
        autoconf automake libtool patch python3 \
        cmake \
    && rm -rf /var/lib/apt/lists/*

# vcpkg：bootstrap 后按需 checkout 基线（镜像内不需要 git 历史，深度 1 即可）
RUN git clone --depth 1 https://github.com/microsoft/vcpkg.git /opt/vcpkg \
    && /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics
ENV VCPKG_ROOT=/opt/vcpkg

# rest-api 与 CMake 的 HSBA_BUILD_REST_API=ON 双门控缺一不可（顶层 CMakeLists
# 会校验 VCPKG_MANIFEST_FEATURES 中是否激活 rest-api）；默认特性（copyleft）
# 一律关闭，由 HSBA_FEATURES 显式声明所需特性
ENV VCPKG_MANIFEST_FEATURES=${HSBA_FEATURES}
ENV VCPKG_MANIFEST_NO_DEFAULT_FEATURES=1

WORKDIR /src

# 先只拷贝清单文件，依赖安装层可在源码改动后复用。
# overlay-ports/ 与 triplets/ 被 vcpkg-configuration.json 引用，随 COPY . . 进入
COPY vcpkg.json vcpkg-configuration.json CMakeLists.txt CMakePresets.json ./

RUN if [ -n "${VCPKG_BASELINE}" ]; then \
        git -C /opt/vcpkg fetch --tags --depth 1 "origin/tag:${VCPKG_BASELINE}" \
        && git -C /opt/vcpkg checkout "${VCPKG_BASELINE}"; \
    fi

COPY . .

# 配置（工具链在 cmake 调用期间触发 vcpkg 依赖编译，即本镜像最慢的一步）。
# downloads 与 binary caching 均挂到 BuildKit 缓存，跨构建复用。
RUN --mount=type=cache,target=/root/.cache/vcpkg/downloads \
    --mount=type=cache,target=/root/.cache/vcpkg/installed \
    export VCPKG_BINARY_SOURCES="clear;read,files;/root/.cache/vcpkg/installed;write,files;/root/.cache/vcpkg/installed" \
    && cmake --preset linux-release \
        -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake \
        -DCMAKE_INSTALL_PREFIX=/opt/hsba-install \
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
