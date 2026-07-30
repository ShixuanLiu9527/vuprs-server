#ifndef FAULT_DETECTOR_H
#define FAULT_DETECTOR_H

#include <string>
#include <vector>
#include "algorithm/inference/feature_extraction.h"
#include "algorithm/inference/launch_npu.h"
#include "algorithm/inference/post_processing.h"

namespace vuprs
{
    struct FaultDetectionConfig
    {
        double mfcc__frame_time_ms; /* Time period (ms) per MFCC frame */
    };

    class FaultDetector
    {
    private:
        SignalExtractor extractor; /* feature extractor */
        RknnModel model;           /* RKnn model & NPU manager */

    public:
        FaultDetector() = default;
        ~FaultDetector() = default;

        FaultDetector(const FaultDetector &other) = delete;
        FaultDetector &operator=(const FaultDetector &other) = delete;
        FaultDetector(FaultDetector &&other) = delete;
        FaultDetector &operator=(FaultDetector &&other) = delete;

        void LoadModel(const std::string &json, const vuprs::FaultDetectionConfig &config);

        void InputSignal(const std::vector<double> &signal, double fs);

        bool CheckReady() const { return this->extractor.Flushed() && this->model.ModelReady(); }

        void RunInference();

        void GetResult(std::vector<double> *res);
    };
}

#endif
