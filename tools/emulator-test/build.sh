#!/bin/sh
# Builds abrun, a headless Arduboy runner on the Ardens emulator core
# (https://github.com/tiberiusbrown/Ardens). Usage:
#   git clone https://github.com/tiberiusbrown/Ardens && cd Ardens
#   git submodule update --init --depth 1 deps/miniz deps/fmt
#   cmake -B build -DARDENS_LLVM=OFF -DARDENS_DEBUGGER=OFF -DARDENS_PLAYER=OFF -DARDENS_FLASHCART=OFF -DARDENS_SDL=OFF -DARDENS_LIB=ON
#   make -C build ardenslib          # builds fmt and miniz
#   ARDENS=$PWD sh path/to/build.sh
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$ARDENS"
mkdir -p "$HERE/obj"
for s in src/absim_*.cpp src/boot/*.c deps/yyjson/yyjson.c; do
  o="$HERE/obj/$(basename $s).o"
  case $s in
    *.c) gcc -O2 -c $s -o "$o" -Ideps -Isrc ;;
    *) g++ -std=c++17 -O2 -w -c $s -o "$o" -Isrc -Ideps -Ideps/miniz -Ideps/bitsery/include -Ideps/fmt/include -Ibuild/deps/miniz \
         -DARDENS_NO_GUI -DARDENS_NO_SAVED_SETTINGS -DARDENS_NO_SCALING -DARDENS_VERSION_MAJOR=0 -DARDENS_VERSION_MINOR=0 -DARDENS_VERSION_PATCH=0 ;;
  esac
done
g++ -std=c++17 -O2 -w "$HERE/abrun.cpp" "$HERE"/obj/*.o -o "$HERE/abrun" -Isrc -Ideps -Ideps/bitsery/include -Ideps/fmt/include \
  build/deps/miniz/libminiz.a build/deps/fmt/libfmt.a -lpthread
echo built "$HERE/abrun"
