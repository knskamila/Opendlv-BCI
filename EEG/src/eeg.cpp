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

#include "eeg.hpp"

//template< typename T, class Popper>
//inline std::ostream & operator<<( std::ostream & os, ::nonstd::ring_span<T, Popper> const & rs )
//{
//    os << "[ring_span: "; std::copy( rs.begin(), rs.end(), std::ostream_iterator<T>(os, ", ") ); return os << "]";
//}

EEG::EEG(const std::string &device, const size_t channels, const size_t bins) noexcept {
  constexpr const uint32_t BAUDRATE{115200};
  constexpr const uint32_t TIMEOUT{500};
  
  value_array = new double*[channels];
  buffers = new nonstd::ring_span_lite::ring_span<double>*[channels];
  
  for (size_t i = 0; i < channels; i++)
  {
	value_array[i] = new double[bins];
	buffers[i] = new nonstd::ring_span_lite::ring_span<double>(value_array[i], value_array[i] + bins, value_array[i], bins);
  }

  m_decoder = new EEGDecoder(buffers, channels, bins);
  
  try {
    m_eegDevice.reset(new serial::Serial(device, BAUDRATE, serial::Timeout::simpleTimeout(TIMEOUT)));
    if (isOpen()) {
      m_eegDevice->setDTR(false);
      m_readingBytesFromDeviceThread.reset(new std::thread([&eegDevice = m_eegDevice, &decoder = *m_decoder](){
		const uint16_t BUFFER_SIZE{2048};
		uint8_t *data = new uint8_t[BUFFER_SIZE];
		size_t size{0};
		while (eegDevice->isOpen()) {
		  std::this_thread::sleep_for(std::chrono::milliseconds(10));
		  if (eegDevice->waitReadable()) {
			size_t bytesAvailable{eegDevice->available()};
			size_t bytesRead = eegDevice->read(data+size, ((BUFFER_SIZE - size) < bytesAvailable) ? (BUFFER_SIZE - size) : bytesAvailable);
			size += bytesRead;
			size_t consumed = decoder.decode(data, size);
			for (size_t i{0}; (0 < consumed) && (i < (size - consumed)); i++) {
			  data[i] = data[i + consumed];
			}

			size -= consumed;
			// If the parser does not work at all, cancel it.
			if (size >= BUFFER_SIZE) {
			  break;
			}
		  }
		}
		delete [] data;
		data = nullptr;
	  }
      ));
    }
  }
  catch(...) {}
}

EEG::~EEG() {
  if (isOpen()) {
    {
      std::cout << "Stopping EEG..." << std::endl;
      const std::vector<uint8_t> COMMAND_STOP{EEGDecoder::STOP};
	  m_eegDevice->write(COMMAND_STOP);
    }

    m_eegDevice->close();
    m_readingBytesFromDeviceThread->join();
  }
  m_eegDevice.reset(nullptr);
}

bool EEG::isOpen() const noexcept {
  return (m_eegDevice) && m_eegDevice->isOpen();
}

bool EEG::isInitialized() const noexcept {
  return m_decoder->getStatus();
}

bool EEG::dataReady() const noexcept {
  return m_decoder->dataReady();
}

void EEG::start() noexcept {
  //std::cout << "status: " << m_decoder->getStatus() << std::endl;
  if(m_decoder->getStatus())
  {
	const std::vector<uint8_t> COMMAND_START{'b'};
    m_eegDevice->write(COMMAND_START);    
  }
}

void EEG::stop() const noexcept {
  const std::vector<uint8_t> COMMAND_STOP{'s'};
  m_eegDevice->write(COMMAND_STOP); 
  m_decoder->reset();
}

std::vector<std::vector<double>>* EEG::readData()
{
	if(!m_decoder->dataReady()) throw "Data not ready.";
	return m_decoder->readData();
}

void EEG::readData(double** arr)
{
	if(!m_decoder->dataReady()) throw "Data not ready.";
	else m_decoder->readData(arr);
}

