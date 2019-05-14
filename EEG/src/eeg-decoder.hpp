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

#ifndef EEG_DECODER
#define EEG_DECODER

#include "opendlv-standard-message-set.hpp"
#include "ring_span.hpp"

#include <functional>
#include <mutex>
#include <sstream>
#include <vector>

#define SAMPLE_RATE 250
#define GAIN 24
#define CHANNEL_TOTAL 8
#define EEG_BYTES 3*CHANNEL_TOTAL


class EEGDecoder {
 public:
  enum EEGMessages {
    UNKNOWN     = 0,
    HEADER_EEG	= 0xA0,
    HEADER_ACC  = 0xC0,
  };

  enum EEGBytes {
    START    	= 0x62,
    INIT    	= 0x24,
    RESET       = 0x40,
    STOP        = 0x73,
    //SYNC_BYTE0  = 0xA5,
    //SYNC_BYTE1  = 0x5A,
  };
  
 const char ACTIVATE_CHANNEL[8] = {'!', '@', '#', '$', '%', '^', '&', '*'};
 const char DEACTIVATE_CHANNEL[8] = {'1', '2', '3', '4', '5', '6', '7', '8'};
 
 uint16_t valuesPerPacket{8};

 public:
  EEGDecoder() = delete;
  EEGDecoder(nonstd::ring_span_lite::ring_span<double>**, size_t, size_t) noexcept;
  ~EEGDecoder() = default;

 public:
  size_t decode(const uint8_t *buffer, const size_t size) noexcept;

 public:
  bool getStatus() const noexcept;
  bool dataReady() const noexcept;
  double getLastEEG(int) noexcept;
  void readData(double**) noexcept;
  void reset() noexcept;
  std::vector<std::vector<double>>* readData() noexcept;
  
 private:
  bool initScan(const uint8_t *buf, const size_t offset) noexcept;
  double translateValue(int32_t raw);
  
 private:
  bool m_initialized{false};
  bool data_ready{false};
  size_t buffer_len{0};
  size_t channels_no{1};
  nonstd::ring_span_lite::ring_span<double>** m_buffers = nullptr;
  //int32_t* outputs[CHANNEL_NO] = nullptr;
  //uint16_t received{0};
  uint8_t internal_sample_counter{0};

 private:
  mutable std::mutex m_dataMutex{};
  mutable std::mutex m_bufferMutex{};
  std::vector<std::vector<double>>* outputVectors = nullptr;
};

#endif

