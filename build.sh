#!/bin/sh
# Rebuild frame.wasm (needs clang with the wasm32 target and wasm-ld).
clang --target=wasm32 -O2 -nostdlib -fno-builtin \
  -Wl,--no-entry -Wl,--strip-all -o frame.wasm src/frame.c
