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

#include <iostream>
#include <thread>
#include <cstring>
#include <cstdlib>

#include <zmq.h>
#include <json.hpp>
#include <msgpack.hpp>

#include "cluon-complete.hpp"
#include "opendlv-standard-message-set.hpp"

int32_t main(int32_t argc, char **argv)
{
  int32_t retCode{0};
  auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
  if (0 == commandlineArguments.count("cid") ||
      0 == commandlineArguments.count("ip") ||
      0 == commandlineArguments.count("tcp")) {
      std::cerr << argv[0] << " is an OpenDLV interface." << std::endl;
      std::cerr << "Usage:   " << argv[0] << " --cid=<Session id> --ip=<Remote ip or localhost> --tcp=<Pupil Remote port> " << std::endl;
      std::cerr << "--acceleration=<Acceleration range> (optional)" << std::endl;
      std::cerr << "--reverseacc=<Reverse acceleration range> (optional)" << std::endl;
      std::cerr << "--angle<Steering angle maximum> (optional)" << std::endl;
      std::cerr << "--eeg=<EEG signal threshold trigger> (optional)" << std::endl;
      std::cerr << "--reverserange=<Upper threshold value for reversing (0.0, 1.0)> (optional)" << std::endl;
      std::cerr << "--deadrange=<Upper threshold value for idle (reverserange, 1.0)> (optional)" << std::endl;
      std::cerr << "--donotsend --verbose" << std::endl;
      std::cerr << "Example: " << argv[0] << " --cid=111 --ip=127.0.0.1 --tcp=50020 --eeg=0 --angle=0.5 --acceleration=47 --deadrange=0.4 --reverserange=0.2" << std::endl;
      retCode = 1;
  } else {
    bool const VERBOSE{commandlineArguments.count("verbose") != 0};
    bool const DONOTSEND{commandlineArguments.count("donotsend") != 0};
    float const REVERSE_RANGE{(commandlineArguments.count("reverserange") != 0) ? std::stof(commandlineArguments["reverserange"]) : 0.3f};
    float const REVERSE_ACC{(commandlineArguments.count("reverseacc") != 0) ? std::stof(commandlineArguments["reverseacc"]) : -5.f};
    float const DEAD_RANGE{(commandlineArguments.count("deadrange") != 0) ? std::stof(commandlineArguments["deadrange"]) : 0.5f};
    float const ANGLE{(commandlineArguments.count("angle") != 0) ? std::stof(commandlineArguments["angle"]) : 2.5f};
    float const ACCELERATION{(commandlineArguments.count("acceleration") != 0) ? std::stof(commandlineArguments["acceleration"]) : 20.f};
    uint16_t const CID = static_cast<uint16_t>(std::stoi(commandlineArguments["cid"]));
    uint16_t const EEG{(commandlineArguments.count("eeg") != 0) ? static_cast<uint16_t>(std::stoi(commandlineArguments["eeg"])) : static_cast<uint16_t>(0)};
    std::string const IP = commandlineArguments["ip"];
    uint16_t const TCP_PORT = static_cast<uint16_t>(std::stoi(commandlineArguments["tcp"]));

    cluon::OD4Session od4{CID};
    std::mutex threadMutex;
    std::mutex fixationMutex;
    bool fixation{false};
    float currentPotential{0};

    auto onEEG = [&currentPotential, &VERBOSE](cluon::data::Envelope &&env){
      auto senderStamp = env.senderStamp();
      if (senderStamp == 5)
      {
        opendlv::proxy::VoltageReading current = cluon::extractMessage<opendlv::proxy::VoltageReading>(std::move(env));
        //std::lock_guard<std::mutex> lck(eegMutex);
        currentPotential = current.voltage();
        if(VERBOSE) std::cout << "Current potential: " << currentPotential << std::endl;
      }
    };

    od4.dataTrigger(opendlv::proxy::VoltageReading::ID(), onEEG);
    std::string const ZMQ_ADDRESS = "tcp://" + IP + ":" + std::to_string(TCP_PORT);

    std::thread gaze_thread([&od4, &ZMQ_ADDRESS, &IP, &VERBOSE, &DONOTSEND, &REVERSE_ACC, &ANGLE, &ACCELERATION, &REVERSE_RANGE, &EEG, &currentPotential, &DEAD_RANGE, &threadMutex, &fixationMutex, &fixation]()
    {
      void *context = zmq_ctx_new();
      void *subscriber = zmq_socket(context, ZMQ_REQ);
      zmq_connect(subscriber, ZMQ_ADDRESS.c_str());

      char sub1[7];
      zmq_send(subscriber, "SUB_PORT", strlen("SUB_PORT"), 0); //request for sub port
      zmq_recv(subscriber, sub1, 7, 0);
      std::cout << "sub port 1: " << sub1 << std::endl;

      subscriber = zmq_socket(context, ZMQ_SUB);
      std::string const ZMQ_ADDRESS_SUB = "tcp://" + IP + ":" + sub1;
      zmq_connect(subscriber, ZMQ_ADDRESS_SUB.c_str());
      zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "gaze", strlen("gaze")); //receive gaze msgs

      int32_t input_length = 0;
      while(od4.isRunning())
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        char msg[16384];
        zmq_recv(subscriber, msg, 16384, 0); // topic, discarded
        input_length = zmq_recv(subscriber, msg, 16384, 0);

        msgpack::object_handle message;
        msgpack::unpack(message, msg, input_length);
        msgpack::object obj = message.get();

        std::stringstream buffer;
        obj = message.get();
        buffer << obj;

        auto const INPUT_JSON = nlohmann::json::parse(buffer);

        if(!INPUT_JSON.empty())
        {
          if (!INPUT_JSON["norm_pos"].empty())
          {
            //if (VERBOSE) std::cout << "gaze: " << INPUT_JSON["norm_pos"] << std::endl;
            nlohmann::json pos = INPUT_JSON["norm_pos"];
            float const x = static_cast<float>(pos[0]);
            float const y = 1 - static_cast<float>(pos[1]);
            bool active = ((x > 0 && x < 1 && y < 1 && y > 0) ? 1 : 0);
            //if (VERBOSE && !active) std::cout << "(not active)" << std::endl;

            if(fixation)
            {
              float steering = 2*x - 1;
              float y_s = 1 - y;
              float acceleration;
              if(y_s < REVERSE_RANGE) acceleration = (REVERSE_RANGE-y_s)/REVERSE_RANGE*REVERSE_ACC;
              else if(y_s < DEAD_RANGE) acceleration = 0;
              else acceleration = (y_s-REVERSE_RANGE)/(1-REVERSE_RANGE);

              opendlv::proxy::ActuationRequest actuation;
              cluon::data::TimeStamp sampleTime;
	      actuation.acceleration(acceleration*ACCELERATION);
              actuation.steering(-steering*ANGLE);
              actuation.isValid(active);

              {
                std::lock_guard<std::mutex> lck1(threadMutex);
                std::lock_guard<std::mutex> lck2(fixationMutex);

                if(!DONOTSEND && fixation && currentPotential >= EEG) od4.send(actuation, sampleTime, 10);
                if (VERBOSE) {
	          std::cout << "Steering: " << actuation.steering() << std::endl;
	          std::cout << "Acceleration: " << actuation.acceleration() << std::endl;
                  std::cout << "Active: " << actuation.isValid() << std::endl;
                }
              }
            }
            else
            {
	      opendlv::logic::sensation::Point gaze;
              cluon::data::TimeStamp sampleTime;
	      gaze.azimuthAngle(x);
              gaze.zenithAngle(y);
              gaze.distance(active);

              {
                std::lock_guard<std::mutex> lck(threadMutex);
                od4.send(gaze, sampleTime, 1);
              }
            }

           // if (VERBOSE) {
	   //   std::cout << "Gaze x: " << x << std::endl;
	   //   std::cout << "Gaze y: " << y << std::endl;
           // }
          }
        }
    }});

    std::thread fixation_thread([&od4, &ZMQ_ADDRESS, &IP, &VERBOSE, &threadMutex, &fixationMutex, &fixation]()
    {

      void *context_f = zmq_ctx_new();
      void *subscriber_f = zmq_socket(context_f, ZMQ_REQ);
      zmq_connect(subscriber_f, ZMQ_ADDRESS.c_str());

      char sub2[7];
      zmq_send(subscriber_f, "SUB_PORT", strlen("SUB_PORT"), 0);
      zmq_recv(subscriber_f, sub2, 7, 0);
      std::cout << "sub port 2: " << sub2 << std::endl;

      subscriber_f = zmq_socket(context_f, ZMQ_SUB);
      std::string const ZMQ_ADDRESS_SUB_F = "tcp://" + IP + ":" + sub2;
      zmq_connect(subscriber_f, ZMQ_ADDRESS_SUB_F.c_str());
      zmq_setsockopt(subscriber_f, ZMQ_SUBSCRIBE, "fixation", strlen("fixation")); //receive fixation msgs

      int32_t input_length = 0;

      while(od4.isRunning())
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        char msg[16384];

        zmq_recv(subscriber_f, msg, 16384, ZMQ_DONTWAIT); //topic discarded
        input_length = zmq_recv(subscriber_f, msg, 16384, ZMQ_DONTWAIT);

        if (input_length > 0)
        {
          std::lock_guard<std::mutex> lck(fixationMutex);
          fixation = true; //no other information needed; fixation occurred
        }

        {
          std::lock_guard<std::mutex> lck(threadMutex);
          //if (VERBOSE) std::cout << "fixation: " << fixation << std::endl;

          opendlv::proxy::SwitchStateReading fix;
          fix.state(fixation);
          od4.send(fix);
        }
      }
    });

    std::thread reset_thread([&od4, &fixationMutex, &fixation]()
    {
      while(od4.isRunning())
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        {
          std::lock_guard<std::mutex> lck(fixationMutex);
          fixation = false; //reset
        }
      }
    });

    reset_thread.join();
    fixation_thread.join();
    gaze_thread.join();


  }
  return retCode;
}

