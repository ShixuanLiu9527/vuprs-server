#ifndef FAULT_DETECTOR_H
#define FAULT_DETECTOR_H

#include <string>
#include <vector>
#include <memory>
#include "algorithm/inference/feature_extraction.h"
#include "algorithm/inference/launch_npu.h"
#include "algorithm/inference/post_processing.h"
#include "logger/log_manager.h"

namespace vuprs
{
    class FaultDetector
    {
    private:
        SignalExtractor extractor; /* feature extractor */
        RknnModel model;           /* RKnn model & NPU manager */
        std::shared_ptr<spdlog::logger> inference_logger;

    public:
        FaultDetector() = default;
        ~FaultDetector() = default;

        FaultDetector(const FaultDetector &other) = delete;
        FaultDetector &operator=(const FaultDetector &other) = delete;
        FaultDetector(FaultDetector &&other) = delete;
        FaultDetector &operator=(FaultDetector &&other) = delete;

        bool InitDetector(const std::string &model_config_json, const std::string &logger_dir);

        void InputSignal(const std::vector<double> &signal, double fs);

        bool CheckReady() const { return this->extractor.Flushed() && this->model.ModelReady(); }

        void RunInference();

        void GetResult(std::vector<double> *res);
    };
}

#endif
