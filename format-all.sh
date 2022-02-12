#!/usr/bin/env bash

[[ -f .clang-format ]] || {
  printf 'Missing .clang-format file'
  exit 1
}

find main -maxdepth 1 \( -name '*.cpp' -or -name '*.c' -or -name '*.h' \) \
  -print -exec clang-format --style=file -i {} +
