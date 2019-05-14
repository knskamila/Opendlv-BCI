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

#include "eeg-decoder.hpp"
#include "ring_span.hpp"

#include <sstream>
#include <iostream>
#include <string>
#include <stdlib.h>

EEGDecoder::EEGDecoder(nonstd::ring_span_lite::ring_span<double>** buffers, size_t channels, size_t len) noexcept
{
  m_buffers = buffers;
  buffer_len = len;
  channels_no = channels;
  //std::vector<std::vector<double>> vectors(CHANNEL_NO, std::vector<double>(buffer_len));
  //outputVectors = &vectors;
}

bool EEGDecoder::getStatus() const noexcept {
  std::lock_guard<std::mutex> lck(m_dataMutex);
  return m_initialized;
}

size_t EEGDecoder::decode(const uint8_t *buffer, const size_t size) noexcept {
  size_t offset{0};
  //int sample_counter = 0;
  while (true) {
    if (!getStatus())
    {
      if (offset + 3 > size) { //shorter than 3 bytes
        return size;
      }

      if (initScan(buffer, offset)) {
		std::cout << buffer[offset] << buffer[offset+1] << buffer[offset+2] << std::endl;
        offset += 3;
      }
      else {
        offset++;
      }
    }
    else
    {
	  if (offset + EEG_BYTES + 2 > size) { //not all eeg data in buffer
        return size;
      }

      if ((buffer[offset] == EEGMessages::HEADER_EEG))
      {	
		//unsigned char sample = buffer[offset + 1];
	    //std::cout << "Sample no: " << (int)(0xFF & sample) << std::endl;
	    
	    //if(internal_sample_counter == 0) internal_sample_counter = sample;
	    //if(abs(sample - internal_sample_counter) > 5) std::cout << "sample difference!" << std::endl;
	    
	    offset +=2;
	    internal_sample_counter++;
	    
	    for(size_t i = 0; i < channels_no; i++)
	    {
			unsigned char byte0 = buffer[offset + 0];
	        unsigned char byte1 = buffer[offset + 1];
	        unsigned char byte2 = buffer[offset + 2];
	        
	        int32_t value = (byte0 << 16) + (byte1 << 8) + byte2;
	        if ((value & 0x00800000) > 0) {  
              value |= 0xFF000000;  
            } else {  
              value &= 0x00FFFFFF;  
            }  
	        //std::cout << "value channel  " << i << ": " << translateValue(value) << " mV" << std::endl;
			offset += 3;
			
			std::lock_guard<std::mutex> lck(m_bufferMutex);
			m_buffers[i]->push_front(translateValue(value));
		}
		
		if(!data_ready && internal_sample_counter > buffer_len) //AFTER filling last values
	    {
		   std::cout << "Buffer filled!" << std::endl;
		   data_ready = true;	
		   internal_sample_counter = 0;
		}

	  }
      else {
        // No EEG; consume one byte.
        offset++;
      }
    }
  }
  // discard everything and start over.
  return size;
}

double EEGDecoder::translateValue(int32_t raw) {
    
    double translated = static_cast<int32_t>(100 * ((raw * 4.5 / GAIN) / 8388607));
	return translated;
}

double EEGDecoder::getLastEEG(int channel) noexcept
{
	std::lock_guard<std::mutex> lck(m_bufferMutex);
	return m_buffers[channel]->front();
}

bool EEGDecoder::initScan(const uint8_t *buffer, const size_t offset) noexcept {
  
  if ( (buffer[offset + 0] == EEGBytes::INIT) &&
       (buffer[offset + 1] == EEGBytes::INIT) &&
       (buffer[offset + 2] == EEGBytes::INIT) )
  {
	  std::lock_guard<std::mutex> lck(m_dataMutex);
	  m_initialized = true;
	  return true;
  }
  return false;
}

bool EEGDecoder::dataReady() const noexcept {
	return data_ready;
}

std::vector<std::vector<double>>* EEGDecoder::readData() noexcept
{
	std::lock_guard<std::mutex> lck(m_bufferMutex);
	for(size_t i = 0; i < channels_no; i++)
	{
	 // std::copy(m_buffers[i]->begin(), m_buffers[i]->end(), outputVectors[i]);
	}
	return outputVectors;
}

void EEGDecoder::readData(double** arr) noexcept
{
	std::lock_guard<std::mutex> lck(m_bufferMutex);
	for(size_t i = 0; i < channels_no; i++)
	{
	  std::copy(m_buffers[i]->begin(), m_buffers[i]->end(), arr[i]);
	}
}

void EEGDecoder::reset() noexcept
{
	data_ready = false;
	internal_sample_counter = 0;
}
