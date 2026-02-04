#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

//CRC32
inline uint32_t crc32(const uint32_t* data, size_t len) {
    // Ethernet polynomial (0x04C11DB7)
    const uint32_t polynomial = 0x04C11DB7;
    // Initial value
    uint32_t crc = 0xFFFFFFFF;

    // Process each byte in the data
    for (size_t k = 0; k < len; k++) {
        // Process each byte in the 32-bit word
        for (int i = 0; i < 4; ++i) {
              
            uint8_t byte = (data[k] >> (24 - i * 8)) & 0xFF;
            crc ^= (static_cast<uint32_t>(byte) << 24);

            // Process each bit in the byte
            for (int j = 0; j < 8; ++j) {
                if (crc & 0x80000000) {
                    crc = (crc << 1) ^ polynomial;
                } else {
                    crc <<= 1;
                }
            }
        }
    }

    // Final XOR value
    return crc ^ 0x00000000;
}

// CRC16-XMODEM implementation
inline uint16_t crc16_xmodem(const uint8_t *data, size_t len) {
    uint16_t crc = 0x0000;
    uint16_t poly = 0x1021;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= (data[i] << 8);
        
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = ((crc << 1) ^ poly) & 0xFFFF;
            } else {
                crc = (crc << 1) & 0xFFFF;
            }
        }
    }
    
    return crc;
}

// Print buffer in hex format
inline void print_hex(const char *label, const uint8_t *data, size_t len) {
    printf("%s", label);
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
    }
    printf("\n");
}

// Print buffer as byte list
inline void print_bytes(const char *label, const uint8_t *data, size_t len) {
    printf("%s[", label);
    for (size_t i = 0; i < len; i++) {
        printf("%d", data[i]);
        if (i < len - 1) printf(", ");
    }
    printf("]\n");
}