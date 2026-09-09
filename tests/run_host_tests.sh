#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
test_build_dir="$(mktemp -d "${TMPDIR:-/tmp}/vaydenet-tests.XXXXXX")"
trap 'rm -rf "$test_build_dir"' EXIT

cxx="${CXX:-clang++}"
common_flags=(
    -std=c++17
    -Wall
    -Wextra
    -Werror
    -fsanitize=address,undefined
    -fno-omit-frame-pointer
)

"$cxx" \
    "${common_flags[@]}" \
    -I"$repo_root/packages/VaydeEngine/include" \
    "$repo_root/tests/unit/packet_layout_test.cpp" \
    -o "$test_build_dir/packet_layout_test"
"$test_build_dir/packet_layout_test"
echo "PASS: Packet layout"

"$cxx" \
    "${common_flags[@]}" \
    -I"$repo_root/packages/VaydeEngine/include" \
    "$repo_root/packages/VaydeEngine/src/VaydeEngine.cpp" \
    "$repo_root/tests/unit/vayde_engine_receive_consumer_test.cpp" \
    -o "$test_build_dir/vayde_engine_receive_consumer_test"
"$test_build_dir/vayde_engine_receive_consumer_test"

"$cxx" \
    "${common_flags[@]}" \
    -I"$repo_root/tests/mocks/esp-idf" \
    -I"$repo_root/packages/adapters/esp-now" \
    -I"$repo_root/packages/VaydeEngine/include" \
    "$repo_root/packages/adapters/esp-now/EspNowTransport.cpp" \
    "$repo_root/tests/unit/esp_now_receive_queue_test.cpp" \
    -o "$test_build_dir/esp_now_receive_queue_test"
"$test_build_dir/esp_now_receive_queue_test"
