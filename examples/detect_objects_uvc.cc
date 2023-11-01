#include <string>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <thread>
#include <chrono>
#include "object_detector.hh"
#include <boost/asio.hpp>

std::string getTimeStampedFolderName() {
    // Get current time
    std::time_t t = std::time(0);
    std::tm* now = std::localtime(&t);

    // Format the string
    std::stringstream ss;
    ss << std::setfill('0') 
       << (now->tm_year + 1900) 
       << "_" << std::setw(2) << (now->tm_mon + 1) 
       << "_" << std::setw(2) << now->tm_mday
       << "_" << std::setw(2) << now->tm_hour
       << std::setw(2) << now->tm_min;

    return ss.str();
}

void detect_and_send(ObjectDetector &detector, boost::asio::ip::udp::socket &socket, cv::Mat &image, const std::string &camera_name)
{
    boost::asio::ip::udp::endpoint endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 12346);
    std::vector<DetectedObject> objects = detector.detect(image);
    for (const DetectedObject& obj : objects) {
        std::cout << "  Name: " << obj.name
                  << ", Confidence: " << obj.confidence
                  << ", Bbox: (" << obj.bbox.x << ", " << obj.bbox.y << ", "
                                 << obj.bbox.w << ", " << obj.bbox.h << ")" << std::endl;
        std::ostringstream oss;
        oss << camera_name << "," << obj.name << "," << obj.confidence << ","
            << obj.bbox.x << "," << obj.bbox.y << "," << obj.bbox.w << "," << obj.bbox.h;
        std::string message = oss.str();
        socket.send_to(boost::asio::buffer(message), endpoint);
    }
    if (objects.empty()) {
        std::string message = camera_name + ",none,0,0,0,0,0";
        socket.send_to(boost::asio::buffer(message), endpoint);
    }
}

int main(int argc, char *argv[]) {
    boost::asio::io_service io_service;
    boost::asio::ip::udp::socket socket(io_service);
    socket.open(boost::asio::ip::udp::v4());

    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        std::cerr << "failed to open camera" << std::endl;
        return -1;
    }

    ObjectDetector detector(argv[1], argv[2], argv[3]);
    // Generate time stamped folder name and create the directory
    std::string folderName1 = getTimeStampedFolderName() + "_webcam";
    std::filesystem::create_directories(folderName1);

    int counter = 0;

    while (true) {
        cv::Mat frame;
        cap >> frame;
        if(frame.empty()) {
            break;
        }
        cv::resize(frame, frame, cv::Size(), 0.5, 0.5);
        detect_and_send(detector, socket, frame, "camera_front");
#if 0
        std::stringstream ss1;
        ss1 << folderName1 << "/image_" << std::setfill('0') << std::setw(5) << counter << ".jpg";
        if (!cv::imwrite(ss1.str(), frame)) {
            std::cerr << "Could not write image to file: " << ss1.str() << std::endl;
            break;
        }
        std::cout << "Image camera 1 (front) saved to " << ss1.str() << std::endl;
#endif
        cv::imshow("Image (front)", frame);
        counter++;

        int key = cv::waitKey(30);
        if (key == 'q') {
            break;
        }

        //std::this_thread::sleep_for(std::chrono::seconds(200));
    }

    return 0;
}

