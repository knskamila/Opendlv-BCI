/*
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

struct Param
{
  cluon::OD4Session* od4;
  void* subscriber;
  std::string SURFACE_NAME;
  bool VERBOSE;
};

int32_t main(int32_t argc, char **argv)
{
  int32_t retCode{0};
  auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
  if (0 == commandlineArguments.count("cid") || 0 == commandlineArguments.count("ip") || 0 == commandlineArguments.count("surface") || 0 == commandlineArguments.count("tcp")) {
    std::cerr << argv[0] << " is an OpenDLV interface." << std::endl;
    std::cerr << "Usage:   " << argv[0] << " --cid=<Session id> --ip=<Remote ip or localhost> --tcp=<port> --simulate --verbose" << std::endl;
    std::cerr << "Example: " << argv[0] << " --cid=111 --ip=127.0.0.1 --tcp=50020" << std::endl;
    retCode = 1;
  } else {
    bool const VERBOSE{commandlineArguments.count("verbose") != 0};
    bool const SIMULATE{commandlineArguments.count("simulate") != 0};
    uint16_t const CID = static_cast<uint16_t>(std::stoi(commandlineArguments["cid"]));
    std::string const IP = commandlineArguments["ip"];
    uint16_t const TCP_PORT = static_cast<uint16_t>(std::stoi(commandlineArguments["tcp"]));

    cluon::OD4Session od4{CID};
    
    if(SIMULATE)
    {
      
    }

    std::string const ZMQ_ADDRESS = "tcp://" + IP + ":" + std::to_string(TCP_PORT);
    
    void *context = zmq_ctx_new();
    void *subscriber = zmq_socket(context, ZMQ_REQ);
    zmq_connect(subscriber, ZMQ_ADDRESS.c_str());
    
    char sub1[7];
    zmq_send(subscriber, "SUB_PORT", strlen("SUB_PORT"), 0); //request for sub port
    zmq_recv(subscriber, sub1, 7, 0);
    std::cout << "sub port 1: " << sub1 << std::endl;
      

    while(od4.isRunning())
    {
    }

  }
  return retCode;
}

