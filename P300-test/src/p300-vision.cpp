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

#include "cluon-complete.hpp"
#include "opendlv-standard-message-set.hpp"

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/features2d/features2d.hpp>

#include <cstdint>
#include <iostream>
#include <mutex>
#include <time.h> 
#include <vector>
#include <chrono>
#include <numeric>
#include <algorithm>

#define WIDTH 1920
#define HEIGHT 1000
#define RADIUS HEIGHT/14

cv::Mat highlight(cv::Mat chart, int n)
{
  cv::Scalar light_beige(220,245,245);

  if(n > 5)
  {
    cv::circle(chart, cv::Point((1)*WIDTH/4, HEIGHT/4), RADIUS, light_beige, cv::FILLED);
    cv::circle(chart, cv::Point((2)*WIDTH/4, HEIGHT/4), RADIUS, light_beige, cv::FILLED);
    cv::circle(chart, cv::Point((3)*WIDTH/4, HEIGHT/4), RADIUS, light_beige, cv::FILLED);

    cv::circle(chart, cv::Point((1)*WIDTH/4, 3*HEIGHT/4), RADIUS, light_beige, cv::FILLED);
    cv::circle(chart, cv::Point((2)*WIDTH/4, 3*HEIGHT/4), RADIUS, light_beige, cv::FILLED);
    cv::circle(chart, cv::Point((3)*WIDTH/4, 3*HEIGHT/4), RADIUS, light_beige, cv::FILLED);
  }
  else if(n < 3) cv::circle(chart, cv::Point((n+1)*WIDTH/4, HEIGHT/4), RADIUS, light_beige, cv::FILLED);
  else cv::circle(chart, cv::Point((n-2)*WIDTH/4, 3*HEIGHT/4), RADIUS, light_beige, cv::FILLED);

  return chart;
}

cv::Mat select(cv::Mat chart, int n, int order)
{
  cv::Scalar red(86,86,255);
  cv::Scalar dark_beige(102,165,211);
  
  cv::Scalar red_blend = 1.0/order * red + (1.0 - 1.0/order) * dark_beige;

  if(n < 3) cv::circle(chart, cv::Point((n+1)*WIDTH/4, HEIGHT/4), RADIUS, red_blend, cv::FILLED);
  else cv::circle(chart, cv::Point((n-2)*WIDTH/4, 3*HEIGHT/4), RADIUS, red_blend, cv::FILLED);

  return chart;
}

cv::Mat mark(cv::Mat chart, int n)
{
  cv::Scalar blue(255,190,0);

  if(n > 5)
  {
    cv::circle(chart, cv::Point((1)*WIDTH/4, HEIGHT/4), RADIUS, blue, cv::FILLED);
    cv::circle(chart, cv::Point((2)*WIDTH/4, HEIGHT/4), RADIUS, blue, cv::FILLED);
    cv::circle(chart, cv::Point((3)*WIDTH/4, HEIGHT/4), RADIUS, blue, cv::FILLED);

    cv::circle(chart, cv::Point((1)*WIDTH/4, 3*HEIGHT/4), RADIUS, blue, cv::FILLED);
    cv::circle(chart, cv::Point((2)*WIDTH/4, 3*HEIGHT/4), RADIUS, blue, cv::FILLED);
    cv::circle(chart, cv::Point((3)*WIDTH/4, 3*HEIGHT/4), RADIUS, blue, cv::FILLED);
  }
  else if(n < 3) cv::circle(chart, cv::Point((n+1)*WIDTH/4, HEIGHT/4), RADIUS, blue, cv::FILLED);
  else cv::circle(chart, cv::Point((n-2)*WIDTH/4, 3*HEIGHT/4), RADIUS, blue, cv::FILLED);

  return chart;
}

cv::Mat createChart()
{
  cv::Mat chart = cv::Mat(HEIGHT, WIDTH, CV_8UC3, cv::Scalar(0, 0 ,0)); //black background
  cv::Scalar dark_beige(102,165,211);

  cv::circle(chart, cv::Point(1*WIDTH/4, HEIGHT/4), RADIUS, dark_beige, cv::FILLED);
  cv::circle(chart, cv::Point(2*WIDTH/4, HEIGHT/4), RADIUS, dark_beige, cv::FILLED);
  cv::circle(chart, cv::Point(3*WIDTH/4, HEIGHT/4), RADIUS, dark_beige, cv::FILLED);

  cv::circle(chart, cv::Point(1*WIDTH/4, 3*HEIGHT/4), RADIUS, dark_beige, cv::FILLED);
  cv::circle(chart, cv::Point(2*WIDTH/4, 3*HEIGHT/4), RADIUS, dark_beige, cv::FILLED);
  cv::circle(chart, cv::Point(3*WIDTH/4, 3*HEIGHT/4), RADIUS, dark_beige, cv::FILLED);

  return chart;
}

int32_t main(int32_t argc, char **argv) {
    int32_t retCode{1};
    auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
    if ( 0 == commandlineArguments.count("cid") || 
         0 == commandlineArguments.count("delay")) {
        std::cerr << argv[0] << " ..." << std::endl;
        std::cerr << "Usage:   " << argv[0] << " --cid=<OD4 session> [--verbose] --delay=<time after which P300 is analysed>" << std::endl;
        std::cerr << "         --cid:    CID of the OD4Session to send and receive messages" << std::endl;
        std::cerr << "Example: " << argv[0] << " --cid=112 --verbose --delay=330" << std::endl;
    }
    else {
        const bool VERBOSE{commandlineArguments.count("verbose") != 0};
        const int DELAY{stoi(commandlineArguments["delay"])};
        float currentPotential{0};
        std::vector<int> highlighted(6, 0);
        std::vector<float> responses(6, 0);
        srand (time(NULL));
        cv::Mat background = createChart();

        cluon::OD4Session od4{static_cast<uint16_t>(std::stoi(commandlineArguments["cid"]))};
        cv::namedWindow("Speller", cv::WINDOW_AUTOSIZE);

        std::mutex eegMutex;
        auto onEEG = [&eegMutex, &currentPotential, &VERBOSE](cluon::data::Envelope &&env){
            opendlv::proxy::VoltageReading current = cluon::extractMessage<opendlv::proxy::VoltageReading>(std::move(env));
            std::lock_guard<std::mutex> lck(eegMutex);
            currentPotential = current.voltage();
        };
                
        od4.dataTrigger(opendlv::proxy::VoltageReading::ID(), onEEG);

        cv::imshow("Speller", background);
	std::cout << "Press any key..." << std::endl;
        cv::waitKey(0);

        //starting sequence...
        for (auto i = 0; i < 6; i++)
        {
          cv::Mat flash = createChart();
          highlight(flash, i);
          cv::imshow("Speller", flash);
          cv::waitKey(1);
          std::this_thread::sleep_for(std::chrono::milliseconds(30)); 
          mark(flash, i);
          cv::imshow("Speller", flash);
          cv::waitKey(1);
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          cv::imshow("Speller", background);
          cv::waitKey(1);
        }
	//flash all...
        cv::Mat flashed = createChart();
        highlight(flashed, 6);
        cv::imshow("Speller", flashed);
        cv::waitKey(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(30)); 
        mark(flashed, 6);
        cv::imshow("Speller", flashed);
        cv::waitKey(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        cv::imshow("Speller", background);
        cv::waitKey(1);

        while (od4.isRunning() && std::accumulate(highlighted.begin(),highlighted.end(),0) < (3*6 + 3))
        {
          while(std::accumulate(highlighted.begin(),highlighted.end(),0) < 3*6)
          {
            cv::Mat flash = createChart();
	    cv::imshow("Speller", background);
	    cv::waitKey(1);
	    std::this_thread::sleep_for(std::chrono::milliseconds(300));
	  
	    int random = rand() % 6;
	    while(highlighted[random] > 2) random = rand() % 6; //up to three highlights per circle;
	  
	    highlighted[random]++;

	    highlight(flash, random);
	    cv::imshow("Speller", flash);
	    cv::waitKey(1);
	    std::this_thread::sleep_for(std::chrono::milliseconds(30)); 
	    mark(flash, random);
	    cv::imshow("Speller", flash);
	    cv::waitKey(1);
	    std::this_thread::sleep_for(std::chrono::milliseconds(100));
	    cv::imshow("Speller", background);
	    cv::waitKey(1);
	    std::this_thread::sleep_for(std::chrono::milliseconds(DELAY - 130));

	    //retrieve p300 ratio
	    std::lock_guard<std::mutex> lck(eegMutex);
	    responses[random] += currentPotential;
          }
          cv::Mat flash = createChart();
          std::this_thread::sleep_for(std::chrono::milliseconds(300));
           
	  int random = rand() % 6;
          highlighted[random]++;
          highlight(flash, random);
          cv::imshow("Speller", flash);
          cv::waitKey(1);
          std::this_thread::sleep_for(std::chrono::milliseconds(30)); 
          mark(flash, random);
          cv::imshow("Speller", flash);
          cv::waitKey(1);
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          cv::imshow("Speller", background);
          cv::waitKey(1);
          std::this_thread::sleep_for(std::chrono::milliseconds(DELAY - 130)); 
        }

	if (VERBOSE) {
          std::cout << "values: " << std::endl;
	  for (auto i = 0; i < 6; i++)
	  std::cout << responses[i] << ", " << std::endl;
	}

        if(std::accumulate(responses.begin(),responses.end(),0) == 0) std::cout << "No responses!" << std::endl;
        std::vector<float>::iterator max = std::max_element(responses.begin(), responses.end());
        int elem = std::distance(responses.begin(), max);
        select(background, elem, 1);
        std::cout << "Element chosen: " << elem << ", value: " << *max << std::endl;
        responses.at(elem) = -1;
        max = std::max_element(responses.begin(), responses.end());
        elem = std::distance(responses.begin(), max);
        select(background, elem, 2);
        std::cout << "Followed by: " << elem << ", value: " << *max << std::endl;
        responses.at(elem) = -1;
        max = std::max_element(responses.begin(), responses.end());
        elem = std::distance(responses.begin(), max);
        select(background, elem, 3);
        cv::imshow("Speller", background);
	cv::waitKey(0);
        retCode = 0;
    }
  return retCode;
}
