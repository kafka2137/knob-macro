ENCODER_ENABLE = yes
EXTRAKEY_ENABLE = yes
ENCODER_MAP_ENABLE = no

# Host-editable texts using the standard VIA macro protocol.
VIA_ENABLE = yes
DEFERRED_EXEC_ENABLE = yes
SRC += kafka_text.c
EEPROM_DRIVER = wear_leveling
WEAR_LEVELING_DRIVER = embedded_flash
# Keep code clear of storage even on the 128 KiB MCU variant.
LDFLAGS += -Wl,-T,$(KEYMAP_PATH)/storage_guard.ld
