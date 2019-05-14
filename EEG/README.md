## OpenDLV Microservice to interface with RPLidar units

This repository provides source code to interface with an RPLidar
unit for the OpenDLV software ecosystem.

[![Build Status](https://travis-ci.org/chalmers-revere/opendlv-device-lidar-rplidar.svg?branch=master)](https://travis-ci.org/chalmers-revere/opendlv-device-lidar-rplidar) [![License: GPLv3](https://img.shields.io/badge/license-GPL--3-blue.svg
)](https://www.gnu.org/licenses/gpl-3.0.txt)


## Table of Contents
* [Dependencies](#dependencies)
* [Usage](#usage)
* [Build from sources on the example of Ubuntu 16.04 LTS](#build-from-sources-on-the-example-of-ubuntu-1604-lts)
* [License](#license)
* [PointCloudReading data structure](#pointcloudreading-data-structure)


## Dependencies
No dependencies! You just need a C++14-compliant compiler to compile this
project as it ships the following dependencies as part of the source distribution:

* [libcluon](https://github.com/chrberger/libcluon) - [![License: GPLv3](https://img.shields.io/badge/license-GPL--3-blue.svg
)](https://www.gnu.org/licenses/gpl-3.0.txt)
* [wjwwood/serial](https://https://github.com/wjwwood/serial) - [![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg
)](https://opensource.org/licenses/MIT)
* [Unit Test Framework Catch2](https://github.com/catchorg/Catch2/releases/tag/v2.1.2) - [![License: Boost Software License v1.0](https://img.shields.io/badge/License-Boost%20v1-blue.svg)](http://www.boost.org/LICENSE_1_0.txt)


## Usage
This microservice is created automatically on changes to this repository via Docker's public registry for:
* [x86_64](https://hub.docker.com/r/chalmersrevere/opendlv-device-lidar-rplidar-amd64/tags/)
* [armhf](https://hub.docker.com/r/chalmersrevere/opendlv-device-lidar-rplidar-armhf/tags/)
* [aarch64](https://hub.docker.com/r/chalmersrevere/opendlv-device-lidar-rplidar-aarch64/tags/)

To run this microservice using our pre-built Docker multi-arch images to connect
to an RPLidar unit providing data via a serial USB connectiong like `/dev/ttyUSB0` and to publish
the messages according to OpenDLV Standard Message Set into session 111 in
Google Protobuf format, simply start it as follows:

```
docker run --init --rm --net=host --device=/dev/ttyUSB0 chalmersrevere/opendlv-device-lidar-rplidar-multi:v0.0.4 --device=/dev/ttyUSB0 --cid=111 --verbose
```

## Build from sources on the example of Ubuntu 16.04 LTS
To build this software, you need cmake, C++14 or newer, and make. Having these
preconditions, just run `cmake` and `make` as follows:

```
mkdir build && cd build
cmake -D CMAKE_BUILD_TYPE=Release ..
make && make test && make install
```


## License

* This project is released under the terms of the GNU GPLv3 License


## PointCloudReading data structure

`opendlv-device-lidar-rplidar` receives the data from an RPLidar as input and
tranforms the payload into a more compact [PointCloudReading](https://github.com/chalmers-revere/opendlv.standard-message-set/blob/6c6ac1f893ab181bbcd7c8a913ad117067fd6f4c/opendlv.odvd#L165-L178)
representation.
