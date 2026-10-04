#!/usr/bin/env bash
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"

winpath() {
  case "$1" in
    /[a-zA-Z]/*) printf '%s' "$1" | sed -E 's|^/([a-zA-Z])/|\1:/|' ;;
    *) printf '%s' "$1" ;;
  esac
}

NDK="${NDK:-$ROOT/.toolchain/android-ndk-r27c}"
API="${API:-30}"

HOST_TAG=""
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*) HOST_TAG=windows-x86_64 ;;
  Linux)                HOST_TAG=linux-x86_64 ;;
  Darwin)               HOST_TAG=darwin-x86_64 ;;
  *) echo "unsupported host: $(uname -s)" >&2; exit 1 ;;
esac

BIN="$NDK/toolchains/llvm/prebuilt/$HOST_TAG/bin"
CXX="$BIN/aarch64-linux-android${API}-clang++"

tool() {
  if [ -x "$BIN/$1.exe" ]; then printf '%s' "$BIN/$1.exe"; else printf '%s' "$BIN/$1"; fi
}
STRIP="$(tool llvm-strip)"
READELF="$(tool llvm-readelf)"

if [ ! -x "$CXX" ]; then
  echo "cross compiler not found: $CXX" >&2
  exit 1
fi

SRCS="gl_util.cpp fdset.cpp sync.cpp kprobe_leak.cpp cred_patch.cpp \
      profile.cpp runner.cpp payload.cpp"

OUT="$HERE/build-ndk"
mkdir -p "$OUT"

echo "[*] compiler: $CXX"
for f in $SRCS; do
  echo "[*] cc  $f"
  "$CXX" -std=c++17 -O2 -fPIC -fvisibility=hidden -fno-exceptions -fno-rtti \
    -Wall -Wextra \
    -pthread -c "$(winpath "$HERE/$f")" -o "$(winpath "$OUT/so_${f%.cpp}.o")"
done

echo "[*] link (shared)"
OBJS=""
for f in $SRCS; do OBJS="$OBJS $(winpath "$OUT/so_${f%.cpp}.o")"; done
"$CXX" -shared -o "$(winpath "$OUT/libghostlock.so")" $OBJS -pthread
"$STRIP" "$(winpath "$OUT/libghostlock.so")"

echo "[+] built $OUT/libghostlock.so"
"$READELF" -h "$(winpath "$OUT/libghostlock.so")" 2>/dev/null \
  | grep -E "Class|Machine|Type" || true
"$READELF" --dyn-syms "$(winpath "$OUT/libghostlock.so")" 2>/dev/null \
  | grep gl_payload_main || true
