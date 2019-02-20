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

#include "cluon-complete.hpp"
#include "opendlv-standard-message-set.hpp"
#include "kiwi-image-processing.hpp"

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/features2d/features2d.hpp>

#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>

#define MAX_ANGLE 0.5f
#define DEF_REVERSE_SPEED -0.2f
#define DEF_FORWARD_SPEED 0.11f
#define MARKER_SIZE 100
#define HORIZON(a) static_cast<uint16_t>(a/2-10)

#define T_BELOW(a) (int)(a * 0.7)
#define T_ABOVE(a) (int)(a * 0.12)
#define T_LEFT(a) (int)(a * 0.10)
#define T_RIGHT(a) a - T_LEFT(a)

#define FIND_BIGGEST 1
#define FIND_SELECTED 0

#define MODE_TARGET 1
#define MODE_SIMPLE 0

#define STATIONARY 0
#define DIRECTIONAL 1
#define FOLLOW_OBJECT 2
#define MOVE_BACKWARDS 3

std::mutex mouseMutex;

struct Args
{
    uint16_t x;
    uint16_t y;
    const uint16_t WIDTH;
    const uint16_t HEIGHT;
    uint16_t action;
    cv::Mat frame;
    cv::Mat hsv_mat;
    cv::Scalar hsv_scalar;
    bool fixation;
    bool active;
    bool MODE;
    const bool SELECTION_MODE;
    const bool VERBOSE;
    const bool VISUALIZE;
    float reverse_speed;
    float forward_speed;
    float angle;
    float speed;

    Args(bool T_MODE, float FORWARD_SPEED, float REVERSE_SPEED, bool P_VERBOSE, bool P_VISUALIZE, uint16_t width, uint16_t height) :
            WIDTH(width), HEIGHT(height), MODE(T_MODE), x(0), y(0), fixation(0), active(0), action(0), SELECTION_MODE(FIND_SELECTED),
            angle(0), speed(0), reverse_speed(REVERSE_SPEED), forward_speed(FORWARD_SPEED), VERBOSE(P_VERBOSE), VISUALIZE(P_VISUALIZE) {}
};

struct MouseArgs
{
    uint16_t x;
    uint16_t y;
    bool fixation;
    bool active;

    MouseArgs() :
        x(0), y(0), fixation(false), active(false) {}
};

static void onMouse(int event, int x, int y, int flags, void* param)
{
    std::lock_guard<std::mutex> lock(mouseMutex);
    MouseArgs* parameters = reinterpret_cast<MouseArgs*>(param);
    parameters->x = x;
    parameters->y = y;

    if(event == cv::EVENT_LBUTTONDOWN) parameters->fixation = true;
    if(event == cv::EVENT_LBUTTONUP) parameters->fixation = false;

    parameters->active = true;
}

//float convertDistance(int position)
//{
//    return 1 - (position - HORIZON)/(float)(HEIGHT-HORIZON);
//}

void defaultValues(Args* p)
{
    uint16_t WIDTH = p->WIDTH;
    uint16_t HEIGHT = p->HEIGHT;
    uint16_t maximum_x = p->WIDTH/2 - T_LEFT(WIDTH);
    uint16_t maximum_y = p->HEIGHT/2 - T_BELOW(HEIGHT);

    float multiplier_x = (p->x - p->WIDTH/2)/(float)maximum_x;
    float multiplier_y = (p->y - p->HEIGHT/2)/(float)maximum_y;
    p->angle = MAX_ANGLE*multiplier_x;
    p->speed = p->forward_speed*multiplier_y;
    p->action = DIRECTIONAL;
    if (p->VERBOSE) std::cout << "Angle: " << p->angle << std::endl;
}

void findObject(Args* p)
{
    uint16_t WIDTH = p->WIDTH;
    uint16_t HEIGHT = p->HEIGHT;
    uint16_t x = p->x;
    uint16_t y = p->y;

    cv::Rect t;
    cv::Mat mask = cv::Mat::zeros(p->frame.rows, p->frame.cols, CV_8UC1);
    cv::Mat mask_temp = mask.clone();
    cv::Scalar offset = cv::Scalar(15,25,25);
    if(p->hsv_scalar[1] <= 30) offset = cv::Scalar(255, 30, 255); //grey
    if(p->hsv_scalar[2] <= 50 || p->hsv_scalar[2] >= 230) offset = cv::Scalar(255, 255, 40); //black, whitet
    inRange(p->hsv_mat, p->hsv_scalar - offset, p->hsv_scalar + offset, mask);
    cv::Rect selection(0, 0, p->WIDTH, T_BELOW(HEIGHT));
    mask = mask(selection);
    mask = cleanMask(mask);

    //if the selection is not within a now cleaned mask – discard:
    if (mask.at<uchar>(x, y) == 0)
    {
        defaultValues(p);
        if (p->VERBOSE) std::cout << "No object" << std::endl;
        return;
    }
    
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    double maxSize = 0;
    int iLabel = 0;
    float distance = 0;

    findContours(mask, contours, hierarchy, CV_RETR_CCOMP, CV_CHAIN_APPROX_SIMPLE);

    if (contours.size() == 0) std::cout << "Error, no contours" << std::endl;
    else if (p->SELECTION_MODE == FIND_BIGGEST)
    {
        for (int i = 0; i < contours.size(); i++)
        {
            double area = cv::contourArea(contours[i], false);
            if (area > WIDTH)
            {
                cv::drawContours(mask_temp, contours, i, cv::Scalar(255), CV_FILLED, 8, hierarchy);
            }
            if (area > maxSize)
            {
                maxSize = area;
                iLabel = i;
            }
        }
        t = cv::boundingRect(contours[iLabel]);
    }
    else
    {
        int closest = WIDTH*HEIGHT;
        for (int i = 0; i < contours.size(); i++)
        {
            double area = cv::contourArea(contours[i], false);
            if (area > WIDTH*2)
            {
                //check how close to the selection
                cv::Rect r = cv::boundingRect(contours[i]);
                int r_cx = r.x + r.width/2;
                int r_cy = r.y + r.height/2;

                int selection_distance = (p->x - r_cx)*(p->x - r_cx) + (p->y - r_cy)*(p->y - r_cy);
                //cout << "distance: " << convertDistance(selection_distance) << endl;
                if(selection_distance < closest)
                {
                    closest = selection_distance;
                    iLabel = i;
                    t = r;
                }
            }
        }
        drawContours(mask_temp, contours, iLabel, cv::Scalar(255), CV_FILLED, 8, hierarchy);
    }
    
    if(t.width > WIDTH*0.8) //the area covers most of the field view in width: not an object
    {
        if(p->VERBOSE) std::cout << "No objects" << std::endl;
        defaultValues(p);
        return;
    }

    distance = -1;

    //finds the point where the object touches the floor
    for (int i = T_BELOW(HEIGHT); i > HORIZON(HEIGHT); --i)
    {
        int val = mask_temp.at<uchar>(i, x);
        if(val != 0)
        {
            if (p->VISUALIZE) cv::rectangle(p->frame, cv::Point(x-1, i-1), cv::Point(x+1, i+1), cv::Scalar(30,30,255), -1);
            //distance = convertDistance(i);
            break;
        }
    }

    if (distance < 0 && p->VERBOSE) std::cout << "Obscured object." << std::endl;
    if (p->VISUALIZE) cv::rectangle(p->frame, t.tl(), t.br(), cv::Scalar(255,255,255), 1);
    p->angle = x/(float)WIDTH - 0.5f;
    p->speed = distance;
    p->action = FOLLOW_OBJECT;
    if(p->VERBOSE) std::cout << "The target was found at angle " << (x/(float)WIDTH - 0.5f)
    << " at distance " << distance << std::endl;
}

bool isGround(cv::Scalar hsv) //test values
{
    cv::Scalar hsv_min = cv::Scalar(10, 35, 10);
    cv::Scalar hsv_max = cv::Scalar(20, 150, 230);

    return isWithin(hsv, hsv_min, hsv_max);
}

void findAction(Args* p)
{
    if(!(p->fixation && p->active)) return;

    uint16_t x = p->x;
    uint16_t y = p->y;
    uint16_t WIDTH = p->WIDTH;
    uint16_t HEIGHT = p->HEIGHT;
    bool MODE = p->MODE;

    if (y < T_ABOVE(HEIGHT))
    {
        p->action = MOVE_BACKWARDS;
        p->angle = 0;
        p->speed = p->reverse_speed;
        if(p->VERBOSE) std::cout << "Back" << std::endl;
        if(p->VISUALIZE) cv::rectangle(p->frame, cv::Point(0, 0),
        cv::Point(WIDTH, T_ABOVE(HEIGHT)), cv::Scalar(255,255,255), -1);
        return;
    }
    else if (y > T_BELOW(HEIGHT))
    {
        p->action = MOVE_BACKWARDS;
        p->angle = 0;
        p->speed = p->reverse_speed;
        if(p->VERBOSE) std::cout << "Back" << std::endl;
        if(p->VISUALIZE) cv::rectangle(p->frame, cv::Point(0, T_BELOW(HEIGHT)),
        cv::Point(WIDTH, HEIGHT), cv::Scalar(255,255,255), -1);
        return;
    }
    else if (x < T_LEFT(WIDTH))
    {
        p->action = DIRECTIONAL;
        p->angle = -MAX_ANGLE;
        p->speed = p->forward_speed;
        if(p->VERBOSE) std::cout << "Left" << std::endl;
        if(p->VISUALIZE) cv::rectangle(p->frame, cv::Point(0, 0),
        cv::Point(T_LEFT(WIDTH), HEIGHT), cv::Scalar(255,255,255), -1);
        return;
    }
    else if (x > T_RIGHT(WIDTH))
    {
        p->action = DIRECTIONAL;
        p->angle = MAX_ANGLE;
        p->speed = p->forward_speed;
        if(p->VERBOSE) std::cout << "Right" << std::endl;
        if(p->VISUALIZE) cv::rectangle(p->frame, cv::Point(T_RIGHT(WIDTH), 0),
        cv::Point(WIDTH, HEIGHT), cv::Scalar(255,255,255), -1);
        return;
    }
    else if (MODE == MODE_TARGET) { //find object

        cv::Rect selection(x-4, y-4, 8, 8); //pixel is guaranteed to be away from the border
        cv::Mat cutout = p->frame(selection);

        cvtColor(cutout, cutout, CV_BGRA2BGR);
        cvtColor(cutout, cutout, CV_BGR2HSV);
        cvtColor(p->frame, p->hsv_mat, CV_BGRA2BGR);
	cvtColor(p->hsv_mat, p->hsv_mat, CV_BGR2HSV);
        cv::Scalar meanvalue = mean(cutout); //mean of the gaze area

        uint16_t h = (int)meanvalue[0];
        uint16_t s = (int)meanvalue[1];
        uint16_t v = (int)meanvalue[2];

        p->hsv_scalar = cv::Scalar(h,s,v);

        if(isGround(p->hsv_scalar))
        {
            if (p->VERBOSE) std::cout << "Selected the ground." << std::endl; //color to be always ignored discarded
            return;
        }
        else findObject(p);
    }
    else if (MODE == MODE_SIMPLE) //provide angle if the directional mode was selected
    {
        defaultValues(p);
    }
}

int32_t main(int32_t argc, char **argv) {
    int32_t retCode{1};
    auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
    if ( (0 == commandlineArguments.count("cid")) ||
         (0 == commandlineArguments.count("name")) ||
         (0 == commandlineArguments.count("mode")) ) {
        std::cerr << argv[0] << " attaches to a shared memory area containing an ARGB image." << std::endl;
        std::cerr << "Usage:   " << argv[0] << " --cid=<OD4 session> --name=<name of shared memory area> --mode=<mode of steering> [--verbose] [--mouse] [--markers] [--visualize]" << std::endl;
        std::cerr << "         --cid:    CID of the OD4Session to send and receive messages" << std::endl;
        std::cerr << "         --name:   name of the shared memory area to attach" << std::endl;
        std::cerr << "         --width:  width of the frame" << std::endl;
        std::cerr << "         --height: height of the frame" << std::endl;
        std::cerr << "	       --mode:   direction {0} or target {1}" << std::endl;
        std::cerr << "	       --mouse:  (optional) uses mouse instead of the eye tracker" << std::endl;
        std::cerr << "	       --markers:  (optional) adds Pupil Labs markers to the image" << std::endl;
        std::cerr << "	       --visualize:  (optional) visualizes areas and objects detected" << std::endl;
        std::cerr << "Example: " << argv[0] << " --cid=112 --name=img.argb --mode=0 --verbose --mouse --markers" << std::endl;
    }
    else {
        const std::string NAME{commandlineArguments["name"]};
        const uint16_t WIDTH{(commandlineArguments["width"].size() != 0) ? static_cast<uint16_t>(std::stoi(commandlineArguments["width"])) : 1344};
        const uint16_t HEIGHT{(commandlineArguments["height"].size() != 0) ? static_cast<uint16_t>(std::stoi(commandlineArguments["height"])) : 1008};
        const bool VERBOSE{commandlineArguments.count("verbose") != 0};
        const bool MODE{static_cast<bool>(std::stoi(commandlineArguments["mode"]))};
        const bool MOUSE{commandlineArguments.count("mouse") != 0};
        const bool MARKERS{commandlineArguments.count("markers") != 0};
        const bool VISUALIZE{commandlineArguments.count("visualize") != 0};
        const float FORWARD_SPEED{(commandlineArguments["fspeed"].size() != 0) ? static_cast<float>(std::stof(commandlineArguments["fspeed"])) : DEF_FORWARD_SPEED};
        const float REVERSE_SPEED{(commandlineArguments["rspeed"].size() != 0) ? static_cast<float>(std::stof(commandlineArguments["rspeed"])) : DEF_REVERSE_SPEED};
        uint16_t x;
        uint16_t y;
        bool active = false;
        bool fixation = false;
        MouseArgs mouseParam;
        Args param(MODE, FORWARD_SPEED, REVERSE_SPEED, VERBOSE, VISUALIZE, WIDTH, HEIGHT);
        cv::Mat background;

        std::unique_ptr<cluon::SharedMemory> sharedMemory{new cluon::SharedMemory{NAME}};
        if (sharedMemory && sharedMemory->valid()) {
            std::clog << argv[0] << ": Attached to shared memory '" << sharedMemory->name() << " (" << sharedMemory->size() << " bytes)." << std::endl;

            IplImage *iplimage{nullptr};
            CvSize size;
            size.width = 640;
            size.height = 480;

            iplimage = cvCreateImageHeader(size, IPL_DEPTH_8U, 4 /* ARGB */);
            sharedMemory->lock();
            {
                iplimage->imageData = sharedMemory->data();
                iplimage->imageDataOrigin = iplimage->imageData;
            }
            sharedMemory->unlock();

            cluon::OD4Session od4{static_cast<uint16_t>(std::stoi(commandlineArguments["cid"]))};
            cv::namedWindow("Stream", cv::WINDOW_AUTOSIZE);

            if(MARKERS) background = create_border_image(WIDTH, HEIGHT);

            if(MOUSE) // alternative mode using a mouse pointer
            {
                cv::setMouseCallback("Stream", onMouse, (void*)&mouseParam);
            }
            else // listening for Pupil data
            {
                std::mutex gazeMutex;
                auto onGaze = [&gazeMutex, &x, &y, &active, &WIDTH, &HEIGHT, &MARKERS](cluon::data::Envelope &&env){
                auto senderStamp = env.senderStamp();
                if (senderStamp == 1)
                {
                    opendlv::logic::sensation::Point gaze = cluon::extractMessage<opendlv::logic::sensation::Point>(std::move(env));

                    std::lock_guard<std::mutex> lck(gazeMutex);
                    x = static_cast<uint16_t>((WIDTH+(MARKERS*(2*MARKER_SIZE)))*gaze.azimuthAngle());
                    y = static_cast<uint16_t>(HEIGHT*gaze.zenithAngle());
                    active = static_cast<bool>(gaze.distance());
                }
                };

                std::mutex fixationMutex;
                auto onFixation = [&fixationMutex, &fixation](cluon::data::Envelope &&env){
                opendlv::proxy::SwitchStateReading fixationReading = cluon::extractMessage<opendlv::proxy::SwitchStateReading>(std::move(env));

                std::lock_guard<std::mutex> lck(fixationMutex);
                fixation = fixationReading.state() == 1;
                };

                od4.dataTrigger(opendlv::logic::sensation::Point::ID(), onGaze);
                od4.dataTrigger(opendlv::proxy::SwitchStateReading::ID(), onFixation);
            }

            while (od4.isRunning())
            {
                if(MOUSE)
                {
                    std::lock_guard<std::mutex> lock(mouseMutex);
                    x = mouseParam.x;
                    y = mouseParam.y;
                    active = mouseParam.active;
                    fixation = mouseParam.fixation;
                }

                if(MARKERS) // adding pointer position offset
                {
                    x-=MARKER_SIZE;
                    if(x>= WIDTH) active = false;
                }

                cv::Mat img, view;
                sharedMemory->wait();
                sharedMemory->lock();
                {
                    img = cv::cvarrToMat(iplimage).clone();
                }
                sharedMemory->unlock();
                cv::resize(img, view, cv::Size(WIDTH,HEIGHT));

                normalize_t(view);
                
                param.x = x;
                param.y = y;
                param.active = active;
                param.fixation = fixation;
                param.frame = view;

                findAction(&param);

                if (active && VISUALIZE)
                {
                    cv::circle(view, cv::Point(x, y), 10, cv::Scalar(100, 255, 255), 2);
                    cv::line(view, cv::Point(x,0), cv::Point(x, HEIGHT), cv::Scalar(255,255,100));
                }
                else if (!active && VERBOSE) std::cout << "Gaze outside of the screen" << std::endl;

                cluon::data::TimeStamp sampleTime;
                opendlv::logic::sensation::Point selectedAction;
                selectedAction.zenithAngle(param.angle);
                selectedAction.azimuthAngle(param.speed);
                selectedAction.distance(param.action);
                od4.send(selectedAction, sampleTime, 1); //sender stamp: several ::Point messages expected

                if (MARKERS)
                {
                    view.copyTo(background(cv::Rect(MARKER_SIZE,0,WIDTH,HEIGHT)));
                    cv::imshow("Stream", background);
                }
                else cv::imshow("Stream", view);

                cv::waitKey(1);
                fixation = false;
            }

            if (nullptr != iplimage) {
                cvReleaseImageHeader(&iplimage);
            }
        }
        retCode = 0;
    }
    return retCode;
}

