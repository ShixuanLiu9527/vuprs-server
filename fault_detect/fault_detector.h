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

        /**
         * @brief Check whether a signal block contains enough samples to form
         *        at least one inference frame at the given sampling frequency.
         *
         * @param samples Number of samples in one signal block (e.g. DMA buffer).
         * @param fs Sampling frequency in Hz.
         *
         * @retval true: enough samples.
         * @retval false: too short, inference can never start.
         */
        bool ValidateInputSignalLength(uint32_t samples, double fs) const;

        bool CheckReady() const { return this->extractor.Flushed() && this->model.ModelReady(); }

        void RunInference();

        void GetResult(Eigen::Matrix<double, -1, 1> *res, int *identity, bool use_softmax);
    };
}

#endif
