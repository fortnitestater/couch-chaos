#!/usr/bin/env bash
# Builds the Couch Chaos native games (C++ / raylib 5.5) to WebAssembly.
# Needs: ~/emsdk (activated "latest") and ~/raylib (tag 5.5).  Output: ../native-games.js + ../native-games.wasm
set -e
cd "$(dirname "$0")"
export EMSDK=~/emsdk EM_CONFIG=~/emsdk/.emscripten EMSDK_PYTHON=~/emsdk/python/3.13.3_64bit/python.exe
export PATH="$HOME/emsdk/upstream/emscripten:$HOME/emsdk/python/3.13.3_64bit:$HOME/emsdk/node/24.19.0_64bit/bin:$PATH"
RL=~/raylib/src
mkdir -p build
if [ ! -f build/libraylib.a ]; then
  echo "building raylib for web..."
  for f in rcore rshapes rtextures rtext rmodels utils raudio; do
    emcc -c $RL/$f.c -o build/$f.o -Os -Wall -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 -I$RL -I$RL/external/glfw/include &
  done
  wait
  emar rcs build/libraylib.a build/*.o
fi
echo "building games..."
em++ src/*.cpp -o ../native-games.js -std=c++17 -O2 -I$RL -Lbuild -lraylib \
  -DPLATFORM_WEB -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1 -sMODULARIZE=1 -sEXPORT_NAME=CCNative \
  -sEXPORTED_FUNCTIONS=_cc_init,_cc_start,_cc_input,_cc_act,_cc_frame,_cc_stop,_cc_shell,_cc_shell_pick,_cc_shell_frame,_cc_dbg,_cc_set_scale,_cc_put_file,_cc_emote,_malloc,_free,_main \
  -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,HEAPU8 -sENVIRONMENT=web -sFILESYSTEM=0 -sGL_ENABLE_GET_PROC_ADDRESS=1
ls -la ../native-games.*
