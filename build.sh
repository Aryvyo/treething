
#!/usr/bin/env bash
set -euo pipefail

SRC="${1:-src/main.c}"
OUT="${2:-main}"


if [[ -n "${XDG_SHELL_XML:-}" ]]; then
  XML="$XDG_SHELL_XML"
else
  PROTO_DIR="$(pkg-config --variable=pkgdatadir wayland-protocols)"
  XML="$PROTO_DIR/stable/xdg-shell/xdg-shell.xml"
fi
[[ -f "$XML" ]] || { echo "xdg-shell.xml not found at: $XML"; exit 1; }

mkdir -p build
wayland-scanner client-header "$XML" build/xdg-shell-client-protocol.h
wayland-scanner private-code  "$XML" build/xdg-shell-protocol.c

zig cc "$SRC" build/xdg-shell-protocol.c -o "$OUT" $(pkg-config --cflags --libs wayland-client)

echo "OK → $OUT"
