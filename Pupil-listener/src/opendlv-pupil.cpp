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
#include <unistd.h>

#include <zmq.h>
#include <json.hpp>
#include <msgpack.hpp>

#include "cluon-complete.hpp"
#include "opendlv-standard-message-set.hpp"

int32_t main(int32_t argc, char **argv)
{
  int32_t retCode{0};
  auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
  if (0 == commandlineArguments.count("cid") || 0 == commandlineArguments.count("ip") || 0 == commandlineArguments.count("surface") || 0 == commandlineArguments.count("tcp")) {
    std::cerr << argv[0] << " is an OpenDLV interface." << std::endl;
    std::cerr << "Usage:   " << argv[0] << " --cid=<Session id> --ip=<Remote ip or localhost> --surface=<Name of the surface to map within> --tcp=<Pupil remote port> --simulate --verbose" << std::endl;
    std::cerr << "Example: " << argv[0] << " --cid=111 --ip=127.0.0.1 --surface=screen --tcp=50020" << std::endl;
    retCode = 1;
  } else {
    bool const VERBOSE{commandlineArguments.count("verbose") != 0};
    bool const SIMULATE{commandlineArguments.count("simulate") != 0};
    uint16_t const CID = static_cast<uint16_t>(std::stoi(commandlineArguments["cid"]));
    std::string const IP = commandlineArguments["ip"];
    std::string const SURFACE_NAME = commandlineArguments["surface"];
    uint16_t const TCP_PORT = static_cast<uint16_t>(std::stoi(commandlineArguments["tcp"]));

    cluon::OD4Session od4{CID};
    std::mutex threadMutex;
    
    if(SIMULATE)
    {
      std::cout << "Simulating Pupil input. Doesn't send fixation messages." << std::endl;
      srand(1234);
      float max = static_cast<float>(RAND_MAX);
      float x = 0.4f;
      float y = 0.5f;
      while(od4.isRunning())
      {
        opendlv::logic::sensation::Point gaze;
        gaze.azimuthAngle(x);
        gaze.zenithAngle(y);

        if(x > 0 && x < 1 && y < 1 && y > 0) gaze.distance(1);
        else gaze.distance(0);
        od4.send(gaze);

        if(VERBOSE) std::cout << "x: " << x << " y: " << y << std::endl;
      
        x += 0.05f*(static_cast<float>(rand() - max/2)/max);
        y += 0.05f*(static_cast<float>(rand() - max/2)/max);
	usleep(100000);
      }
    }

    std::string const ZMQ_ADDRESS = "tcp://" + IP + ":" + std::to_string(TCP_PORT);
    
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
    zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "surface", strlen("surface")); //receive surface msgs

    void *context_f = zmq_ctx_new();
    void *subscriber_f = zmq_socket(context_f, ZMQ_REQ);
    zmq_connect(subscriber_f, ZMQ_ADDRESS.c_str());
    
    char sub2[7];
    zmq_send(subscriber_f, "SUB_PORT", strlen("SUB_PORT"), 0);
    zmq_recv(subscriber_f, sub2, 7, 0);
    std::cout << "sub port 2: " << sub2 << std::endl;

    subscriber_f = zmq_socket(context, ZMQ_SUB);
    std::string const ZMQ_ADDRESS_SUB_F = "tcp://" + IP + ":" + sub2;
    zmq_connect(subscriber_f, ZMQ_ADDRESS_SUB_F.c_str());
    zmq_setsockopt(subscriber_f, ZMQ_SNDTIMEO, "fixation", strlen("fixation")); //receive fixation msgs

    std::thread gaze_thread([&od4, &subscriber, &SURFACE_NAME, &VERBOSE, &threadMutex]()
    {
      int32_t input_length = 0;
      while(od4.isRunning())
      {
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
      
        if(INPUT_JSON["name"] == SURFACE_NAME) //ignore other surfaces
        {
          nlohmann::json pos = INPUT_JSON["gaze_on_srf"][0]["norm_pos"];
          float const x = static_cast<float>(pos[0]);
          float const y = 1 - static_cast<float>(pos[1]);
          bool active = ((x > 0 && x < 1 && y < 1 && y > 0) ? 1 : 0);
      
          opendlv::logic::sensation::Point gaze;
          gaze.azimuthAngle(x);
          gaze.zenithAngle(y);
          gaze.distance(active);
          
          std::lock_guard<std::mutex> lck(threadMutex);
          od4.send(gaze);

          if (VERBOSE) {
	    std::cout << "Gaze x: " << x << std::endl;
	    std::cout << "Gaze y: " << y << std::endl;
          }
        }  
    }});

    std::thread fixation_thread([&od4, &subscriber_f, &VERBOSE, &threadMutex]()
    {
 
      int32_t input_length = 0;
      bool fixation = false;

      while(od4.isRunning())
      {
        char msg[16384];
        fixation = false;
        zmq_recv(subscriber_f, msg, 16384, ZMQ_DONTWAIT); //topic discarded
        input_length = zmq_recv(subscriber_f, msg, 16384, ZMQ_DONTWAIT);
    
        if (input_length > 0)
        {
          fixation = true; //no other information needed; fixation occurred
        }

        std::lock_guard<std::mutex> lck(threadMutex);
        {
          if (VERBOSE && fixation) std::cout << "fixation" << std::endl;

          opendlv::proxy::SwitchStateReading fix;
          fix.state(fixation);
          od4.send(fix);
        }

        usleep(1000);
      }
    });

    gaze_thread.join();
    fixation_thread.join();

  }
  return retCode;
}
