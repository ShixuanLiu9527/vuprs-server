#include <fstream>
#include "config.h"
#include "logger/check.h"
#include "logger/log_manager.h"
#include "3rdparty/nlohmann/json.hpp"
#include "algo/detector/fault_detector.h"
#include "algo/inference/post_processing.h"
#include "misc/string_parse.h"

bool vuprs::FaultDetector::InitDetector(const std::string &model_config_json,
                                        const std::string &logger_dir)
{
    std::ifstream f;
    f.open(model_config_json);
    RUNTIME_CHECK(f.is_open(), "bf", "Cannot open file: " + model_config_json);
    nlohmann::json json_data;
    try
    {
        f >> json_data;
    }
    catch (const std::exception &e)
    {
        RUNTIME_CHECK(false, "bf", " Failed to load array data from: " + model_config_json);
    }
    /* Read parameters from Json (frequency range is bound to certain model) */
    RUNTIME_CHECK(json_data.contains("frequency-range"), "detector", "Missing key [frequency-range] in model config.");
    RUNTIME_CHECK(json_data["frequency-range"].contains("from"), "detector", "Missing key [from] in model config.");
    RUNTIME_CHECK(json_data["frequency-range"].contains("to"), "detector", "Missing key [to] in model config.");
    RUNTIME_CHECK(json_data.contains("rknn-model"), "detector", "Missing key [rknn-model] in model config.");
    RUNTIME_CHECK(json_data["rknn-model"].contains("path"), "detector", "Missing key [path] in model config.");
    RUNTIME_CHECK(json_data.contains("frame-time-ms"), "detector", "Missing key [rknn-model] in model config.");
    RUNTIME_CHECK(json_data["frame-time-ms"].contains("time-ms"), "detector", "Missing key [path] in model config.");
    std::string s_f_l = json_data["frequency-range"]["from"];
    std::string s_f_h = json_data["frequency-range"]["to"];
    std::string s_frame_time_ms = json_data["frame-time-ms"]["time-ms"];
    std::string model_path = json_data["rknn-model"]["path"];
    double f_l = vuprs::ParseDoubleFromString(s_f_l, nullptr);
    double f_h = vuprs::ParseDoubleFromString(s_f_h, nullptr);
    double frame_time_ms = vuprs::ParseDoubleFromString(s_frame_time_ms, nullptr);
    /* Load RKNN model */
    this->model.InitModel(model_path);
    RUNTIME_CHECK(this->model.ModelReady(), "inference", "Model is empty.");
    /* Get and check input number and tensor size */
    std::vector<rknn_tensor_attr> inputs = this->model.GetInputAttrs();
    RUNTIME_CHECK(inputs.size() == 1, "inference", "Input number must be 1.");
    /* NHWC layout (batch, height, width, channel) */
    RUNTIME_CHECK(inputs[0].n_dims == 4, "inference", "Not a 4D-input model.");
    RUNTIME_CHECK(inputs[0].dims[0] == 1, "inference", "Input batch must be 1.");
    RUNTIME_CHECK(inputs[0].dims[3] == 1, "inference", "Input channel must be 1.");
#if VUPRS_DEBUG
    printf("Input tensor layout: [%d, %d, %d, %d]\n", inputs[0].dims[0],
           inputs[0].dims[1],
           inputs[0].dims[2],
           inputs[0].dims[3]);
    std::vector<rknn_tensor_attr> outputs = this->model.GetOutputAttrs();
    printf("Output tensor layout: [");
    for (uint32_t i = 0; i < outputs[0].n_dims; i++)
    {
        printf("%d%s", outputs[0].dims[i], (i + 1 < outputs[0].n_dims) ? ", " : "");
    }
    printf("]\n");
#endif
    uint32_t mfcc_dim = inputs[0].dims[1];   /* H: MFCC dims per frame */
    uint32_t mfcc_frame = inputs[0].dims[2]; /* W: frames */
    /* Initialize extractor */
    this->extractor.SetParameters(mfcc_dim,
                                  mfcc_frame,
                                  frame_time_ms,
                                  f_l,
                                  f_h);
    this->inference_logger = vuprs::LogManager::getLogger("inference",
                                                          "log.txt",
                                                          logger_dir);
    return true;
}

void vuprs::FaultDetector::InputSignal(const std::vector<double> &signal, double fs)
{
    Eigen::Matrix<double, -1, 1> _signal;
    vuprs::stdVector2eigenVector(signal, &_signal);
    this->extractor.InputSignal(_signal, fs);
}

bool vuprs::FaultDetector::ValidateInputSignalLength(uint32_t samples, double fs) const
{
    return samples >= this->extractor.FrameLengthSamples(fs);
}

double vuprs::FaultDetector::FrameTimeMs() const
{
    return this->extractor.FrameTimeMs();
}

uint32_t vuprs::FaultDetector::FrameLengthSamples(double fs) const
{
    return this->extractor.FrameLengthSamples(fs);
}

uint32_t vuprs::FaultDetector::OutputTensorLength() const
{
    const std::vector<rknn_tensor_attr> &outputs = this->model.GetOutputAttrs();
    if (outputs.empty() || outputs[0].n_dims == 0)
        return 0;
    uint64_t elements = 1;
    for (uint32_t i = 0; i < outputs[0].n_dims; i++)
        elements *= static_cast<uint64_t>(outputs[0].dims[i]);
    if (elements == 0 || elements > static_cast<uint64_t>(UINT32_MAX))
        return 0;
    return static_cast<uint32_t>(elements);
}

void vuprs::FaultDetector::RunInference()
{
    RUNTIME_CHECK(this->CheckReady(), "default_detect", "Model not ready.");
    Eigen::Matrix<uint8_t, -1, -1> tensor;
    this->extractor.GetExtractTensor(&tensor); /* tensor size: H x W (col first) */
    tensor.transposeInPlace();                 /* convert to row first */
    this->model.SetInput(0,
                         tensor.data(),
                         tensor.size() * sizeof(uint8_t),
                         RKNN_TENSOR_UINT8,
                         RKNN_TENSOR_NHWC);
    this->model.run();
}

void vuprs::FaultDetector::GetResult(Eigen::Matrix<double, -1, 1> *res, int *identity, bool use_softmax)
{
    uint32_t n_elements = this->OutputTensorLength();
    RUNTIME_CHECK(n_elements > 0, "default_detect", "Model has no valid output tensor.");
    /* Allocate buffer and get output from NPU.
     * Note: [RknnModel::GetOutput] requests float32 output (want_float = 1),
     * so the NPU runtime dequantizes the tensor (FP32/INT8/INT16... all
     * supported), and we always read floats here. */
    std::vector<float> output_buffer(n_elements);
    this->model.GetOutput(0, output_buffer.data(), n_elements * sizeof(float));
    /* Convert output to double logits */
    Eigen::Matrix<double, -1, 1> logits(n_elements);
    for (uint32_t i = 0; i < n_elements; i++)
    {
        logits(i) = static_cast<double>(output_buffer[i]);
    }
    /* Apply softmax to obtain class probabilities */
    if (use_softmax)
        *res = vuprs::softmax(logits);
    else
        *res = logits;
    /* Class identity (argmax is invariant under softmax) */
    if (identity != nullptr)
    {
        res->maxCoeff(identity);
    }
#if VUPRS_DEBUG
    std::cout << "NPU Inference time comsuming: " << this->model.GetInferenceRuntime() << " us" << std::endl;
#endif
}
