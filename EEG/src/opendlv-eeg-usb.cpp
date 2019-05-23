/*
 * Copyright (C) 2019 Kamila Kowalska
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cluon-complete.hpp"

#include "opendlv-standard-message-set.hpp"
#include "eeg.hpp"
#include "p300-detector.hpp"

#include <iostream>

int32_t main(int32_t argc, char **argv) {
  int32_t retCode{1};

  auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
  if ( (0 == commandlineArguments.count("cid")) ||
       (0 == commandlineArguments.count("bins")) ||
       (0 == commandlineArguments.count("freq")) ||
       (0 == commandlineArguments.count("device")) ) {
    std::cerr << argv[0] << " connects to an OpenBCI circuit board, analyses and sends the signal as ???." << std::endl;
    std::cerr << "Usage:   " << argv[0] << " --cid=<OD4 session> --device=<serial port to open> [--verbose]" << std::endl;
    std::cerr << "         --cid:    CID of the OD4Session to send and receive messages" << std::endl;
    std::cerr << "         --device: serial port where the dongle is attached" << std::endl;
    std::cerr << "         --bins: number of bins (measurements in a buffer) for FFT" << std::endl;
    std::cerr << "         --freq: how often the value is returned (ms)" << std::endl;
    std::cerr << "Example: " << argv[0] << " --cid=111 --device=/dev/ttyUSB0 --bins=128 --channels=3 --freq=50 --verbose" << std::endl;
  }
  else {
    const std::string DEVICE{commandlineArguments["device"]};
    const bool VERBOSE{commandlineArguments.count("verbose") != 0};
    const size_t BINS{stoi(commandlineArguments["bins"])};
    const size_t CHANNELS{stoi(commandlineArguments["channels"])};
    const int FREQ{stoi(commandlineArguments["freq"])};
    
    /*TEST*/
    /*
    double* testArray[1];
    std::cout << "test" << std::endl;
    testArray[0] = new double [128] {1037,   1068,   1090,   1100,   1095,   1077,   1048,   1013,    975,    941,    916,
    902,    902,    916,    941,    975,   1013,   1048,   1077,   1095,   1100,   1090,
   1068,   1037,  1000,    963,    932,    910,    900,    905,    923,    952,    987,
   1025,   1059,   1084,   1098,   1098,   1084,   1059,   1025,    987,    952,    923,
    905,    900,    910,    932,    963,   1000,   1037,   1068,   1090,   1100,   1095,
   1077,   1048,   1013,    975,    941,    916,    902,    902,    916,    941,    975,
   1013,   1048,   1077,   1095,   1100,   1090,   1068,   1037,
   1000,    945,    897,    864,    850,    857,    884,    928,    981,   1037,   1088,
   1127,   1147,   1147,   1127,   1088,   1037,    981,    928,    884,    857,    850,
    864,    897,    945,   1000,   1055,   1103,   1136,   1150,   1143,   1116,   1072,
   1019,    963,    912,    873,    853,    853,    873,    912,    963,   1019,   1072,
   1116,   1143,   1150,   1136,   1103,   1055,   1000,    945,    897,    864};
    P300Detector test(testArray, 1, 128);
    std::cout << "test returned: " << test.detect() << std::endl;
    * */
    /*/TEST*/
    
    std::cout << "Waiting for initialization signal...";
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
 
    double* eegArray[CHANNELS];
    for (size_t i = 0; i < CHANNELS; i++){
      eegArray[i] = new double [BINS];
    }
    EEG eeg(DEVICE, CHANNELS, BINS);
    P300Detector p300(eegArray, CHANNELS, BINS);
    
    if (eeg.isOpen()) {
      cluon::OD4Session od4{static_cast<uint16_t>(std::stoi(commandlineArguments["cid"]))};

      while(!eeg.isInitialized())
      {
        std::cout << ".";
	std::this_thread::sleep_for(std::chrono::seconds(1));
      }
      std::cout << std::endl;
      eeg.stop(); //to make sure everything works as intended;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      eeg.start();

      while (od4.isRunning()) {
	/*it is unnecessary to check for P300 too often */
        std::this_thread::sleep_for(std::chrono::milliseconds(FREQ));
        if(eeg.dataReady())
        {
          eeg.readData(eegArray);
		  
          /* Microservice sends RATIO between the power spectrum 1-20 Hz of
           * the FIRST 300 ms of the signal and the remaining part of the buffer.
           * Buffer length is specified by BINS command. */
          float difference = static_cast<float>(p300.detect());
           
          if(VERBOSE)
            std::cout << "difference: " << difference << std::endl;
           
	  opendlv::proxy::VoltageReading p300Difference;
          p300Difference.voltage(difference);
          od4.send(p300Difference);
	}
      }
      std::cout << "Stopping stream..." << std::endl;
      eeg.stop();
      retCode = 0;
    }
    else {
      std::cerr << "[opendlv-eeg-usb]: Failed to open " << DEVICE << std::endl;
    }
  }
  return retCode;
}

