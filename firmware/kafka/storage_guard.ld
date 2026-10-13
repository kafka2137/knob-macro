/* Reserve at least the final 64 KiB of a 128 KiB device for EEPROM.
 * The EFL driver locates storage from the actual MCU flash-size register. */
SECTIONS {
    .kafka_storage_guard : {
        ASSERT(LOADADDR(.data) + SIZEOF(.data) <= 0x08010000,
               "Kafka firmware overlaps reserved EEPROM storage");
    }
} INSERT AFTER .data;
