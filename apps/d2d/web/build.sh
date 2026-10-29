#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Builds d2d for the browser into build-web/apps/d2d (index.html, d2d.js,
# d2d.wasm, coi-serviceworker.js, touch.js): a folder any static host can serve.
#
# Needs an activated Emscripten SDK (source emsdk_env.sh), git, cmake and
# ninja. SDL3, zlib and bzip2 are built here with -pthread, as d2d's
# threads (Web Workers) need every object built that way.
#   apps/d2d/web/build.sh
#   cd build-web/apps/d2d && python3 -m http.server   # http://localhost:8000
set -euo pipefail

root="$(cd "$(dirname "$0")/../../.." && pwd)"
build="${D2D_WEB_BUILD:-$root/build-web}"
deps="$build/deps"
prefix="$build/prefix"
flags=(-G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_C_FLAGS=-pthread)

command -v emcmake >/dev/null || { echo "build.sh: activate the Emscripten SDK first (emsdk_env.sh)" >&2; exit 1; }
mkdir -p "$deps"

fetch() {   # name, url, ref
    [[ -d "$deps/$1" ]] || git clone -q --depth 1 --branch "$3" "$2" "$deps/$1"
}

if [[ ! -f "$prefix/lib/libz.a" ]]; then
    fetch zlib https://github.com/madler/zlib.git v1.3.1
    emcmake cmake -S "$deps/zlib" -B "$deps/zlib/build" "${flags[@]}"
    cmake --build "$deps/zlib/build" && cmake --install "$deps/zlib/build"
    rm -f "$prefix"/lib/libz.so*   # Emscripten's stand-in .so would win over libz.a
fi
if [[ ! -f "$prefix/lib/libbz2.a" ]]; then
    fetch bzip2 https://gitlab.com/bzip2/bzip2.git bzip2-1.0.8
    # 1.0.8 has no CMake build: compile the library's sources directly.
    (cd "$deps/bzip2" && emcc -O2 -pthread -c blocksort.c huffman.c crctable.c randtable.c compress.c decompress.c bzlib.c \
        && emar rcs libbz2.a blocksort.o huffman.o crctable.o randtable.o compress.o decompress.o bzlib.o)
    mkdir -p "$prefix/lib" "$prefix/include"
    cp "$deps/bzip2/libbz2.a" "$prefix/lib/" && cp "$deps/bzip2/bzlib.h" "$prefix/include/"
fi
if [[ ! -f "$prefix/lib/libSDL3.a" ]]; then
    fetch SDL https://github.com/libsdl-org/SDL.git release-3.2.24
    emcmake cmake -S "$deps/SDL" -B "$deps/SDL/build" "${flags[@]}" \
        -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF
    cmake --build "$deps/SDL/build" && cmake --install "$deps/SDL/build"
fi

emcmake cmake -S "$root" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$prefix" -DCMAKE_FIND_ROOT_PATH="$prefix" -DSDL3_DIR="$prefix/lib/cmake/SDL3" \
    -DD2D_BUILD_TOOLS=OFF -DD2D_BUILD_TESTS=OFF -DBUILD_TESTING=OFF
cmake --build "$build" --target d2d
echo "Built: $build/apps/d2d (index.html, d2d.js, d2d.wasm, coi-serviceworker.js, touch.js)"
