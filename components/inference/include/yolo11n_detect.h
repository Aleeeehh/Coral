#pragma once
#include "dl_detect_base.hpp"
#include "dl_detect_yolo11_postprocessor.hpp"
#include "dl_image_preprocessor.hpp"
#include <list>

class YOLO11nDetect {
    private:
        dl::Model* m_model;
        dl::image::ImagePreprocessor* m_image_preprocessor;
        dl::detect::yolo11PostProcessor* m_postprocessor;
        
    public:
        YOLO11nDetect(dl::Model* model);
        ~YOLO11nDetect();
        std::list<dl::detect::result_t> &run(const dl::image::img_t &img);
    };