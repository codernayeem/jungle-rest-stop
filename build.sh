#!/usr/bin/env bash
# Incremental parallel build (Git Bash / MSYS2). Usage: ./build.sh [clean]
set -e
cd "$(dirname "$0")"
CXX=${CXX:-g++}
FLAGS="-std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -Iinclude -Isrc"
mkdir -p build
[ "$1" == "clean" ] && rm -rf build/* && echo "cleaned"
SRCS=$(find src -name '*.cpp' -o -name '*.c' | sort)
compile() {
  src="$1"; obj="build/$(echo "$src" | sed 's#/#_#g').o"
  if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ] || [ -n "$(find src -name '*.h' -newer "$obj" 2>/dev/null | head -1)" ]; then
    if [[ "$src" == *.c ]]; then gcc -O2 -Iinclude -c "$src" -o "$obj"; else $CXX $FLAGS -c "$src" -o "$obj"; fi && echo "  compiled $src"
  fi
}
export -f compile; export CXX FLAGS
echo "$SRCS" | xargs -P 8 -I{} bash -c 'compile {}'
$CXX build/*.o -o main.exe -Llib -lglfw3 -lopengl32 -lgdi32 -lwinmm -static-libgcc -static-libstdc++
echo "built main.exe"
