#include "spi_port.h"



bool SpiPort::connect(const bool virtual_comm)
{
    if(ftdi)
        return true;

    int rc = 0;

    virtual_ = virtual_comm;
    
    ftdi = ftdi_new();
    if (ftdi == NULL) {
        fprintf(stderr, "ftdi_new failed\n");
        return false;
    }
    
    rc = ftdi_usb_open(ftdi, FTDI_VID, FTDI_PID);
    if (rc < 0) {
        fprintf(stderr, "Unable to open FTDI device: %d (%s)\n", 
                rc, ftdi_get_error_string(ftdi));
        ftdi_free(ftdi);
        return false;
    }
    
    printf("\nFTDI device opened successfully\n");
    
    // Reset and configure device
    ftdi_usb_reset(ftdi);
    ftdi_set_latency_timer(ftdi, 1);
    ftdi_tcioflush(ftdi);
    
    // Enable MPSSE mode
    rc = ftdi_set_bitmode(ftdi, 0x00, BITMODE_RESET);
    if (rc < 0) {
        fprintf(stderr, "Unable to reset bitmode: %d (%s)\n", 
                rc, ftdi_get_error_string(ftdi));
        return false;
    }
    usleep(BASE_SLEEP);
    
    rc = ftdi_set_bitmode(ftdi, 0x00, BITMODE_MPSSE);
    if (rc < 0) {
        fprintf(stderr, "Unable to enable MPSSE mode: %d (%s)\n", 
                rc, ftdi_get_error_string(ftdi));
        return false;
    }
    usleep(BASE_SLEEP);
    
    ftdi_tcioflush(ftdi);

    // configure
    uint8_t mpsse_cmd[20];
    int idx = 0;
    
    // Disable divide-by-5 to get 60 MHz base clock
    mpsse_cmd[idx++] = DIS_DIV_5;
    mpsse_cmd[idx++] = DIS_ADAPTIVE;
    mpsse_cmd[idx++] = DIS_3_PHASE;
    
    // Set clock divisor: 60 MHz / ((1 + divisor) * 2) = X MHz
    double divf = (30.0 / frequency) - 1.0;
    if (divf < 0)
        divf = 0;          // clamp (max speed)
    uint16_t divisor = (uint16_t)(divf + 0.5);  // round to nearest

    printf("divisor %u\n", divisor);

    // mpsse_cmd[idx++] = TCK_DIVISOR;
    // mpsse_cmd[idx++] = divisor & 0xFF;        // low byte
    // mpsse_cmd[idx++] = (divisor >> 8) & 0xFF; // high byte
        
    mpsse_cmd[idx++] = TCK_DIVISOR;
    mpsse_cmd[idx++] = 0x02; // low byte
    mpsse_cmd[idx++] = 0x00; // high byte

    // Configure pins: SK, DO, CS as outputs; DI as input
    mpsse_cmd[idx++] = SET_BITS_LOW;
    mpsse_cmd[idx++] = CS_HIGH;
    mpsse_cmd[idx++] = PIN_DIRECTION;
    
    // Set high byte pins to inputs
    mpsse_cmd[idx++] = SET_BITS_HIGH;
    mpsse_cmd[idx++] = 0x00;
    mpsse_cmd[idx++] = 0x00;
    
    if (write_check(mpsse_cmd, idx) < 0) {
        return false;
    }
    
    printf("Configured for SPI Mode 0\n");
    usleep(BASE_SLEEP);
    
    return true;

}

void SpiPort::disconnect()
{
    int rc = 0;

    if (!ftdi)
        return;

    rc = ftdi_usb_close(ftdi);
    if (rc < 0)
        fprintf(stderr, "Error: ftdi_usb_close failed: %s\n", ftdi_get_error_string(ftdi));
    ftdi_free(ftdi);

    ftdi = nullptr;

    printf("\nFTDI device closed\n");
}


bool SpiPort::checkConnected()
{
    return ftdi == nullptr;
}

int SpiPort::activate(bool activ)
{
    int state = activ ? CS_ASSERT : CS_DEASSERT;
    return setCS(state);
}

bool SpiPort::sendCharArray(uint8_t* buffer, int size)
{
    size_t tx_size = size + CRC_SIZE;
    uint8_t* tx_buffer = (uint8_t *)malloc(tx_size);
    prepare_tx_buffer(tx_buffer, buffer, size);

    uint8_t* dummy_rx = (uint8_t *)calloc(tx_size, 1);  

    size_t rx_size_rec = 0;
    print_hex("test", tx_buffer, tx_size);
    int rc = exchange(tx_buffer, tx_size, dummy_rx, &rx_size_rec);

    if(rc || rx_size_rec != tx_size){
        printf("falied to send cahr array rc = %d, rx_size_rec = %zu, tx_size = %zu\n", rc, rx_size_rec, tx_size);
        return false;
    }

    return true;
}

int SpiPort::readSerial(uint8_t* rx_buffer, int buf_max_size)
{
    if(!ftdi)
        return 0;;

    int rc = 0;
    int ready = 0;
    size_t rx_size = 0;
    size_t rx_expected_size = 0;
    
    // Poll for ready status
    printf("Polling for device ready...\n");
    size_t poll_len = 0;
    for (int i = 0; i < MAX_READ_ATTEMPTS; i++) {
        usleep(POLL_SLEEP);
        
        poll_len = 0;

        // poll_len == 0 for the first message already received during cmd exchange
        if ((rc = exchange(dummy_tx_poll, STATUS_POLL_SIZE, 
                                            poll_buffer, &poll_len)) < 0) 
        {
            fprintf(stderr, "Failed to poll device\n");
            // todo - do something here
        }
        
        print_hex(" --- poll bf: ", poll_buffer, poll_len);

        if (poll_len >= 2 && (((poll_buffer[0] & 0xF0) >> 4 == HEADER_READY) || 
                            ((poll_buffer[0] & 0x0F) == HEADER_READY) ||
                            (poll_len > 1 && (poll_buffer[1] & 0xF0) >> 4 == HEADER_READY))) 
        {
            printf("\nDevice ready (header: 0x%03X)\n", (poll_buffer[0] << 4) | ((poll_buffer[1] & 0xF0) >> 4));
            ready = 1;
            rx_expected_size = (int)(((poll_buffer[1] & 0x0F) << 8) | poll_buffer[2]);
            break;
        }
        else{
            printf("Device not ready (header: 0x%03X) idx = %d\n", (poll_buffer[0] << 4) | ((poll_buffer[1] & 0xF0) >> 4), i);
            fflush(stdout);
        }
    }
    
    if (!ready || !rx_expected_size) {
        fprintf(stderr, "Device did not become ready\n");
        return 0;;
    }
    
    // check that expected size is not bigger then it should be
    if((size_t)buf_max_size < rx_expected_size){
        printf("error - expected size %zu is bigger than rx buffer size %u\n", rx_expected_size, buf_max_size);         
        return 0;
    }

    // Read response message
    printf("Reading response (%zu bytes expected)...\n", rx_expected_size);
    usleep(POLL_SLEEP);
    
    uint8_t *dummy_tx_msg = (uint8_t *)calloc(rx_expected_size, 1);

    if (!dummy_tx_msg || !rx_buffer) {
        fprintf(stderr, "Failed to allocate RX buffers\n");
        free(dummy_tx_msg);
        return 0;;
    }
    
    rx_size = 0;
    if ((rc = exchange(dummy_tx_msg, rx_expected_size, rx_buffer, &rx_size)) < 0) {
        fprintf(stderr, "Failed to read response\n");
        free(dummy_tx_msg);
        return 0;;
    }
    
    free(dummy_tx_msg);

    print_hex("response: ", rx_buffer, rx_size);

    // check sizes
    if(rx_size != rx_expected_size){
        printf("incorrect received size: expected = %zu, received = %zu \n", rx_expected_size, rx_size);
        return 0;
    }

    // validate CRC and remove CRC if success
    if(verify_crc(rx_buffer, rx_size)){
        return 0;
    }else{
       rx_size -= CRC_SIZE; 
    }

    // Check for valid data header and given response
    // if (rx_buffer[0] == HEADER_DATA) {
    //     printf("Got data header (0x%02X)\n", rx_buffer[0]);
        
    //     if (rx_size > 1) {
    //         memmove(rx_buffer, rx_buffer + 1, rx_size - 1);
    //         rx_size--; 
    //     } else {
    //         rx_size = 0;
    //     }
        
    // } else {
    //     fprintf(stderr, "Invalid response - header: 0x%02X and size %zu\n", 
    //             rx_size > 0 ? rx_buffer[0] : 0x00, rx_size);
    //     return 0;
    // }
    
    print_hex("final response: ", rx_buffer, rx_size);
    print_bytes("final response: ", rx_buffer, rx_size);


    return rx_size;
}

#include <stdio.h>

int SpiPort::readWriteSerial(uint8_t* tx_buffer, int tx_size)
{
    if(!ftdi)
        return 0;

    int rc = 0;
    int ready = 0;
    size_t rx_size = 0;
    
    // Poll for ready status
    printf("Polling for device ready...\n");
    size_t poll_len = 0;
    for (int i = 0; i < MAX_READ_ATTEMPTS; i++) {
        usleep(POLL_SLEEP);
        
        poll_len = 0;

        // poll_len == 0 for the first message already received during cmd exchange
        if ((rc = exchange(dummy_tx_poll, STATUS_POLL_SIZE, 
                                            poll_buffer, &poll_len)) < 0) 
        {
            fprintf(stderr, "Failed to poll device\n");
            // todo - do something here
        }
        
        print_hex(" --- poll bf: ", poll_buffer, poll_len);

        if (poll_len >= 2 && (((poll_buffer[0] & 0xF0) >> 4 == HEADER_RECEIVE_READY) || 
                            ((poll_buffer[0] & 0x0F) == HEADER_RECEIVE_READY) ||
                            (poll_len > 1 && (poll_buffer[1] & 0xF0) >> 4 == HEADER_RECEIVE_READY))) 
        {
            printf("\nDevice ready for receive (header: 0x%03X)\n", (poll_buffer[0] << 4) | ((poll_buffer[1] & 0xF0) >> 4));
            ready = 1;
            break;
        }
        else{
            printf("Device not ready (header: 0x%03X) idx = %d\n", (poll_buffer[0] << 4) | ((poll_buffer[1] & 0xF0) >> 4), i);
            fflush(stdout);
        }
    }
    
    if (!ready) {
        fprintf(stderr, "Device did not become ready\n");
        return 0;
    }

    // Read response message
    printf("Sending data (%zu bytes)...\n", tx_size);
    usleep(POLL_SLEEP);

    sendCharArray(tx_buffer, tx_size);
    fprintf(stderr, "Chunk sent, wait for ack\n");

    static const size_t RX_SIZE = 10;
    uint8_t  buffer[RX_SIZE];
    readSerial(buffer, RX_SIZE);

    return ready;

    //validate CRC and remove CRC if success
    if(verify_crc(buffer, RX_SIZE)){
        return 0;
    }else{
       rx_size -= CRC_SIZE; 
    }

    // Check for valid data header and given response
    // if (rx_buffer[0] == HEADER_DATA) {
    //     printf("Got data header (0x%02X)\n", rx_buffer[0]);
        
    //     if (rx_size > 1) {
    //         memmove(rx_buffer, rx_buffer + 1, rx_size - 1);
    //         rx_size--; 
    //     } else {
    //         rx_size = 0;
    //     }
        
    // } else {
    //     fprintf(stderr, "Invalid response - header: 0x%02X and size %zu\n", 
    //             rx_size > 0 ? rx_buffer[0] : 0x00, rx_size);
    //     return 0;
    // }
    
    // print_hex("final response: ", rx_buffer, rx_size);
    // print_bytes("final response: ", rx_buffer, rx_size);


    return rx_size;
}


void SpiPort::prepare_tx_buffer(uint8_t *tx_buffer, const uint8_t *data, size_t data_size) {
    // Copy original data
    memcpy(tx_buffer, data, data_size);
    
    // Calculate and append CRC16
    uint16_t crc = crc16_xmodem(data, data_size);
    tx_buffer[data_size] = (crc >> 8) & 0xFF;      // High byte
    tx_buffer[data_size + 1] = crc & 0xFF;         // Low byte
}

int SpiPort::verify_crc(const uint8_t *data, size_t len) 
{
    uint16_t decoded_crc = crc16_xmodem(data, len);
    printf("Decoded CRC16: 0x%04X\n", decoded_crc);
    
    if (decoded_crc == 0) {
        printf("CRC OK!\n");
        return 0;
    } else {
        printf("CRC MISMATCH!\n");
        return -1;
    }
}

int SpiPort::setCS(int state) {
    if(!ftdi)
        return -1;

    uint8_t cmd[3];
    cmd[0] = SET_BITS_LOW;
    cmd[1] = state ? CS_HIGH : CS_LOW;
    cmd[2] = PIN_DIRECTION;
    if(write_check(cmd, 3)){
        printf("failed to set CS to %d\n", state);
        return -1;
    }
    cs = state;
    return 0;
}

int SpiPort::write_check(uint8_t *buf, int size) 
{
    if (!ftdi)
        return -1;
    
    int ret = ftdi_write_data(ftdi, buf, size);
    
    if (ret < 0) {
        fprintf(stderr, "Write failed: %s\n", ftdi_get_error_string(ftdi));
        return -1;
    }
    
    // Check if all bytes were written
    if (ret != size) {
        fprintf(stderr, "Partial write: %d of %d bytes written\n", ret, size);
        return -1;
    }
    
    return 0;
}

int SpiPort::read_with_retry(uint8_t *rx_buffer, size_t read_len, size_t *rx_len) 
{
    if(!ftdi)
        return -1;

    int total_read = 0;
    int attempts = 0;
    int ret = 0;
    int tryc = 0;

    while (total_read < (int)read_len && attempts < MAX_READ_ATTEMPTS) {
        tryc++;

        ret = ftdi_read_data(ftdi, &rx_buffer[total_read], read_len - total_read);
        if (ret < 0) {
            fprintf(stderr, "Read failed: %s\n", ftdi_get_error_string(ftdi));
            return -1;
        }
        total_read += ret;
        // printf(" --- tryc = %d recived %d \n", tryc, ret);

        if (total_read < (int)read_len) {
            usleep(1000);
            attempts++;
        }
    }

    *rx_len = total_read;
    return 0;
}

int SpiPort::exchange(const uint8_t *tx_buffer, size_t tx_len, uint8_t *rx_buffer, size_t *rx_len) 
{
    if(!ftdi)
        return -1;

    // Allocate command buffer: 3 bytes header + data + 1 byte SEND_IMMEDIATE
    uint8_t *spi_cmd = (uint8_t *)malloc(tx_len + 4);
    if (spi_cmd == NULL) {
        fprintf(stderr, "Failed to allocate SPI command buffer\n");
        return -1;
    }
    
    int idx = 0;
    int ret;
    
    // Prepare SPI command: full duplex transfer
    spi_cmd[idx++] = SPI_DUPLEX_MSB;
    spi_cmd[idx++] = (tx_len - 1) & 0xFF;        // Length low byte
    spi_cmd[idx++] = ((tx_len - 1) >> 8) & 0xFF; // Length high byte
    memcpy(&spi_cmd[idx], tx_buffer, tx_len);
    idx += tx_len;
    
    // Send immediate command
    spi_cmd[idx++] = SEND_IMMEDIATE;
    
    // Write command
    ret = write_check(spi_cmd, idx);
    free(spi_cmd);
    
    if (ret < 0) {
        return -1;
    }
    
    // Read response with retry logic
    return read_with_retry(rx_buffer, tx_len, rx_len);
}