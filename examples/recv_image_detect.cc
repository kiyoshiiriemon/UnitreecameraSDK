#include <zmq.hpp>
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

#define USE_LEFT_CAMERA 0

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

cv::Mat receiveImage(zmq::socket_t& socket, const std::string& serverName) {
    while (true) {
        // Send request
        zmq::message_t request(6);
        memcpy(request.data(), "Hello", 6);
        std::cout << "Sending request to " << serverName << "…" << std::endl;
        socket.send(request, zmq::send_flags::none);

        // Set the receive timeout
        int timeout = 1000; // 1 second
        socket.set(zmq::sockopt::rcvtimeo, timeout);

        // Get the reply
        zmq::message_t reply;
        if (!socket.recv(reply, zmq::recv_flags::none)) {
            std::cerr << "Failed to receive data from " << serverName << " within the timeout period. Retrying..." << std::endl;
            continue;
        }

        // Decode the image
        std::vector<char> data(static_cast<char*>(reply.data()), static_cast<char*>(reply.data()) + reply.size());
        cv::Mat img = cv::imdecode(cv::Mat(data), cv::IMREAD_COLOR);

        if (img.empty()) {
            std::cerr << "Received empty or corrupted image from " << serverName << ". Retrying..." << std::endl;
            continue;
        }

        return img;
    }
}

std::vector<DetectedObject> detect_and_send(ObjectDetector &detector, boost::asio::ip::udp::socket &socket, cv::Mat &image, const std::string &camera_name)
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
    return objects;
}

int main(int argc, char *argv[]) {
    boost::asio::io_service io_service;
    boost::asio::ip::udp::socket socket(io_service);
    socket.open(boost::asio::ip::udp::v4());

    ObjectDetector detector(argv[1], argv[2], argv[3]);
    // Prepare our context and sockets
    zmq::context_t context(1);
    zmq::socket_t socket1(context, ZMQ_REQ);
    zmq::socket_t socket2(context, ZMQ_REQ);


    std::cout << "Connecting to servers…" << std::endl;
    socket1.connect("tcp://192.168.123.13:25661");
#if USE_LEFT_CAMERA == 1
    //socket2.connect("tcp://192.168.123.14:25661");
#endif

    // Generate time stamped folder name and create the directory
    std::string folderName1 = getTimeStampedFolderName() + "_front";
    std::string folderName2 = getTimeStampedFolderName() + "_left";
    std::filesystem::create_directories(folderName1);
    std::filesystem::create_directories(folderName2);

    int counter = 0;

    while (true) {
        cv::Mat capimg = receiveImage(socket1, "server 1 (front)");
        cv::Mat img1 = cv::Mat(capimg, cv::Rect(100, 70, 730, 730)); //for go1-aka
        auto objects = detect_and_send(detector, socket, img1, "camera_front");

#if USE_LEFT_CAMERA == 1
        cv::Mat img2 = receiveImage(socket2, "server 2 (left)");
        detectObjects(detector, img2);
#endif

        // Save the image from server 1
        std::stringstream ss1;
        ss1 << folderName1 << "/image_" << std::setfill('0') << std::setw(5) << counter << ".jpg";
        if (!cv::imwrite(ss1.str(), capimg)) {
            std::cerr << "Could not write image to file: " << ss1.str() << std::endl;
            break;
        }
        std::cout << "Image received from server 1 (front) and saved to " << ss1.str() << std::endl;

        cv::Mat img = img1;
#if USE_LEFT_CAMERA == 1
        // Save the image from server 2
        std::stringstream ss2;
        ss2 << folderName2 << "/image_" << std::setfill('0') << std::setw(5) << counter << ".jpg";
        if (!cv::imwrite(ss2.str(), img2)) {
            std::cerr << "Could not write image to file: " << ss2.str() << std::endl;
            break;
        }
        std::cout << "Image received from server 2 (left) and saved to " << ss2.str() << std::endl;

        // Combine the two images horizontally
        cv::hconcat(img1, img2, img);
#endif

        for (const auto &det : objects) {
            int left  = (det.bbox.x - det.bbox.w / 2) * img.cols;
            int right = (det.bbox.x + det.bbox.w / 2) * img.cols;
            int top   = (det.bbox.y - det.bbox.h / 2) * img.rows;
            int bot   = (det.bbox.y + det.bbox.h / 2) * img.rows;

            cv::rectangle(img, cv::Point(left, top), cv::Point(right, bot), cv::Scalar(0, 255, 0), 3);
            std::string label = det.name + " " + std::to_string((int)(det.confidence * 100)) + "%";
            cv::putText(img, label, cv::Point(left, top - 5), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 2);
        }


        // Show the combined image
        cv::resize(img, img, cv::Size(), 0.5, 0.5);
        cv::imshow("Received Images (front | left)", img);

        counter++;

        if (cv::waitKey(50) >= 0) break;
        //if (cv::waitKey(30) >= 0) break;

    }

    return 0;
}

