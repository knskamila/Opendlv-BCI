/*
 * Copyright (C) 2018  Christian Berger
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

#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"

#include "eeg-decoder.hpp"
#include "ring_span.hpp"

#include <vector>

#define CHANNEL_NO 1

const std::vector<uint8_t> INFO_BYTES {
  0xAB, 0xa5, 0x5a, 0x24, 0x24, 0x24, 0x0, 0x4, 0x0,
  0xf, 0x1, 0x0, 0xff, 0xef, 0xe7, 0xf2, 0xc1,
  0xe2, 0x9b, 0xf3, 0xc0, 0xe3, 0x9e, 0xf6, 0x24,
  0x24, 0x24, 0x24, 0xA0, 0x0, 0xe3, 0xe3, 0xe3, 0xe3, 0xe3, 0xe3,
  0xe3, 0xe3, 0xe3, 0xe3, 0xe3, 0xe3,
  0xe3, 0xe3, 0xe3, 0xe3, 0xe3, 0xe3,
  0xe3, 0xe3, 0xe3, 0xe3, 0xe3, 0xe3, 0xC0, 0x0, 0x0, 0x0
};

const std::vector<uint8_t> INFO_BYTES2 {
  0xAB, 0xa5, 0x5a, 0x24, 0x24, 0x5a, 0x0, 0x4, 0x0,
  0xf, 0x1, 0x0, 0xff, 0xef, 0xe7, 0xf2, 0xc1,
  0xe2, 0x9b, 0xf3, 0xc0, 0xe3, 0x9e, 0xf6, 0xe,
  0x24, 0x2, 0x24
};

TEST_CASE("Test INIT") {
  const static size_t buffer_len{5};
  double* arr[CHANNEL_NO];
  nonstd::ring_span_lite::ring_span<double>* buffers[CHANNEL_NO];

  for (size_t i = 0; i < CHANNEL_NO; i++)
  {
    arr[i] = new double[buffer_len];
    buffers[i] = new nonstd::ring_span_lite::ring_span<double>(arr[i], arr[i] + buffer_len, arr[i], buffer_len);
  }

  EEGDecoder decoder(buffers, CHANNEL_NO, buffer_len);

  decoder.decode(INFO_BYTES.data(), INFO_BYTES.size());
  {
     REQUIRE(true == decoder.getStatus());
     //REQUIRE(decoder.getLastEEG(0) == 333);
  }

  //decoder.decode(INFO_BYTES2.data(), INFO_BYTES2.size());
  //{
  //   REQUIRE(false == decoder.getStatus());
  //}
}
