#include "yolo11n_detect.h"
#include "esp_log.h"

static const char* TAG = "YOLO11nDetect";

// IMPLEMENTAZIONE del costruttore (non dichiarazione della classe)
YOLO11nDetect::YOLO11nDetect(dl::Model* model) {
    m_model = model;
    
    // Inizializza ImagePreprocessor
    m_image_preprocessor = new dl::image::ImagePreprocessor( //parametri perfetti per normalizzazione di YOLO11n
        m_model, 
        {0, 0, 0},      // mean
        {255, 255, 255} // std
    );
    
    // Inizializza yolo11PostProcessor 
    m_postprocessor = new dl::detect::yolo11PostProcessor(
        m_model, 
        0.15,   // score_threshold //ORIGINARIAMENTE E' 0.25!! //si può abbassare per risoluzioni bassissime come 64x64 DEFAULT=0.25
        0.7,    // nms_threshold DEFAULT=0.7
        10,     // resize_scale_x DEFAULT=10
        {{8, 8, 4, 4}, {16, 16, 8, 8}, {32, 32, 16, 16}} // stages/livelli della feature pyramid DEFAULT={{8, 8, 4, 4}, {16, 16, 8, 8}, {32, 32, 16, 16}}
        //{{8, 8, 4, 4}, {16, 16, 8, 8}} // stages per risoluzioni bassissime come 64x64 (evita detection ridicole e microscopiche)
    );
}

// IMPLEMENTAZIONE del distruttore
YOLO11nDetect::~YOLO11nDetect() {
    delete m_image_preprocessor;
    delete m_postprocessor;
}

// IMPLEMENTAZIONE del metodo run
std::list<dl::detect::result_t> &YOLO11nDetect::run(const dl::image::img_t &img) {
    // 1. PREPROCESSING AUTOMATICO
    m_image_preprocessor->preprocess(img);
    
    // 2. INFERENCE
    m_model->run();
    
    // 3. POSTPROCESSING AUTOMATICO
    m_postprocessor->clear_result();
    m_postprocessor->set_resize_scale_x(m_image_preprocessor->get_resize_scale_x());
    m_postprocessor->set_resize_scale_y(m_image_preprocessor->get_resize_scale_y());
    m_postprocessor->postprocess();
    
    // 4. RISULTATI
    return m_postprocessor->get_result(img.width, img.height);
}