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

#ifndef EEG_
#define EEG_

#include "opendlv-standard-message-set.hpp"
#include "serialport.hpp"

#include "eeg-decoder.hpp"

#include <functional>
#include <memory>
#include <thread>

class EEG {
 private:
  EEG(const EEG &) = delete;
  EEG(EEG &&)      = delete;
  EEG &operator=(const EEG &) = delete;
  EEG &operator=(EEG &&) = delete;

 public:
  EEG(const std::string &device, const size_t, const size_t) noexcept;
  ~EEG();

 public:
  bool isOpen() const noexcept;
  bool isInitialized() const noexcept;
  bool dataReady() const noexcept;
  void start() noexcept;
  void stop() const noexcept;
  std::vector<std::vector<double>>* readData();
  void readData(double**);

 private:
  std::unique_ptr<serial::Serial> m_eegDevice{nullptr};
  std::unique_ptr<std::thread> m_readingBytesFromDeviceThread{nullptr};
  double** value_array = nullptr;
  nonstd::ring_span_lite::ring_span<double>** buffers = nullptr;
  //bool data_ready{false};

  EEGDecoder* m_decoder = nullptr;
};

#endif

