#include <iostream>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

extern "C" {
#include "darknet.h"
#include "network.h"
#include "classifier.h"
#include "option_list.h"
}

struct DetectedObject {
    std::string name;
    float confidence;
    box bbox;
};

class ObjectDetector {
public:
    ObjectDetector(const char *cfgfile, const char *weightfile, const char *datacfg)
    {
        // Darknetのネットワークを初期化
        net_ = load_network_custom((char*)cfgfile, (char*)weightfile, 0, 1);
        set_batch_network(net_, 1);

        // クラス名を読み込み
        list *options = read_data_cfg((char*)datacfg);
        char names[6]="names";
        char *name_list = option_find_str(options, names, 0);
        names_ = get_labels(name_list);
    }

    std::vector<DetectedObject> detect(const cv::Mat &cv_img)
    {
        // OpenCVの画像をdarknetのimageに変換
#if 1
        cv::Mat imconv;
        cv::cvtColor(cv_img, imconv, cv::COLOR_BGR2RGB);
        image im = mat_to_image(imconv);
#elif 0
        image im = mat_to_image2(cv_img);
#else
        image im = mat_to_image(cv_img);
#endif
        image sized = letterbox_image(im, net_->w, net_->h);

        // 物体検出を行い、結果を取得
        float *X = sized.data;
        network_predict_ptr(net_, X);
        int nboxes = 0;
        detection *dets = get_network_boxes(net_, im.w, im.h, 0.5, 0.5, 0, 1, &nboxes, 0);

        // 非最大抑制を行い、結果をフィルタリング
        do_nms_sort(dets, nboxes, net_->layers[net_->n - 1].classes, 0.45);

        // 検出結果を構造体にして返す
        std::vector<DetectedObject> detected_objects;
        for (int i = 0; i < nboxes; ++i) {
            DetectedObject obj;
            obj.confidence = -1;
            for (int j = 0; j < net_->layers[net_->n - 1].classes; ++j) {
                if (dets[i].prob[j] > obj.confidence) {
                    obj.name = names_[j];
                    obj.confidence = dets[i].prob[j];
                    obj.bbox = dets[i].bbox;
                }
            }
            if (obj.confidence > 0.5) {
                detected_objects.push_back(obj);
            }
        }

        // リソースの解放
        free_detections(dets, nboxes);
        free_image(im);
        free_image(sized);

        return detected_objects;
    }

private:
    network *net_;
    char **names_;

    image mat_to_image(const cv::Mat& mat) {
        int w = mat.cols;
        int h = mat.rows;
        int c = mat.channels();
        image im = make_image(w, h, c);
        unsigned char *data = (unsigned char *)mat.data;
        int step = mat.step;
        for(int y = 0; y < h; ++y) {
            for(int x = 0; x < w; ++x) {
                for(int k= 0; k < c; ++k) {
                    im.data[k*w*h + y*w + x] = data[y*step + x*c + k]/255.0;
                }
            }
        }
        return im;
    }
    image mat_to_image2(const cv::Mat& mat) {
        int w = mat.cols;
        int h = mat.rows;
        int c = mat.channels();
        image im = make_image(w, h, c);
        unsigned char *data = (unsigned char *)mat.data;
        int step = mat.step;
        for(int y = 0; y < h; ++y) {
            for(int x = 0; x < w; ++x) {
                for(int k = 0; k < c; ++k) {
                    int src_index = y * step + x * c + k;
                    int dst_index = k * w * h + y * w + x;
                    // BGRからRGBに変換
                    if (c == 3 && k == 0) {
                        dst_index += 2 * w * h;
                    } else if (c == 3 && k == 2) {
                        dst_index -= 2 * w * h;
                    }
                    im.data[dst_index] = data[src_index] / 255.0;
                }
            }
        }
        return im;
    }

};

