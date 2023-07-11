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

int main() {
    // Prepare our context and sockets
    zmq::context_t context(1);
    zmq::socket_t socket1(context, ZMQ_REQ);
    zmq::socket_t socket2(context, ZMQ_REQ);

    std::cout << "Connecting to servers…" << std::endl;
    socket1.connect("tcp://192.168.123.13:25661");
    socket2.connect("tcp://192.168.123.14:25661");

    // Generate time stamped folder name and create the directory
    std::string folderName1 = getTimeStampedFolderName() + "_front";
    std::string folderName2 = getTimeStampedFolderName() + "_left";
    std::filesystem::create_directories(folderName1);
    std::filesystem::create_directories(folderName2);

    int counter = 0;

    while (true) {
        // Send request to server 1
        zmq::message_t request1(6);
        memcpy(request1.data(), "Hello", 6);
        std::cout << "Sending request to server 1 (front)…" << std::endl;
        socket1.send(request1);

        // Get the reply from server 1 (image data).
        zmq::message_t reply1;
        socket1.recv(&reply1);

        // Assuming the server responds with a jpeg image, load it into OpenCV
        std::vector<char> data1(static_cast<char*>(reply1.data()), static_cast<char*>(reply1.data()) + reply1.size());
        cv::Mat img1 = cv::imdecode(cv::Mat(data1), cv::IMREAD_COLOR);

        if (img1.empty()) {
            std::cerr << "Image from server 1 (front) is empty or corrupted." << std::endl;
            break;
        }

        // Save the image from server 1
        std::stringstream ss1;
        ss1 << folderName1 << "/image_" << std::setfill('0') << std::setw(5) << counter << ".jpg";
        if (!cv::imwrite(ss1.str(), img1)) {
            std::cerr << "Could not write image to file: " << ss1.str() << std::endl;
            break;
        }

        std::cout << "Image received from server 1 (front) and saved to " << ss1.str() << std::endl;


        // Send request to server 2
        zmq::message_t request2(6);
        memcpy(request2.data(), "Hello", 6);
        std::cout << "Sending request to server 2 (left)…" << std::endl;
        socket2.send(request2);

        // Get the reply from server 2 (image data).
        zmq::message_t reply2;
        socket2.recv(&reply2);

        // Assuming the server responds with a jpeg image, load it into OpenCV
        std::vector<char> data2(static_cast<char*>(reply2.data()), static_cast<char*>(reply2.data()) + reply2.size());
        cv::Mat img2 = cv::imdecode(cv::Mat(data2), cv::IMREAD_COLOR);

        if (img2.empty()) {
            std::cerr << "Image from server 2 (left) is empty or corrupted." << std::endl;
            break;
        }

        // Save the image from server 2
        std::stringstream ss2;
        ss2 << folderName2 << "/image_" << std::setfill('0') << std::setw(5) << counter << ".jpg";
        if (!cv::imwrite(ss2.str(), img2)) {
            std::cerr << "Could not write image to file: " << ss2.str() << std::endl;
            break;
        }

        std::cout << "Image received from server 2 (left) and saved to " << ss2.str() << std::endl;

        // Combine the two images horizontally
        cv::Mat img;
        cv::hconcat(img1, img2, img);

        // Show the combined image
        cv::imshow("Received Images (front | left)", img);

        counter++;

        if (cv::waitKey(30) >= 0) break;

        // Wait for 1 second
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}

