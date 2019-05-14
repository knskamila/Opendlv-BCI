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

#ifndef P300_DETECTOR
#define P300_DETECTOR

#include <mutex>
#include <sstream>
#include <fftw3.h>

#define FREQUENCY 250
#define MAX_CHANNELS 16

class P300Detector {
 public:
  P300Detector() = delete;
  P300Detector(double**, size_t, size_t) noexcept;
  ~P300Detector() = default;

 public:
  double detect() noexcept;
  
 private:
  fftw_plan  plan_pre{0}, plan_post{0};
  fftw_complex* pre_output_buffer = nullptr;
  fftw_complex* post_output_buffer = nullptr;
  size_t first_300_ms_length{0};
  size_t post_300_ms_length{0};
  size_t channels{1};
  size_t bins{1};
  uint16_t pre_20hz_cutoff{0};
  uint16_t post_20hz_cutoff{0};
  uint16_t pre_output_size{1};
  uint16_t post_output_size{1};
  double* arrayPre = nullptr;
  double* arrayPost = nullptr;
  double** eegArray = nullptr;
  mutable std::mutex m_resultsMutex{};
  mutable std::mutex m_dataMutex{};
  //unsigned int flags[] = {FFTW_ESTIMATE, FFTW_FORWARD};
  //mutable std::mutex m_dataMutex[MAX_CHANNELS]{};
};

#endif

