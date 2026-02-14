#include <chrono>
#include <cstddef>
#include <cstdio>
#include <thread>

#include <spi_port.h>
#include <mui.h>
#include <unistd.h>

#include <cstdint>
#include <algorithm>


// This is better to be large for Linux, in case the serial driver
// fills in more than one packet.
#define RX_SERIAL_BUFFER_SIZE 5 * LLCP_RX_TX_BUFFER_SIZE

#define TX_SERIAL_BUFFER_SIZE LLCP_RX_TX_BUFFER_SIZE

SpiPort     serial_port_minipix_;
uint8_t     tx_buffer[TX_SERIAL_BUFFER_SIZE];

MUI_Handler_t mui_handler_;


bool              ack_ = true;
bool              power_up_failed_ = false;
bool              measuring_frame_ = false;
uint16_t          save_max_pixels_ = 10000;
uint16_t          number_of_pixels_saved_ = 0;
uint16_t          number_of_pixels_not_saved_ = 0;

uint16_t          pixel_count_     = 0;

int16_t           temperature_ = 0;

FILE* measured_data_file_ = nullptr;




// --------------------------------------------------------------
// |               Method for saving data to file               |
// --------------------------------------------------------------

void bin2hex(const uint8_t x, uint8_t *buffer) {

    if (x >= 16) {
        *buffer       = "0123456789ABCDEF"[x / 16];
        *(buffer + 1) = "0123456789ABCDEF"[x & 15];
    } else {
        *buffer       = '0';
        *(buffer + 1) = "0123456789ABCDEF"[x];
    }
}


void saveFrameDataToFile(const LLCP_FrameData_t *data) {

    // I am putting the data back into our communication packet
    // ... to be able to decode it fully from the file.

    LLCP_FrameDataMsg_t msg;
    init_LLCP_FrameDataMsg_t(&msg);

    // fill in the payload
    msg.payload = *data;

    // max llcp message size * 2
    uint8_t out_buffer[3];
    memset(out_buffer, 0, 3);

    // fill in the out buffer with the message in HEX form
    for (size_t i = 0; i < sizeof(LLCP_FrameDataMsg_t); i++) {

        bin2hex(*(((uint8_t *)&msg) + i), out_buffer);

        fprintf(measured_data_file_, "%s", out_buffer);
    }

    fprintf(measured_data_file_, "\n");

    // probably not neccessary, but to be sure...
    fflush(measured_data_file_);
}

// --------------------------------------------------------------
// |          method which encapsulate the MUI methods          |
// --------------------------------------------------------------

void measureFrame(int acquisition_time_ms, int pixel_mode) {

    measuring_frame_ = true;

    mui_measureFrame(&mui_handler_, acquisition_time_ms, pixel_mode);
}

// --------------------------------------------------------------
// |                    callbacks for the MUI                   |
// --------------------------------------------------------------


void mui_linux_processAck(const LLCP_Ack_t *data) {

    printf("got ack: %d\n", data->success);
    ack_ = true;
}


void mui_linux_processTemperature(const LLCP_Temperature_t *data) {

    printf("measured temperature %u C \n", data->temperature);
}

void mui_linux_processChipVoltage(const LLCP_ChipVoltage_t *data) {

    printf("measured voltage %u mV\n", data->chip_voltage);
}


void mui_linux_processMinipixError([[maybe_unused]] const LLCP_MinipixError_t *data) {

    LLCP_MinipixErrorMsg_t* msg = (LLCP_MinipixErrorMsg_t*)data;
    ntoh_LLCP_MinipixErrorMsg_t(msg);
    LLCP_MinipixError_t* error = (LLCP_MinipixError_t*)&msg->payload;

    switch (error->error_id) {

        case LLCP_MINIPIX_ERROR_MEASUREMENT_FAILED: {

            printf("Error: '%s'\n", LLCP_MinipixErrors[LLCP_MINIPIX_ERROR_MEASUREMENT_FAILED]);

            // measuring_frame_ = false;

            break;
        }

        case LLCP_MINIPIX_ERROR_POWERUP_FAILED: {

            power_up_failed_ = true;
            printf("Error: '%s'\n", LLCP_MinipixErrors[LLCP_MINIPIX_ERROR_POWERUP_FAILED]);

            break;
        }

        case LLCP_MINIPIX_ERROR_POWERUP_TPX3_RESET_SYNC: {

            power_up_failed_ = true;
            printf("Error: '%s'\n", LLCP_MinipixErrors[LLCP_MINIPIX_ERROR_POWERUP_TPX3_RESET_SYNC]);

            break;
        }

        case LLCP_MINIPIX_ERROR_POWERUP_TPX3_RESET_RECVDATA: {

            power_up_failed_ = true;
            printf("Error: '%s'\n", LLCP_MinipixErrors[LLCP_MINIPIX_ERROR_POWERUP_TPX3_RESET_RECVDATA]);

            break;
        }

        case LLCP_MINIPIX_ERROR_POWERUP_TPX3_INIT_RESETS: {

            power_up_failed_ = true;
            printf("Error: '%s'\n", LLCP_MinipixErrors[LLCP_MINIPIX_ERROR_POWERUP_TPX3_INIT_RESETS]);

            break;
        }

        case LLCP_MINIPIX_ERROR_POWERUP_TPX3_INIT_CHIPID: {

            power_up_failed_ = true;
            printf("Error: '%s'\n", LLCP_MinipixErrors[LLCP_MINIPIX_ERROR_POWERUP_TPX3_INIT_CHIPID]);

            break;
        }

        case LLCP_MINIPIX_ERROR_POWERUP_TPX3_INIT_DACS: {

            power_up_failed_ = true;
            printf("Error: '%s'\n", LLCP_MinipixErrors[LLCP_MINIPIX_ERROR_POWERUP_TPX3_INIT_DACS]);

            break;
        }

        case LLCP_MINIPIX_ERROR_POWERUP_TPX3_INIT_PIXCFG: {

            power_up_failed_ = true;
            printf("Error: '%s'\n", LLCP_MinipixErrors[LLCP_MINIPIX_ERROR_POWERUP_TPX3_INIT_PIXCFG]);

            break;
        }

        case LLCP_MINIPIX_ERROR_POWERUP_TPX3_INIT_MATRIX: {

            power_up_failed_ = true;
            printf("Error: '%s'\n", LLCP_MinipixErrors[LLCP_MINIPIX_ERROR_POWERUP_TPX3_INIT_MATRIX]);

            break;
        }

        case LLCP_MINIPIX_ERROR_INVALID_PRESET: {

            printf("Error: '%s'\n", LLCP_MinipixErrors[LLCP_MINIPIX_ERROR_INVALID_PRESET]);

            break;
        }

        default: {
            printf("Error: received unhandled error message, id %d\n", error->error_id);
        }
    }
}

void mui_linux_processMeasurementFinished() {

    printf("measurement finished - ask for data \n");

    LLCP_GetFrameDataReqMsg_t msg;
    init_LLCP_GetFrameDataReqMsg_t(&msg);

    // convert to network endian
    // hton_LLCP_GetFrameDataReqMsg_t(&msg);

    uint16_t n_bytes = llcp_prepareMessage((uint8_t *)&msg, sizeof(msg), tx_buffer);

    serial_port_minipix_.sendCharArray(tx_buffer, n_bytes);
}

void mui_linux_processFrameData(const LLCP_FrameData_t *data) {

    printf("getting data -  pixel count = %u \n", data->n_pixels);

    pixel_count_ += data->n_pixels;

    if (number_of_pixels_saved_ + data->n_pixels < save_max_pixels_) {

        saveFrameDataToFile(data);
        number_of_pixels_saved_ += data->n_pixels;
    } else {
        number_of_pixels_not_saved_ += data->n_pixels;
    }

    // send ack back
    {
        printf("send ack for the data\n");

        LLCP_AckMsg_t msg;
        init_LLCP_AckMsg_t(&msg);

        msg.payload.success = 1;

        // // convert to network endian
        // hton_LLCP_AckMsg_t(&msg);

        uint16_t n_bytes = llcp_prepareMessage((uint8_t *)&msg, sizeof(msg), tx_buffer);

        serial_port_minipix_.sendCharArray(tx_buffer, n_bytes);
    }
}


void mui_linux_processFrameDataTerminator([[maybe_unused]] const LLCP_FrameDataTerminator_t *data) {

    printf("data terminator\n");
    printf("Received frame with %d pixels\n", number_of_pixels_saved_ + number_of_pixels_not_saved_);
    printf("Saved only %d of them\n", number_of_pixels_saved_);

    measuring_frame_ = false;
}

// --------------------------------------------------------------
// |                     methods for the MUI                    |
// --------------------------------------------------------------


void mui_linux_sleepHW(const uint16_t milliseconds) {

    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

void mui_linux_sendString(const uint8_t *str_out, const uint16_t len) {

    if (!serial_port_minipix_.sendCharArray((unsigned char *)str_out, len)) {
        printf("failed sending message with %d bytes\n", len);
    }
}


// --------------------------------------------------------------
// |                     read from minipix                      |
// --------------------------------------------------------------

int read_response(void) {

    uint8_t  buffer[RX_SERIAL_BUFFER_SIZE];
    uint16_t bytes_read = serial_port_minipix_.readSerial(buffer, RX_SERIAL_BUFFER_SIZE);

    if(!bytes_read)
        return -1;

    for (uint16_t i = 0; i < bytes_read; i++) {
        mui_receiveCharCallback(&mui_handler_, buffer[i]);
    }

    return 0;
}

int validate_response(size_t poll_sleep = POLL_SLEEP) {

    const uint8_t HEADER_BYTE = 0x62;
    const uint8_t RESPONSE_MARKER_1 = 0xAB;
    const uint8_t RESPONSE_MARKER_2 = 0xCD;
    const uint8_t ACK_OK = 0x00;

    static const size_t RESPONSE_LEN = 10;
    uint8_t buffer[RESPONSE_LEN];
    uint16_t bytes_read = serial_port_minipix_.readSerial(buffer, RESPONSE_LEN, poll_sleep);

    if (bytes_read < 1) {
        return 0;
    }
    
    // Check for valid data header and response markers
    if (buffer[0] == HEADER_BYTE && 
        buffer[3] == RESPONSE_MARKER_1 && 
        buffer[4] == RESPONSE_MARKER_2) {
        
        printf("Got response (0x%02X%02X%02X%02X)\n", 
               buffer[3], buffer[4], buffer[5], buffer[6]);
        
        if (buffer[6] == ACK_OK) {
            printf("ACK OK\n");
            return 1;
        } else {
            printf("ACK ERR\n");
            return 0; // Error code in buffer[6]
        }
    }
    
    fprintf(stderr, "Did not get the response\n");
    return 0;
}


// --------------------------------------------------------------
// |                     bootloader functions                   |
// --------------------------------------------------------------

static const uint8_t HEADER = 0x62;
static const size_t CHECKSUM_SIZE = 1;
static const size_t HEADER_SIZE = 1;
static const size_t CMD_SIZE = 1;
static const size_t PAYLOAD_LENGTH_SIZE = 1;
static const size_t CRC_SIZEE = 2;
static const size_t MAX_BUFF_SIZE = 256;

static const size_t TOP_SECTOR = 12;

int sendNewFwSize(uint32_t fwSize) {

    if (fwSize > 0x178000) {
        printf("fw size too large %u\n", fwSize);
        return -1;
    }

    static const size_t PAYLOAD_SIZE = 3;
    static const size_t DATA_SIZE = CMD_SIZE + PAYLOAD_SIZE;
    static const size_t BUFF_SIZE = HEADER_SIZE + PAYLOAD_LENGTH_SIZE + DATA_SIZE + CHECKSUM_SIZE + CRC_SIZEE;

    uint8_t buffer[MAX_BUFF_SIZE] = {HEADER};
    buffer[1] = DATA_SIZE;
    buffer[2] = 'g';
    buffer[3] = (fwSize >> 16) & 0xFF; 
    buffer[4] = (fwSize >> 8) & 0xFF; 
    buffer[5] = fwSize & 0xFF; 
    buffer[6] = 0xFE; // as check sum
    printf("send fw size %d\n", fwSize);
    serial_port_minipix_.activate(true);
    serial_port_minipix_.sendCharArray(buffer, BUFF_SIZE - CRC_SIZEE);

    int rc = validate_response();

    printf("rc = %d\n", rc);
    serial_port_minipix_.activate(false);
    printf("finished send fw size\n");

    return rc;
}

int unlockFlash() {

    static const size_t PAYLOAD_SIZE = 6;
    static const size_t DATA_SIZE = CMD_SIZE + PAYLOAD_SIZE;
    static const size_t BUFF_SIZE = HEADER_SIZE + PAYLOAD_LENGTH_SIZE + DATA_SIZE + CHECKSUM_SIZE + CRC_SIZEE;

    uint8_t buffer[MAX_BUFF_SIZE] = {HEADER};
    buffer[1] = DATA_SIZE;
    buffer[2] = 'u';
    buffer[3] = 0x01; 
    buffer[4] = 0x02; 
    buffer[5] = 0x03; 
    buffer[6] = 0x04; 
    buffer[7] = 0x05; 
    buffer[8] = 0x06; 
    buffer[9] = 0xFE; // as check sum
    printf("send unlock flash\n");
    serial_port_minipix_.activate(true);
    serial_port_minipix_.sendCharArray(buffer, BUFF_SIZE - CRC_SIZEE);

    int rc = validate_response();

    printf("rc = %d\n", rc);
    serial_port_minipix_.activate(false);
    printf("finished send unlock flash\n");

    return rc;
}

int eraseFlashSector(uint8_t sector) {

    if (sector < 1 || sector >= TOP_SECTOR) {
        return 0;
    }

    static const size_t PAYLOAD_SIZE = 1;
    static const size_t DATA_SIZE = CMD_SIZE + PAYLOAD_SIZE;
    static const size_t BUFF_SIZE = HEADER_SIZE + PAYLOAD_LENGTH_SIZE + DATA_SIZE + CHECKSUM_SIZE + CRC_SIZEE;

    uint8_t buffer[MAX_BUFF_SIZE] = {HEADER};
    buffer[1] = DATA_SIZE;
    buffer[2] = 'e';
    buffer[3] = sector;  
    buffer[4] = 0xFE; // as check sum
    printf("send erase flash sector\n");
    serial_port_minipix_.activate(true);
    serial_port_minipix_.sendCharArray(buffer, BUFF_SIZE - CRC_SIZEE);

    int rc = validate_response(POLL_SLEEP_SLOW);

    printf("rc = %d\n", rc);
    serial_port_minipix_.activate(false);
    printf("finished erasing flash sector\n");

    return rc;
}

int eraseFlash() {

    int rc = 0;

    rc = unlockFlash();
    if (!rc) {
        return rc;
    }

    for (uint8_t i = 1; i < TOP_SECTOR; i ++) {
        rc = eraseFlashSector(i); // poll sleep must be 100000
        if (!rc) {
            return rc;
        }
    }

    return rc;
}

int eraseFlashArea(size_t size) {

    static const size_t SECTOR_BASE = 1024; 
    static const size_t SECTOR_SIZES[12] = { 32, 32, 32, 32, 128, 256, 256, 256, 256, 256, 256, 256};
    static const size_t REPEAT = 10;
    uint8_t sector = 1;
    size_t totalSectorsSize = 0;
    size_t sectorsToErase = 0;
    int rc = 0;

    while (size > totalSectorsSize) { // erasedSectors size must be bigger than size to write
        if (sector >= TOP_SECTOR) {
            printf("Not enough memory, flash limit reached: %d bytes\n", totalSectorsSize);
            return 1;
        }

        totalSectorsSize += SECTOR_SIZES[sector] * SECTOR_BASE; // helper to iterate
        sector ++;
    }
    printf("TotalSectorsSize: %d / data size: %d\n", totalSectorsSize, size);

    rc = unlockFlash();
    if (!rc) {
        return rc;
    }

    uint8_t i = 1;
    uint8_t repeat = 0;
    while (i < sector) {
        rc = eraseFlashSector(i); // poll sleep must be 100000
        if (!rc)  {
            printf("failed to erase sector: %d\n", i);
            printf("repeat to erase sector: %d, attempt: %d, \n", i, repeat);

            repeat++;
            if (repeat > REPEAT) {
                printf("fatal error sector: %d\n", i);
                return 0;
            }
            // Don't increment, retry the same sector
            continue;
        }
        i ++;
        repeat = 0;
    }

    printf("Finished erasing flash area: %d/%d\n", totalSectorsSize, size);
    return 1;
}


int sendNewFwChunk(uint32_t offset, uint8_t *chunk, size_t chunk_size) {

    static const size_t PAYLOAD_SIZE = 3;
    static const size_t DATA_SIZE = CMD_SIZE + PAYLOAD_SIZE;
    static const size_t BUFF_SIZE = HEADER_SIZE + PAYLOAD_LENGTH_SIZE + DATA_SIZE + CHECKSUM_SIZE + CRC_SIZEE;

    uint8_t buffer[MAX_BUFF_SIZE] = {HEADER};
    buffer[1] = DATA_SIZE;
    buffer[2] = 'f';
    buffer[3] = (offset >> 16) & 0xFF; 
    buffer[4] = (offset >> 8) & 0xFF; 
    buffer[5] = offset & 0xFF; 
    buffer[6] = 0xFE; // as check sum
    printf("send fw chunk offset 0x%x\n", offset);
    serial_port_minipix_.activate(true);
    serial_port_minipix_.sendCharArray(buffer, BUFF_SIZE - CRC_SIZEE);
    int rc = serial_port_minipix_.readWriteSerial(chunk, chunk_size); // response is evaluated inside function
    //TODO: handle return codes for writing the flash
    printf("rc = %d\n", rc);
    serial_port_minipix_.activate(false);
    printf("finished send fw chunk offset %d\n", offset);
    return rc;
}

int switchToApp() {

    static const size_t PAYLOAD_SIZE = 0;
    static const size_t DATA_SIZE = CMD_SIZE + PAYLOAD_SIZE;
    static const size_t BUFF_SIZE = HEADER_SIZE + PAYLOAD_LENGTH_SIZE + DATA_SIZE + CHECKSUM_SIZE + CRC_SIZEE;

    uint8_t buffer[MAX_BUFF_SIZE] = {HEADER};
    buffer[1] = DATA_SIZE;
    buffer[2] = 's';
    buffer[3] = 0xFE; // as check sum
    printf("send switch to app\n");
    serial_port_minipix_.activate(true);
    serial_port_minipix_.sendCharArray(buffer, BUFF_SIZE - CRC_SIZEE);

    int rc = read_response();
    printf("rc = %d\n", rc);
    serial_port_minipix_.activate(false);
    printf("finished send switch to app\n");

    return rc;
}

int verifyNewFw() {

    static const size_t PAYLOAD_SIZE = 0;
    static const size_t DATA_SIZE = CMD_SIZE + PAYLOAD_SIZE;
    static const size_t BUFF_SIZE = HEADER_SIZE + PAYLOAD_LENGTH_SIZE + DATA_SIZE + CHECKSUM_SIZE + CRC_SIZEE;

    uint8_t buffer[MAX_BUFF_SIZE] = {HEADER};
    buffer[1] = DATA_SIZE;
    buffer[2] = 'c';
    buffer[3] = 0xFE; // as check sum
    printf("verify new fw\n");
    serial_port_minipix_.activate(true);
    serial_port_minipix_.sendCharArray(buffer, BUFF_SIZE - CRC_SIZEE);

    auto rc = validate_response();

    if (rc) {
        printf("FW VERIFY OK\n");
    } else {
        printf("FW VERIFY ERROR\n");
    }

    serial_port_minipix_.activate(false);
    printf("end verify new fw\n");

    return rc;
}

void appendCrc32ToBuffer(uint8_t *tx_buffer, uint8_t *dataIn, size_t data_size) {
    // Copy original data
    memcpy(tx_buffer, dataIn, data_size);

    // Use aligned temporary buffer for CRC calculation if needed
    size_t data_size_u32 = data_size / 4;
    uint32_t temp_buffer[data_size_u32];
    memcpy(temp_buffer, dataIn, data_size);

    printf("data: 0x%08x\n", temp_buffer[0]);
    
    // Calculate and append CRC16
    uint32_t crc = crc32(temp_buffer, data_size_u32);
    printf("crc32: 0x%08x\n", crc);
    printf("data_size_u32: %x\n", data_size);

    tx_buffer[data_size + 3] = (crc >> 24) & 0xFF; 
    tx_buffer[data_size + 2] = (crc >> 16) & 0xFF;  
    tx_buffer[data_size + 1] = (crc >> 8) & 0xFF;   
    tx_buffer[data_size] = crc & 0xFF;             
}

int flashNewFw(uint8_t *fw_data, size_t fw_size) {

    static const size_t CHUNK_SIZE = 512;
    static const size_t CRC_FW_SIZE = 4;
    static const size_t REPEAT = 10;

    auto start = std::chrono::system_clock::now();

    // add crc to the end of whole fw, added padding to be divisable by CRC_FW_SIZE
    size_t padding = (CRC_FW_SIZE - (fw_size % CRC_FW_SIZE)) % CRC_FW_SIZE;
    size_t total_size = fw_size + padding + CRC_FW_SIZE;
    uint8_t* tx_buffer = (uint8_t *)malloc(total_size);
    memset(tx_buffer, 0, total_size);
    // for (size_t i = 0; i < fw_size + padding; i ++ ) {
    //     printf("%02x", fw_data[i]);
    // }
    // printf("\n");

    appendCrc32ToBuffer(tx_buffer, fw_data, fw_size + padding);
    printf("size before %d\n", fw_size);
    printf("size padding %d\n", padding);
    printf("total buffer size %d\n", total_size);

    // for (size_t i = 0; i < fw_size + padding; i ++ ) {
    //     printf("%02x", fw_data[i]);
    // }
    // printf("\n");

    // for (size_t i = 0; i < total_size; i ++ ) {
    //     printf("%02x", tx_buffer[i]);
    // }
    // printf("\n");

    // send fw size and unlock flash
    int rc = 0;
    for (uint8_t i = 0; i < REPEAT; i ++) {
        rc = eraseFlashArea(total_size);
        if (rc) break;
    }
    if (!rc) return rc;

    for (uint8_t i = 0; i < REPEAT; i ++) {
        rc = sendNewFwSize(total_size);
        if (rc) break;
    }
    if (!rc) return rc;

    for (uint8_t i = 0; i < REPEAT; i ++) {
        rc = unlockFlash();
        if (rc) break;
    }
    if (!rc) return rc;

    auto before = std::chrono::system_clock::now();
    uint32_t sent = 0;
    uint32_t resendCntr = 0;
    uint8_t chunk[CHUNK_SIZE];
    uint32_t offset = 0;

    while (offset < total_size) {
        size_t chunk_size = (offset + CHUNK_SIZE > total_size) ? (total_size - offset) : CHUNK_SIZE;
        
        // Always use full CHUNK_SIZE, pad with zeros if needed
        memset(chunk, 0, CHUNK_SIZE);  // Zero the entire chunk first
        memcpy(chunk, tx_buffer + offset, chunk_size);
        
        auto rc = sendNewFwChunk(offset, chunk, CHUNK_SIZE);  // Always send CHUNK_SIZE
        if (!rc)  {
            printf("failed to send chunk offset 0x%x\n", offset);
            printf("resending offset: 0x%08x, attempt: %d, \n", offset, resendCntr);

            resendCntr++;
            if (resendCntr > REPEAT) {
                free(tx_buffer);
                printf("fatal error offset 0x%x\n", offset);
                return 0;
            }
            // Don't increment offset, retry the same chunk
            continue;
        }
        printf("sent chunk offset 0x%x\n", offset);
        sent += chunk_size;  // Track actual data sent (not padding)
        
        offset += CHUNK_SIZE;  // Move to next chunk
        resendCntr = 0;
    }

    auto after = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(after - before);
    printf("%ld ms\n", duration.count());
    printf("%d\n", sent);
    free(tx_buffer);

    for (uint8_t i = 0; i < REPEAT; i ++) {
        rc = verifyNewFw();
        if (rc) break;
    }
    if (!rc) return 0;

    auto durationWhole = std::chrono::duration_cast<std::chrono::milliseconds>(after - start);
    printf("%ld ms\n", durationWhole.count());
    printf("FW sent succesfully size %d/%d\n", sent, total_size);
    printf("Resent chunks: %d\n", resendCntr);
    
    return 1;
}

// Funkce pro naplnění bufferu náhodnými daty
void fillRandomData(uint8_t* buffer, size_t length) {
    for (size_t i = 0; i < length; i++) {
        buffer[i] = rand() & 0xFF;
    }
}

// --------------------------------------------------------------
// |                            MAIN                            |
// --------------------------------------------------------------


int main(int argc, char *argv[]) {

    int rc = 0;
    std::string data_path;
    bool serial_port_virtual = false;

    if (argc == 3) {
        data_path    = argv[1];
        serial_port_virtual = atoi(argv[2]);

        printf("loaded params:\n");
        printf("data path %s\n", data_path.c_str());

    } else {
        printf("params not supplied\n");
        return -1;
    }

    // open the serial ports
    serial_port_minipix_.connect(serial_port_virtual);

    // supply callback fuctions
    // mui_handler_.fcns.ledSetHW                        = &mui_linux_ledSetHW;
    // mui_handler_.fcns.sleepHW                         = &mui_linux_sleepHW;
    mui_handler_.fcns.processFrameData                = &mui_linux_processFrameData;
    mui_handler_.fcns.processFrameDataTerminator      = &mui_linux_processFrameDataTerminator;
    mui_handler_.fcns.processFrameMeasurementFinished = &mui_linux_processMeasurementFinished;
    // mui_handler_.fcns.processStatus                   = &mui_linux_processStatus;
    mui_handler_.fcns.processTemperature              = &mui_linux_processTemperature;
    mui_handler_.fcns.processAck                      = &mui_linux_processAck;
    mui_handler_.fcns.processMinipixError             = &mui_linux_processMinipixError;
    mui_handler_.fcns.processChipVoltage              = &mui_linux_processChipVoltage;
    mui_handler_.fcns.sendString                      = &mui_linux_sendString;

    mui_initialize(&mui_handler_);

    // | --------------- create file for saving data -------------- |

    measured_data_file_ = fopen(data_path.c_str(), "w");

    if (measured_data_file_ == nullptr) {
        printf("Error: cannot open the data output file '%s' for writing!\n", data_path.c_str());
    }

    // | ------------------ test bootloader ------------------------ |

    // FILE* fw_file = fopen("/home/curdam/_PROJECTS/lunar_lander/minipix_uart_interface/software/example_spi/_build/minipix.bin", "rb");
    // if (!fw_file) {
    //     printf("failed to open fw file\n");
    //     // handle error
    //     return 0;
    // }

    // // Get file size
    // fseek(fw_file, 0, SEEK_END);
    // long fw_size = ftell(fw_file);
    // fseek(fw_file, 0, SEEK_SET);

    // // Allocate buffer and read
    // uint8_t* fw_buffer = (uint8_t*)malloc(fw_size);
    // size_t bytes_read = fread(fw_buffer, 1, fw_size, fw_file);

    // fclose(fw_file);

    // return flashNewFw(fw_buffer, 11);

    static const size_t TEST_CNT = 5;
    static const size_t BUFF_LEN = 1000000;
    uint8_t fw_buffer[BUFF_LEN];

    // Initialize random number generator
    srand(time(NULL));
    
    printf("Starting flashNewFw test with random data and lengths\n");
    printf("=================================================\n\n");
    
    int successful_tests = 0;
    int failed_tests = 0;
    size_t test_lengths[TEST_CNT];
    int test_results[TEST_CNT];
    
    for (int test = 0; test < TEST_CNT; test++) {
        // Generate random length (from 1 to BUFF_LEN)
        size_t random_length = 111035;// (rand() % BUFF_LEN) + 1;
        test_lengths[test] = random_length;
        
        // Fill buffer with random data
        fillRandomData(fw_buffer, random_length);
        
        printf("Test %d: length = %zu bytes (%.2f MB)\n", 
               test + 1, random_length, random_length / 1024.0 / 1024.0);
        
        // Call tested function
        int result = flashNewFw(fw_buffer, random_length);
        test_results[test] = result;

        if (result == 1) {
            successful_tests++;
            printf("  -> Result: %d [OK]\n\n", result);
        } else {
            failed_tests++;
            printf("  -> Result: %d [ERROR]\n\n", result);
        }
        
        // Optional: short pause between tests
        // delay_ms(100);
    }
    
    printf("=================================================\n");
    printf("Test completed!\n");
    printf("Successful tests: %d/%zu\n", successful_tests, TEST_CNT);
    printf("Failed tests: %d/%zu\n", failed_tests, TEST_CNT);
    printf("Success rate: %.1f%%\n\n", (successful_tests / (float)TEST_CNT) * 100.0);
    
    printf("Detailed results:\n");
    printf("-------------------------------------------------\n");
    for (size_t i = 0; i < TEST_CNT; i++) {
        printf("Test %2zu: %8zu bytes (%.2f MB) - %s\n", 
               i + 1, 
               test_lengths[i], 
               test_lengths[i] / 1024.0 / 1024.0,
               (test_results[i] == 1) ? "OK" : "ERROR");
    }
    printf("=================================================\n");
    
    return 0;



    // static const size_t RECV_SIZE = 10;
    // uint8_t  recv[RECV_SIZE];
    // uint16_t bytes_read = serial_port_minipix_.readSerial(recv, RECV_SIZE);
    // for(int i = 0; i < RECV_SIZE; i++) {
    //     printf("%02X ", recv[i]);
    // }
    // serial_port_minipix_.activate(false);
    // printf("rc = %d\n", rc);
    // printf("finished test bootloader\n");


    // printf("power on the device\n");
    // serial_port_minipix_.activate(true);
    // mui_pwr(&mui_handler_, 1);  
    // rc = read_response();
    // printf("rc = %d\n", rc);
    // serial_port_minipix_.activate(false);
    // printf("power on the device finished\n");


    // // sleep(3);

    // printf("measure frame\n");
    // serial_port_minipix_.activate(true);
    // int acq_time_ms = 1000;
    // measureFrame(acq_time_ms, 0);
    // usleep(acq_time_ms*1000);
    // // while(measuring_frame_)
    // // {
    // //     usleep(10000);
    // rc = read_response();    
    // // }
    // serial_port_minipix_.activate(false);
    // printf("rc = %d\n", rc);
    // printf("finished measure frame\n");


    // printf("measure temperature\n");
    // serial_port_minipix_.activate(true);
    // mui_getTemperature(&mui_handler_);
    // rc = read_response();
    // serial_port_minipix_.activate(false);
    // printf("rc = %d\n", rc);
    // printf("finished measure temperature\n");


    return 0;


    // --------------------------------------------------------------
    // |                   let's measure something                  |
    // --------------------------------------------------------------

    // // the following parameters should be configurable from Earth

    // // global parameters 
    // bool     PARAM_SET_CONFIG_USING_TEMP = false;
    // uint16_t PARAM_CONFIG_TEMP_THRESHOLD = 35;

    // // A1-specific parameters
    // uint16_t PARAM_A1_DESIRED_OCCUPANCY_PX     = 1500;
    // uint16_t PARAM_A1_DEFAULT_ACQUISITION_TIME = 1000;  // milliseconds
    // uint8_t  PARAM_A1_CONFIGURATION_ID         = 0;
    // uint8_t  PARAM_A1_PXL_MODE                 = LLCP_TPX3_PXL_MODE_MPX_ITOT;  // {0, 1, 2}
    // uint16_t PARAM_A1_SAVE_MAX_PIXELS          = 3024;
    // uint16_t PARAM_A1_MIN_ACQUISITION_TIME     = 10;     // milliseconds
    // uint16_t PARAM_A1_MAX_ACQUISITION_TIME     = 10000;  // milliseconds

    // // A2-specific parameters
    // uint8_t  PARAM_A2_CONFIGURATION_ID = 0;
    // uint16_t PARAM_A2_ACQUISITION_TIME = 10000;  // milliseconds
    // uint8_t  PARAM_A2_PXL_MODE         = LLCP_TPX3_PXL_MODE_MPX_ITOT;  // {0, 1, 2}
    // uint16_t PARAM_A2_SAVE_MAX_PIXELS  = 3024;

    // for (int i = 0; i < 10; i++) {

    //     measurementA1(PARAM_A1_DESIRED_OCCUPANCY_PX, PARAM_A1_PXL_MODE, PARAM_A1_DEFAULT_ACQUISITION_TIME,
    //                     PARAM_A1_CONFIGURATION_ID, PARAM_SET_CONFIG_USING_TEMP, PARAM_CONFIG_TEMP_THRESHOLD, PARAM_A1_SAVE_MAX_PIXELS, PARAM_A1_MIN_ACQUISITION_TIME,
    //                     PARAM_A1_MAX_ACQUISITION_TIME);

    //     for (int j = 0; j < 6; j++) {
    //         measurementA2(PARAM_A2_PXL_MODE, PARAM_A2_ACQUISITION_TIME, PARAM_A2_CONFIGURATION_ID,
    //                     PARAM_SET_CONFIG_USING_TEMP, PARAM_CONFIG_TEMP_THRESHOLD, PARAM_A2_SAVE_MAX_PIXELS);
    //     }
    // }

    return 0;
}