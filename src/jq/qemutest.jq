
set -e

LIBMAIN_PATH=/opt/yi/toolchain-sunxi-musl/toolchain

echo '{"foo": 0}' | qemu-arm-static -L $LIBMAIN_PATH -E LD_LIBRARY_PATH=./_install/lib/ ./_install/bin/jq