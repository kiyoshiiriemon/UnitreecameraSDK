#include <zmq.hpp>
#include <string>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <filesystem>

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
    // Prepare our context and socket
    zmq::context_t context(1);
    zmq::socket_t socket(context, ZMQ_REQ);

    std::cout << "Connecting to server…" << std::endl;
    socket.connect("tcp://192.168.123.13:25661");

    cv::namedWindow("Received Images", cv::WINDOW_AUTOSIZE);

    // Generate time stamped folder name and create the directory
    std::string folderName = getTimeStampedFolderName();
    std::filesystem::create_directories(folderName);

    int counter = 0;

    while (true) {
        // Sending request
        zmq::message_t request(6);
        memcpy(request.data(), "Hello", 6);
        std::cout << "Sending request…" << std::endl;
        socket.send(request);

        // Get the reply (image data).
        zmq::message_t reply;
        socket.recv(&reply);

        // Assuming the server responds with a jpeg image, load it into OpenCV
        std::vector<char> data(static_cast<char*>(reply.data()), static_cast<char*>(reply.data()) + reply.size());
        cv::Mat img = cv::imdecode(cv::Mat(data), cv::IMREAD_COLOR);

        if (img.empty()) {
            std::cerr << "Image is empty or corrupted." << std::endl;
            break;
        }

        // Show the image
        cv::imshow("Received Images", img);

        // Save the image
        std::stringstream ss;
        ss << folderName << "/image_" << std::setfill('0') << std::setw(5) << counter << ".jpg";
        if (!cv::imwrite(ss.str(), img)) {
            std::cerr << "Could not write image to file: " << ss.str() << std::endl;
            break;
        }

        std::cout << "Image received and saved to " << ss.str() << std::endl;
        counter++;

        if (cv::waitKey(30) >= 0) break;
    }

    return 0;
}
