#!/usr/bin/env bash
# Builds the virtual device host. Firmware sources are compiled unchanged;
# only simulator/src replaces firmware/RoutineDevice/src/platform/esp32.
set -euo pipefail
export PATH=/c/msys64/ucrt64/bin:$PATH
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
FW="$ROOT/firmware/RoutineDevice/src"
SIM="$ROOT/simulator"
OUT="$SIM/build/obj"
mkdir -p "$OUT"
CXX=${CXX:-g++}
FLAGS="-std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -DROUTINE_BLE_DEVELOPMENT=1 -DROUTINE_SHOWCASE=1"
SOURCES=()
for area in application content domain presentation generated; do
  for f in "$FW/$area"/*.cpp; do SOURCES+=("$f"); done
done
SOURCES+=("$FW/diagnostics/DemoSession.cpp" "$FW/platform/FileArchive.cpp")
for f in "$SIM"/src/*.cpp; do SOURCES+=("$f"); done
OBJECTS=()
pids=()
for src in "${SOURCES[@]}"; do
  rel="${src#$ROOT/}"; obj="$OUT/$(echo "$rel" | tr '/' '_' ).o"
  OBJECTS+=("$obj")
  if [[ ! -f "$obj" || "$src" -nt "$obj" || -n "$(find "$SIM/src" "$FW" -name '*.h' -newer "$obj" -print -quit)" ]]; then
    echo "CXX $rel"
    $CXX $FLAGS -c "$src" -o "$obj" &
    pids+=($!)
    if (( ${#pids[@]} >= 6 )); then wait "${pids[0]}"; pids=("${pids[@]:1}"); fi
  fi
done
for p in "${pids[@]}"; do wait "$p"; done
echo "LINK simulator/build/yeso-sim.exe"
$CXX -static -o "$SIM/build/yeso-sim.exe" "${OBJECTS[@]}" -lws2_32 -lwinhttp
echo OK
