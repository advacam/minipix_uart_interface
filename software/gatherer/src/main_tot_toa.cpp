//Program for frame acq in tot and toa mode
//
//Following parameters can be changed:
//  *Acq time
//  *Configuration
//
//Exported results:
//  *Data in frame format of tot and toa in ASCII
//  *Temperature in separate file (name of the frame file?)


#include <string>
#include <chrono>

#include <gatherer.h>

using namespace std::chrono;

void sleep(const double s) {

  std::this_thread::sleep_for(std::chrono::milliseconds(int(s * 1000)));
}

int main(int argc, char* argv[]) {

  std::string serial_port_file;
  int         baud_rate;
  bool        serial_port_virtual;
  std::string data_path;
  int         acq_time;
  int         frame_count;
  int         conf_num;

  if (argc == 8) {

    serial_port_file    = argv[1];
    baud_rate           = atoi(argv[2]);
    serial_port_virtual = atoi(argv[3]);
    data_path           = argv[4];
    acq_time            = atoi(argv[5]);
    frame_count         = atoi(argv[6]);
    conf_num            = atoi(argv[7]);

    printf(
        "\nloaded params: \n \
serial port:\t\t'%s'\n \
baud rate:\t\t'%d'\n \
serial port is:\t'%s'\n \
output data path:\t'%s'\n \
acqusition time [ms]:\t'%d'\n \
frame count:\t\t'%d'\n \
config number:\t\t'%d'\n",

        serial_port_file.c_str(), baud_rate, serial_port_virtual ? "virtual" : "real", data_path.c_str(), acq_time, frame_count, conf_num);
  } else {
    printf("params not supplied!\n");
    printf("required: ./gatherer <serial port file> <baud rate> <serial port virtual ? true : false> <output data path> <acq time> <frame count> <configuration num>\n");
    return 0;
  }

  // initialize the gatherer
  Gatherer gatherer(data_path);

  // open the serial line
  gatherer.connect(serial_port_file, baud_rate, serial_port_virtual);

  auto millisec_since_epoch = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
  printf("\nstart time of meas:\t\t%.0f\n", (double)millisec_since_epoch );

  sleep(0.01);

  printf("getting status\n");
  gatherer.getStatus();

  sleep(0.01);

  printf("getting temperature\n");
  gatherer.getTemperature();

  sleep(0.01);

  printf("getting chip voltage\n");
  gatherer.getChipVoltage();

  sleep(0.01);

  printf("getting fw version\n");
  gatherer.getFwVer();

  sleep(0.01);

  // power up cycling
  int cycle_count = 10;
  int cycle_current = 0;
  double wait_for_power_up = 5.; // wait in seconds to get reposnse about error or not error of power up
  do
  {
    cycle_current++;
    printf("power up main, try: %d\n", cycle_current);
    gatherer.pwr(true);
    sleep(wait_for_power_up);
    if(cycle_current > cycle_count){
      printf("failed to power up device\n");
      exit(1);
    }
  } while (gatherer.power_up_failed_);
  
  sleep(0.01);


  /* printf("masking pixels\n"); */
  /* int square_size = 20; */
  /* for (int i = 128 - int(square_size / 2.0); i < 128 + int(square_size / 2.0); i++) { */
  /*   for (int j = 245 - int(square_size / 2.0); j < 245 + int(square_size / 2.0); j++) { */

  /*     bool state = true; */

  /*     gatherer.maskPixel(i, j, state); */
  /*     printf("%s pixel x: %d, y: %d\n", state ? "masking" : "unmasking", i, j); */
  /*   } */
  /* } */

  /* printf("setting threshold"); */
  /* gatherer.setThreshold(333, 555); */

  /* printf("setting configuration preset 0\n"); */

  // if(conf_num != 1 && conf_num != 0) 
  //   conf_num = 0;


  printf("setting configuration preset %d\n", conf_num);
  gatherer.setConfigurationPreset(conf_num);

  printf("getting chip voltage\n");
  gatherer.getChipVoltage();

  sleep(0.01);


  // printf("initial temperature:\t%d\n", gatherer.temperature_curr );

  millisec_since_epoch = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
  printf("start of taking frames:\t%.0f\n", (double)millisec_since_epoch );

  for (int i = 0; i < frame_count; i++) {

    //gatherer.getTemperature();
    // printf("measuring frame in TOA TOT:\t%d\n", i);
    //sleep(0.01);
    // gatherer.setAcqTime(acq_time);
    gatherer.measureFrame(acq_time, LLCP_TPX3_PXL_MODE_TOA_TOT);
    sleep(0.01);
  }

   millisec_since_epoch = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
  printf("end of meas:\t%.0f\n", (double)millisec_since_epoch );
    

  printf("powering off\n");
  gatherer.pwr(false);

  // this will stop the threads and disconnect the uart
  gatherer.stop();

  printf("finished\n\n");

  return 0;
}
