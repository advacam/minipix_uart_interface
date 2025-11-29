#ifndef SERIAL_PORT_H_
#define SERIAL_PORT_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string>
#include <unistd.h>
#include <ftdi.h>
#include <string>
#include <cstdint>

#include "utils.h"


// SPI Commands
#define SPI_DUPLEX_MSB  0x31  // Clock data out on -ve, in on +ve, MSB first
#define SPI_READ_MSB    0x20  // Read data on +ve clock, MSB first

// FTDI Device IDs
#define FTDI_VID        0x0403
#define FTDI_PID        0x6014

// Pin Configuration
#define CS_HIGH         0x08
#define CS_LOW          0x00
#define PIN_DIRECTION   0x0B  // SK, DO, CS as outputs; DI as input

#define CS_ASSERT       1   // ON
#define CS_DEASSERT     0   // OFF - going to using for reset

// Protocol Configuration
#define CRC_SIZE            2
#define MAX_READ_ATTEMPTS   15
#define BASE_SLEEP          100000
#define POLL_SLEEP          100000
#define STATUS_POLL_SIZE    5

// Response headers
#define HEADER_READY        0x0D
#define HEADER_NOT_READY    0x0A
// #define HEADER_DATA         0x9A

class SpiPort {
public:
    SpiPort(){}
    virtual ~SpiPort()
    {
        if(poll_buffer)
            free(poll_buffer);
        if(dummy_tx_poll)
            free(dummy_tx_poll);
    }

    bool connect(const bool virtual_comm);
    void disconnect();

    bool checkConnected();

    bool sendCharArray(uint8_t* buf, int size);
    int readSerial(uint8_t* arr, int arr_max_size);

    int activate(bool activ);

private:

    int setCS(int state);
    int write_check(uint8_t *buf, int size);
    int read_with_retry(uint8_t *rx_buffer, size_t read_len, size_t *rx_len);
    int exchange(const uint8_t *tx_buffer, size_t tx_len, uint8_t *rx_buffer, size_t *rx_len);
    void prepare_tx_buffer(uint8_t *tx_buffer, const uint8_t *data, size_t data_len);
    int verify_crc(const uint8_t *data, size_t len);


    bool virtual_ = false;
    struct ftdi_context *ftdi = nullptr;
    int frequency = 15; // MHz
    int cs = CS_DEASSERT;
    uint8_t *poll_buffer = (uint8_t *)malloc(STATUS_POLL_SIZE);
    uint8_t *dummy_tx_poll = (uint8_t *)calloc(STATUS_POLL_SIZE, 1);    
};

#endif  // SERIAL_PORT_H_
