ARG VARIANT="debian-11"
FROM mcr.microsoft.com/vscode/devcontainers/base:0-${VARIANT}

RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends \
    build-essential \
    git \
    wget \
    curl \
    rsync \
    unzip \
    bzip2 \
    file \
    qemu-user-static \
    pkg-config

# Download toolchain
RUN git clone https://github.com/lindenis-org/lindenis-v536-prebuilt                                              && \
    mkdir -p /opt/yi/toolchain-sunxi-musl                                                                         && \
    cp -r lindenis-v536-prebuilt/gcc/linux-x86/arm/toolchain-sunxi-musl/toolchain /opt/yi/toolchain-sunxi-musl/

