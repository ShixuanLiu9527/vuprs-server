#include <fstream>
#include "logger/check.h"
#include "3rdparty/nlohmann/json.hpp"
#include "fault_detect/fault_detector.h"
#include "system_tools/string_parse.h"

void vuprs::FaultDetector::LoadModel(const std::string &json, const vuprs::FaultDetectionConfig &config)
{
    std::ifstream f;
    f.open(json);
    RUNTIME_CHECK(f.is_open(), "bf", "Cannot open file: " + json);
    nlohmann::json json_data;
    try
    {
        f >> json_data;
    }
    catch (const std::exception &e)
    {
        RUNTIME_CHECK(false, "bf", " Failed to load array data from: " + json);
    }
    /* Read parameters from Json (frequency range is bound to certain model) */
    RUNTIME_CHECK(json_data.contains("frequency-range"), "fault_detect", "Missing key [frequency-range] in model config.");
    RUNTIME_CHECK(json_data["frequency-range"].contains("from"), "fault_detect", "Missing key [from] in model config.");
    RUNTIME_CHECK(json_data["frequency-range"].contains("to"), "fault_detect", "Missing key [to] in model config.");
    RUNTIME_CHECK(json_data.contains("rknn-model"), "fault_detect", "Missing key [rknn-model] in model config.");
    RUNTIME_CHECK(json_data["rknn-model"].contains("path"), "fault_detect", "Missing key [path] in model config.");
    std::string s_f_l = json_data["frequency-range"]["from"];
    std::string s_f_h = json_data["frequency-range"]["to"];
    std::string model_path = json_data["rknn-model"]["path"];
    double f_l = vuprs::ParseDoubleFromString(s_f_l, nullptr);
    double f_h = vuprs::ParseDoubleFromString(s_f_h, nullptr);
    /* Load RKNN model */
    this->model.InitModel(model_path);
    RUNTIME_CHECK(this->model.ModelReady(), "inference", "Model is empty.");
    /* Get and check input number and tensor size */
    std::vector<rknn_tensor_attr> inputs = this->model.GetInputAttrs();
    RUNTIME_CHECK(inputs.size() == 1, "inference", "Input number must be 1.");
    /* NCWH layout (0-batch, 1-channel, 2-width, 3-height) */
    RUNTIME_CHECK(inputs[0].n_dims == 4, "inference", "Not a 4D-input model.");
    uint32_t mfcc_dim = inputs[0].dims[3];   /* MFCC image height (MFCC dims) */
    uint32_t mfcc_frame = inputs[0].dims[2]; /* MFCC image width (frames) */
    /* Initialize extractor */
    this->extractor.SetParameters(mfcc_dim,
                                  mfcc_frame,
                                  config.mfcc__frame_time_ms,
                                  f_l,
                                  f_h);
}

void vuprs::FaultDetector::InputSignal(const std::vector<double> &signal, double fs)
{
    Eigen::Matrix<double, -1, 1> _signal;
    vuprs::stdVector2eigenVector(signal, &_signal);
    this->extractor.InputSignal(_signal, fs);
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
                         RKNN_TENSOR_NCHW);
    this->model.run();
}

void vuprs::FaultDetector::GetResult(std::vector<double> *res)
{
    /* Retrieve output attributes */
    std::vector<rknn_tensor_attr> output_attrs = this->model.GetOutputAttrs();
    RUNTIME_CHECK(output_attrs.size() >= 1, "default_detect", "Model has no output.");
    rknn_tensor_attr &out_attr = output_attrs[0];
    /* Calculate number of output elements from tensor dimensions */
    uint32_t n_elements = 1;
    for (uint32_t i = 0; i < out_attr.n_dims; i++)
    {
        n_elements *= out_attr.dims[i];
    }
    /* Allocate buffer and get output from NPU */
    std::vector<uint8_t> output_buffer(out_attr.size);
    this->model.GetOutput(0, output_buffer.data(), out_attr.size);
    /* Convert raw output to double logits based on tensor type */
    Eigen::Matrix<double, -1, 1> logits(n_elements);
    if (out_attr.type == RKNN_TENSOR_FLOAT32)
    {
        float *float_out = reinterpret_cast<float *>(output_buffer.data());
        for (uint32_t i = 0; i < n_elements; i++)
        {
            logits(i) = static_cast<double>(float_out[i]);
        }
    }
    else if (out_attr.type == RKNN_TENSOR_UINT8)
    {
        for (uint32_t i = 0; i < n_elements; i++)
        {
            logits(i) = static_cast<double>(output_buffer[i]);
        }
    }
    else if (out_attr.type == RKNN_TENSOR_INT8)
    {
        int8_t *int8_out = reinterpret_cast<int8_t *>(output_buffer.data());
        for (uint32_t i = 0; i < n_elements; i++)
        {
            logits(i) = static_cast<double>(int8_out[i]);
        }
    }
    else
    {
        RUNTIME_CHECK(false, "default_detect",
                      "Unsupported output tensor type: " + std::to_string(out_attr.type));
    }
    /* Apply softmax to obtain class probabilities */
    Eigen::Matrix<double, -1, 1> probs = vuprs::softmax(logits);
    /* Write result */
    res->resize(n_elements);
    for (uint32_t i = 0; i < n_elements; i++)
    {
        (*res)[i] = probs(i);
    }
}
