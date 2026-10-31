#!/usr/bin/env bash
# Builds the browser (WebAssembly) version of the virtual device into
# simulator/web/wasm/ and copies the UI asset pack next to the pages.
# Needs Emscripten: run emsdk_env first, or set EMSDK to the emsdk folder.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
FW="$ROOT/firmware/RoutineDevice/src"
SIM="$ROOT/simulator"
OUT="$SIM/build/wasm-obj"
WEB="$SIM/web"
if ! command -v em++ >/dev/null 2>&1; then
  for candidate in "${EMSDK:-}" "$HOME/emsdk" /c/emsdk; do
    if [[ -n "$candidate" && -d "$candidate/upstream/emscripten" ]]; then EMSDK="$candidate"; break; fi
  done
  if [[ -z "${EMSDK:-}" || ! -d "$EMSDK/upstream/emscripten" ]]; then echo "em++ not found: install emsdk (see simulator/README.md)"; exit 1; fi
  export EMSDK EM_CONFIG="$EMSDK/.emscripten" PATH="$EMSDK/upstream/emscripten:$PATH"
fi
EMXX=em++
mkdir -p "$OUT" "$WEB/wasm" "$WEB/assets"
FLAGS="-std=c++17 -O2 -DROUTINE_BLE_DEVELOPMENT=1 -DROUTINE_SHOWCASE=1"
SOURCES=()
for area in application content domain presentation generated; do
  for f in "$FW/$area"/*.cpp; do SOURCES+=("$f"); done
done
SOURCES+=("$FW/diagnostics/DemoSession.cpp" "$FW/platform/FileArchive.cpp")
for f in "$SIM"/src/*.cpp; do SOURCES+=("$f"); done
OBJECTS=()
for src in "${SOURCES[@]}"; do
  rel="${src#$ROOT/}"; obj="$OUT/$(echo "$rel" | tr '/' '_').o"
  OBJECTS+=("$obj")
  if [[ ! -f "$obj" || "$src" -nt "$obj" || -n "$(find "$SIM/src" "$FW" -name '*.h' -newer "$obj" -print -quit)" ]]; then
    echo "EMXX $rel"
    $EMXX $FLAGS -c "$src" -o "$obj"
  fi
done
echo "LINK simulator/web/wasm/yeso.js"
$EMXX -O2 -o "$WEB/wasm/yeso.js" "${OBJECTS[@]}" \
  -sFETCH=1 -lidbfs.js -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=64MB -sSTACK_SIZE=1MB \
  -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createYesoModule -sENVIRONMENT=web \
  -sEXPORTED_FUNCTIONS=_sim_begin,_sim_tick,_sim_buttons,_sim_command,_sim_pixels,_sim_frames,_sim_screen_on,_sim_status,_sim_console_total,_sim_console_since \
  -sEXPORTED_RUNTIME_METHODS=ccall,FS,IDBFS,HEAPU16,UTF8ToString
cp "$SIM/assets/ui.pak" "$WEB/assets/ui.pak"
echo OK
