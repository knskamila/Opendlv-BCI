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

#include "p300-detector.hpp"

#include <iostream>
#include <stdlib.h>

P300Detector::P300Detector(double** arr, size_t n_channels, size_t n_bins) noexcept
{
  eegArray = arr;
  bins = n_bins;
  channels = n_channels;
  
  first_300_ms_length = 0.3*FREQUENCY;
  post_300_ms_length = bins - first_300_ms_length;
  
  pre_output_size = first_300_ms_length/2 + 1;
  post_output_size = post_300_ms_length/2 + 1;
  
  pre_20hz_cutoff = first_300_ms_length * 20/FREQUENCY;
  post_20hz_cutoff = post_300_ms_length * 20/FREQUENCY;
  
  arrayPre = new double[first_300_ms_length];
  arrayPost = new double[post_300_ms_length];
  
  pre_output_buffer = new fftw_complex[pre_output_size];
  post_output_buffer = new fftw_complex[post_output_size];
  
  plan_pre = fftw_plan_dft_r2c_1d(first_300_ms_length, 
                                               arrayPre, 
                                               pre_output_buffer, 
                                               FFTW_ESTIMATE);
                                               
                                              
  plan_post = fftw_plan_dft_r2c_1d(post_300_ms_length, 
                                               arrayPost, 
                                               post_output_buffer,
                                               FFTW_ESTIMATE);
                                               
  if(!plan_pre || !plan_post) std::cout << "Caution: FFT plan could not be created." << std::endl;
}

double P300Detector::detect() noexcept {
  
  double total_pre{0}, total_post{0};
  
  for (size_t i = 0; i < channels; i++)
  {
	double channel_sum{0}, channel_mean{0};
	double sum_pre_channel{0}, sum_post_channel{0};
	//std::lock_guard<std::mutex> lck(m_dataMutex[i]);
	
	// Normalizing the input: first 300 ms
	for(size_t b = 0; b < first_300_ms_length; b++)
		channel_sum += eegArray[i][b];
	
	channel_mean = channel_sum/first_300_ms_length;
    for(size_t b = 0; b < first_300_ms_length; b++)
		eegArray[i][b] -= channel_mean;
		
    channel_mean = channel_sum = 0;
    
    // Normalizing the input: second part
	for(size_t b = first_300_ms_length; b < bins; b++)
		channel_sum += eegArray[i][b];
	
	channel_mean = channel_sum/post_300_ms_length;
    for(size_t b = first_300_ms_length; b < bins; b++)
		eegArray[i][b] -= channel_mean;
		
    std::copy(eegArray[i] + 0, eegArray[i] + first_300_ms_length, arrayPre);
    std::copy(eegArray[i] + first_300_ms_length, eegArray[i] + bins-1, arrayPost);
                                               
	fftw_execute(plan_pre);
	fftw_execute(plan_post);

	for (int b = 1; b <= pre_20hz_cutoff; b++){
		double real = pre_output_buffer[b][0];
		double imag = pre_output_buffer[b][1];
		double abs2 = (real*real + imag*imag)/(pre_output_size);
		sum_pre_channel += abs2;
	}
	
	for (int b = 0; b <= post_20hz_cutoff; b++){
		double real = post_output_buffer[b][0];
		double imag = post_output_buffer[b][1];
		double abs2 = (real*real + imag*imag)/(post_output_size);
		sum_post_channel += abs2;
	}
	
	total_pre += sum_pre_channel;
	total_post += sum_post_channel;
  }
  
  if(total_pre < 1.0) return total_post/channels;
  else return (total_post/channels)/total_pre;
}
