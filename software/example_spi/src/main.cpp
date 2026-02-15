#include <chrono>
#include <thread>

#include <spi_port.h>
#include <mui.h>


// This is better to be large for Linux, in case the serial driver
// fills in more than one packet.
#define RX_SERIAL_BUFFER_SIZE 5 * LLCP_RX_TX_BUFFER_SIZE

#define TX_SERIAL_BUFFER_SIZE LLCP_RX_TX_BUFFER_SIZE

SpiPort     serial_port_minipix_;  // communication port/SPI with device 
uint8_t     tx_buffer[TX_SERIAL_BUFFER_SIZE];

MUI_Handler_t mui_handler_; // communication protocol handler (callbacks for device responses, reading msgs etc) 


bool              ack_ = true;
bool              power_up_failed_ = false;
bool              measuring_frame_ = false;
bool              receiving_frame_ = false;
uint16_t          save_max_pixels_ = 10000;
uint16_t          number_of_pixels_saved_ = 0;
uint16_t          number_of_pixels_not_saved_ = 0;
uint16_t          pixel_count_     = 0;

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

    if(!measured_data_file_)
        return;

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

void mui_linux_processStatus(const LLCP_Status_t *data) {

    ;
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

            measuring_frame_ = false;
            printf("Error: '%s'\n", LLCP_MinipixErrors[LLCP_MINIPIX_ERROR_MEASUREMENT_FAILED]);

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

    measuring_frame_ = false;
    receiving_frame_ = true;
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

        uint16_t n_bytes = llcp_prepareMessage((uint8_t *)&msg, sizeof(msg), tx_buffer);

        serial_port_minipix_.send_char_array(tx_buffer, n_bytes);
    }
}


void mui_linux_processFrameDataTerminator([[maybe_unused]] const LLCP_FrameDataTerminator_t *data) {

    printf("data terminator\n");
    printf("received frame with %d pixels\n", number_of_pixels_saved_ + number_of_pixels_not_saved_);
    printf("saved only %d of them\n", number_of_pixels_saved_);

    receiving_frame_ = false;
}

// --------------------------------------------------------------
// |                     methods for the MUI                    |
// --------------------------------------------------------------

void mui_linux_sleepHW(const uint16_t milliseconds) {

    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

void mui_linux_sendString(const uint8_t *str_out, const uint16_t len) {

    if (!serial_port_minipix_.send_char_array((unsigned char *)str_out, len)) {
        printf("failed sending message with %d bytes\n", len);
    }
}

// --------------------------------------------------------------
// |                     read from minipix                      |
// --------------------------------------------------------------

int read_response(void) {

    uint8_t  buffer[RX_SERIAL_BUFFER_SIZE];
    uint16_t bytes_read = serial_port_minipix_.read_serial(buffer, RX_SERIAL_BUFFER_SIZE);

    if(!bytes_read)
        return -1;

    for (uint16_t i = 0; i < bytes_read; i++) {
        mui_receiveCharCallback(&mui_handler_, buffer[i]);
    }

    return 0;
}

// --------------------------------------------------------------
// |                            MAIN                            |
// --------------------------------------------------------------

int main(int argc, char *argv[]) {

    int rc = 0;
    std::string data_path;
    bool serial_port_virtual = false;

    int frame_count = 200;
    int acq_time_ms = 100;

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
    mui_handler_.fcns.processFrameData                = &mui_linux_processFrameData;
    mui_handler_.fcns.processFrameDataTerminator      = &mui_linux_processFrameDataTerminator;
    mui_handler_.fcns.processFrameMeasurementFinished = &mui_linux_processMeasurementFinished;
    mui_handler_.fcns.processStatus                   = &mui_linux_processStatus;
    mui_handler_.fcns.processTemperature              = &mui_linux_processTemperature;
    mui_handler_.fcns.processAck                      = &mui_linux_processAck;
    mui_handler_.fcns.processMinipixError             = &mui_linux_processMinipixError;
    mui_handler_.fcns.processChipVoltage              = &mui_linux_processChipVoltage;
    mui_handler_.fcns.sendString                      = &mui_linux_sendString;

    mui_initialize(&mui_handler_);

    // | --------------- lets do some measurement and save data -------------- |

    measured_data_file_ = fopen(data_path.c_str(), "w");

    if (!measured_data_file_) {
        printf("Error: cannot open the data output file '%s' for writing!\n", data_path.c_str());
    }

    printf("power on the device\n");
    serial_port_minipix_.activate(true);
    mui_pwr(&mui_handler_, 1);  
    rc = read_response();
    if(rc){
        // todo - if power up fails -> try several times
        return -1;
    } 
    serial_port_minipix_.activate(false);
    printf("power on the device finished\n");

    printf("measure temperature\n");
    serial_port_minipix_.activate(true);
    mui_getTemperature(&mui_handler_);
    rc = read_response();
    if(rc){
        // todo - reset communication and try again
        printf("failed to read temperature\n");
        return -1;
    }
    serial_port_minipix_.activate(false);
    printf("finished measure temperature\n");


    uint8_t confNum = 1;
    printf("changing configuration to %d\n", confNum);
    serial_port_minipix_.activate(true);
    mui_setConfigurationPreset(&mui_handler_, confNum);
    rc = read_response();
    if(rc){
        // todo - reset communication and try again
        printf("failed to change conf\n");
        return -1;
    }
    serial_port_minipix_.activate(false);
    // change configuration

    for(int i = 0; i < frame_count; i++){
        printf("=================\n");
        printf("measuring frame %d\n", i);

        serial_port_minipix_.activate(true);
        measureFrame(acq_time_ms, 0);
        while(measuring_frame_){
            // todo - some timeout = acq_time + offset for reading data -> reset communication and try again
            usleep(10000);
            rc = read_response();    
        }
        serial_port_minipix_.activate(false);

        printf(" * requesting frame data\n");
        serial_port_minipix_.activate(true);
        mui_getFrameData(&mui_handler_);
        rc = read_response();
        if(rc){
            // todo - reset communication and try again
            ;
        }
        serial_port_minipix_.activate(false);

        while(receiving_frame_)
        {
            usleep(10000);
            serial_port_minipix_.activate(true);
            mui_sendAck(&mui_handler_, true);
            rc = read_response();    
            if(rc){
                // todo - reset communication and try again
                ;
            }            
            serial_port_minipix_.activate(false);

        }

        printf(" * measuring frame %d finished \n", i);
    }

    return 0;
}