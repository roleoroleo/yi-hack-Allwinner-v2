ARG VARIANT="debian-10"
FROM mcr.microsoft.com/vscode/devcontainers/base:0-${VARIANT}

RUN export DEBIAN_FRONTEND=noninteractive               && \
    apt-get update                                      && \
    apt-get -y install --no-install-recommends             \
      git wget unzip build-essential libtool bison         \
      bisonc++ libbison-dev autoconf autotools-dev         \
      automake libssl-dev zlibc zlib1g-dev libzzip-dev     \
      flex libfl-dev yui-compressor closure-compiler       \
      optipng jpegoptim libtidy5deb1 node-less sassc       \
      sass-spec libhtml-tidy-perl libxml2-utils rsync

# Download toolchain
RUN git clone https://github.com/lindenis-org/lindenis-v536-prebuilt                                              && \
    mkdir -p /opt/yi/toolchain-sunxi-musl                                                                         && \
    cp -r lindenis-v536-prebuilt/gcc/linux-x86/arm/toolchain-sunxi-musl/toolchain /opt/yi/toolchain-sunxi-musl/

