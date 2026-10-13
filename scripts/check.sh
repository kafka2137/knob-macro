#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
node --check dist/app.mjs
node --check dist/protocol.mjs
node --check dist/keycodes.mjs
node --check scripts/prepare-pages.mjs
node protocol.test.mjs
scratch_dir=$(mktemp -d)
trap 'rm -rf "$scratch_dir"' EXIT
for debounce in 200 350 0; do
  cc -std=gnu11 -Wall -Wextra -Werror -include firmware/kafka/config.h \
    -DKAFKA_NORMAL_DEBOUNCE_MS="$debounce" -DQMK_KEYBOARD_H='"qmk_mock.h"' \
    -Ifirmware/tests -Ifirmware/kafka firmware/tests/deferred_text_test.c \
    firmware/kafka/kafka_text.c -o "$scratch_dir/player-tests"
  "$scratch_dir/player-tests"
done
