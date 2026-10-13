# Q8 ISO encoder firmware

Custom QMK source is in `kafka/`. The target is `keychron/q8/iso_encoder:kafka`; this is not a generic Keychron binary.

The validated QMK base was `qmk/qmk_firmware` commit `fe75e748f89d201ab1b7d97113a51d7e2bc3d861`. Use that compatible tree/toolchain and its required submodules. This repo contains the custom keymap, not the full QMK tree.

Copy all files from `kafka/` into `keyboards/keychron/q8/iso_encoder/keymaps/kafka/` in QMK, then run:

```sh
qmk compile -kb keychron/q8/iso_encoder -km kafka
```

The isolated build used `make keychron/q8/iso_encoder:kafka -j4 DFU_SUFFIX_ARGS=` to omit the optional DFU suffix because the installed platform helper could not run. The resulting raw binary was 52,040 bytes. Flashing is a separate step.

Export macros before flashing, especially when upgrading from shared storage. Firmware binaries belong in GitHub Releases. Existing independent-bank storage remains compatible with the layer-number and knob fixes.

## Layers and controls

FN3/FN4 mean numeric VIA/Launcher layers **3 and 4**, named `_FN2` and `_FN3` in the inherited enum. Each has 15 definitions and assignments. The sixth dynamic layer stays allocated for storage compatibility; numeric layer 5 has no macro encoder bank.

Rotation cancels and selects a step. Each bank keeps its selection while powered. Knob matrix position **row 0, column 14** is intercepted by `pre_process_record_user` before normal key processing on macro layers: text cancels and normal replays immediately. Its release is consumed even if Fn is released first. Other layers retain the knob key binding.

## Timing

Settings in `kafka/config.h` require rebuilding/flashing after modification:

- `KAFKA_NORMAL_DEBOUNCE_MS`: rotation debounce, default 200 ms; 0 disables it. Knob press and macro keys bypass it.
- `DYNAMIC_KEYMAP_MACRO_DELAY`: default 1 ms playback spacing.
- `KAFKA_UNICODE_STEP_MS`: defaults to macro spacing.
- `KAFKA_COMPOSE_DELAY_MS`: default 1 ms Compose gap.
- `KAFKA_COMPOSE_KEY`: Right Alt for Windows WinCompose.

Increase spacing for applications that miss fast input. Intentional waits use deferred callbacks. USB transport backpressure can still delay reports.

## Storage and protocol

Six dynamic layers and 30 macros use a **31,827-byte** macro buffer in 32 KiB logical EEPROM backed by 64 KiB wear-leveled flash. The linker guard reserves the final 64 KiB of a 128 KiB device; code must fit in the first 64 KiB. The driver locates storage from actual MCU flash size.

Global slots 0–14 belong to numeric layer 3, slots 15–29 to layer 4. Definitions are NUL-terminated and at most 1,023 bytes. Text is raw UTF-8. Normal macros start with `01 4E`; opcodes are 1=tap, 2=down, 3=up, 4=delay. Keys use two nibbles, delays four, stored plus one to avoid NUL. Invalid actions are rejected.

The final 34 bytes contain `KM3`, two 15-byte assignment maps, and the write marker. Assignments are bank-local indices 0–14 or 255 for unassigned. Missing metadata defaults each bank to its own macros in order.

VIA Raw HID commands `0x0E`/`0x0F` read/write up to 28 bytes per report. Capability command `0xB0` returns `KFU8`, format 3, count 30, maximum bytes 1,023, WinCompose mode 4, two banks, 15 steps, and layer IDs 3/4. There are no site-driven layer activation commands.

Writes cancel playback. Saving sets the write marker, uploads/verifies the full buffer, then clears it. Failed saves disable playback until a retry succeeds; this is not atomic rollback.

## Tests

From the repository root run `bash scripts/check.sh`. Native tests compile the actual player against mocked QMK APIs and check Unicode, cancellation, ownership, deferred delays, assignments, knob replay, and debounce. Protocol tests check bank serialization, legacy migration, capacity, uploads/readback, recovery, and timeouts. Hardware interaction is not automated.

## Knob press troubleshooting

Build from this repository's current `firmware/kafka/` files. An older external QMK checkout may still contain a keymap without the knob handler. Copy **all** keymap source files before compiling. Flash the newly built binary rather than a previous release asset. Updating the website alone cannot change physical knob behavior.

The knob event test compiles the actual keymap event hooks. It verifies that a resolved `KC_MUTE` event on numeric layers 3/4 is swallowed before regular processing, including release after Fn is released. It also tests remapped knob keys, independent selections, unassigned steps, and passthrough outside macro layers.
