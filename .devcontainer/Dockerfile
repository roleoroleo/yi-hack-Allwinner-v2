ARG VARIANT="debian-11"
FROM mcr.microsoft.com/vscode/devcontainers/base:0-${VARIANT}

# Debian 11 LTS ended in Aug 2026: bullseye-security packages are only on archive.debian.org now (deb.debian.org returns 404)
RUN export DEBIAN_FRONTEND=noninteractive               && \
    sed -i "s#http://deb.debian.org/debian-security#http://archive.debian.org/debian-security#" /etc/apt/sources.list && \
    apt-get update                                      && \
    apt-get -y install --no-install-recommends             \
      git wget unzip build-essential libtool bison         \
      bisonc++ libbison-dev autoconf autotools-dev         \
      automake libssl-dev zlib1g-dev libzzip-dev           \
      flex libfl-dev yui-compressor closure-compiler       \
      optipng jpegoptim libtidy5deb1 node-less sassc       \
      sass-spec libhtml-tidy-perl libxml2-utils rsync cmake \
      qemu-user-static pkg-config curl bzip2 file

# Download toolchain
RUN git clone https://github.com/lindenis-org/lindenis-v536-prebuilt                                              && \
    mkdir -p /opt/yi/toolchain-sunxi-musl                                                                         && \
    cp -r lindenis-v536-prebuilt/gcc/linux-x86/arm/toolchain-sunxi-musl/toolchain /opt/yi/toolchain-sunxi-musl/

