# SPDX-License-Identifier: Apache-2.0

# Docker Hub by default. Select a mirror with --build-arg BASE_IMAGE=...
# Xuanyuan public registry (requires login): docker.xuanyuan.run/library/ubuntu:26.04
# Pass personal mirror addresses locally; do not save credentials here.
ARG BASE_IMAGE=ubuntu:26.04
FROM ${BASE_IMAGE}

ARG APT_MIRROR=http://mirrors.ustc.edu.cn/ubuntu
ARG BUILD_JOBS=2

ENV LANG=C.UTF-8 \
    LC_ALL=C.UTF-8

# Ubuntu 26.04 provides Qt >= 6.9, including Core5Compat and SerialPort.
# Keep APT signature verification enabled when using a package mirror.
RUN set -eux; \
    sed -i -E "s#https?://(archive\.ubuntu\.com/ubuntu|security\.ubuntu\.com/ubuntu)#${APT_MIRROR}#g" \
        /etc/apt/sources.list.d/ubuntu.sources; \
    apt-get -o Acquire::Retries=3 update; \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build ccache gdb git ca-certificates \
        qt6-base-dev qt6-serialport-dev qt6-5compat-dev qt6-svg-dev \
        qt6-image-formats-plugins fonts-noto-cjk fonts-dejavu-core \
        openssh-client python3; \
    rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
COPY . .

RUN cmake -S . -B /opt/serial-workbench-build -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
        -DBUILD_TESTING=OFF \
    && cmake --build /opt/serial-workbench-build \
        --target SerialWorkbench SerialWorkbenchMCP SerialWorkbenchSSHAskpass \
        --parallel "${BUILD_JOBS}"

CMD ["bash"]
