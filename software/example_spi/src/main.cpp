#include <chrono>
#include <thread>

#include <spi_port.h>
#include <mui.h>


// This is better to be large for Linux, in case the serial driver
// fills in more than one packet.
#define RX_SERIAL_BUFFER_SIZE 5 * LLCP_RX_TX_BUFFER_SIZE

#define TX_SERIAL_BUFFER_SIZE LLCP_RX_TX_BUFFER_SIZE

SpiPort     serial_port_minipix_;
uint8_t     tx_buffer[TX_SERIAL_BUFFER_SIZE];

MUI_Handler_t mui_handler_;


bool              ack_ = true;
uint16_t          save_max_pixels_ = 0;
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
// |                    callbacks for the MUI                   |
// --------------------------------------------------------------


void mui_linux_processAck(const LLCP_Ack_t *data) {

    printf("got ack: %d", data->success);
    ack_ = true;
}


void mui_linux_processTemperature(const LLCP_Temperature_t *data) {

    printf("measured temperature %u C \n", data->temperature);
}

void mui_linux_processChipVoltage(const LLCP_ChipVoltage_t *data) {

    printf("measured voltage %u mV\n", data->chip_voltage);
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
    // mui_handler_.fcns.processFrameData                = &mui_linux_processFrameData;
    // mui_handler_.fcns.processFrameDataTerminator      = &mui_linux_processFrameDataTerminator;
    // mui_handler_.fcns.processStatus                   = &mui_linux_processStatus;
    mui_handler_.fcns.processTemperature              = &mui_linux_processTemperature;
    mui_handler_.fcns.processAck                      = &mui_linux_processAck;
    // mui_handler_.fcns.processMinipixError             = &mui_linux_processMinipixError;
    // mui_handler_.fcns.processFrameMeasurementFinished = &mui_linux_processMeasurementFinished;
    mui_handler_.fcns.processChipVoltage              = &mui_linux_processChipVoltage;
    mui_handler_.fcns.sendString                      = &mui_linux_sendString;

    mui_initialize(&mui_handler_);

    // | --------------- create file for saving data -------------- |

    measured_data_file_ = fopen(data_path.c_str(), "w");

    if (measured_data_file_ == nullptr) {
        printf("Error: cannot open the data output file '%s' for writing!\n", data_path.c_str());
    }


    printf("measure temperature\n");
    serial_port_minipix_.activate(true);
    
    mui_getTemperature(&mui_handler_);
    rc = read_response();

    mui_getTemperature(&mui_handler_);
    rc = read_response();

    mui_getTemperature(&mui_handler_);
    rc = read_response();    

    serial_port_minipix_.activate(false);
    printf("rc = %d\n", rc);
    printf("finished measure temperature\n");


    // printf("measure chip voltage\n");
    // serial_port_minipix_.activate(true);
    // mui_getChipVoltage(&mui_handler_);
    // rc = read_response();
    // serial_port_minipix_.activate(false);
    // printf("rc = %d\n", rc);
    // printf("finished measure chip voltage\n");

    // printf("power on the device\n");
    // serial_port_minipix_.activate(true);
    // mui_pwr(&mui_handler_, 1);  
    // rc = read_response();
    // printf("rc = %d\n", rc);
    // serial_port_minipix_.activate(false);
    // printf("power on the device finished\n");




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