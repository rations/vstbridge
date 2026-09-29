# Shared helpers for the aarch64 build scripts. Source versions.sh first.

# Download and unpack the aarch64-hosted llvm-mingw toolchain into "$1", then add it to PATH.
# Setting $LLVM_MINGW to an unpacked llvm-mingw uses that one instead.
fetch_llvm_mingw() {
    local dir="$1"
    local name="llvm-mingw-${LLVM_MINGW_VERSION}-ucrt-ubuntu-22.04-aarch64"

    if [ -n "${LLVM_MINGW:-}" ]; then
        export PATH="$LLVM_MINGW/bin:$PATH"
        return
    fi

    if [ ! -x "$dir/$name/bin/clang" ]; then
        mkdir -p "$dir"
        curl -fsSL -o "$dir/$name.tar.xz" \
            "https://github.com/mstorsjo/llvm-mingw/releases/download/${LLVM_MINGW_VERSION}/$name.tar.xz"
        echo "$LLVM_MINGW_SHA256_AARCH64  $dir/$name.tar.xz" | sha256sum -c -
        tar -C "$dir" -xJf "$dir/$name.tar.xz"
        rm "$dir/$name.tar.xz"
    fi

    export PATH="$dir/$name/bin:$PATH"
}
