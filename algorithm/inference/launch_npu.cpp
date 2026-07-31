#include "logger/log_manager.h"
#include "system_tools/file_processing.h"
#include "algorithm/inference/launch_npu.h"

vuprs::RknnModel::RknnModel(const std::string &model_path) : ctx_(0), io_num_{0, 0}
{
    this->InitModel(model_path);
}

vuprs::RknnModel::~RknnModel()
{
    if (this->ctx_ != 0)
    {
        rknn_destroy(this->ctx_);
        this->ctx_ = 0;
    }
}

vuprs::RknnModel::RknnModel(RknnModel &&other) noexcept
    : ctx_(other.ctx_),
      io_num_(other.io_num_),
      input_attrs_(std::move(other.input_attrs_)),
      output_attrs_(std::move(other.output_attrs_))
{
    other.ctx_ = 0;
}

vuprs::RknnModel &vuprs::RknnModel::operator=(RknnModel &&other) noexcept
{
    if (this != &other)
    {
        if (this->ctx_ != 0)
            rknn_destroy(this->ctx_);
        this->ctx_ = other.ctx_;
        io_num_ = other.io_num_;
        input_attrs_ = std::move(other.input_attrs_);
        output_attrs_ = std::move(other.output_attrs_);
        other.ctx_ = 0;
    }
    return *this;
}

void vuprs::RknnModel::InitModel(const std::string &model_path)
{
    std::ifstream file(model_path, std::ios::binary | std::ios::ate);
    /* Check model file opened */
    RUNTIME_CHECK(file.is_open(), "inference", "Failed to open model file: " + model_path);
    /* Read model file */
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> model_data(size);
    RUNTIME_CHECK(file.read(model_data.data(), size), "inference", "Failed to read model file: " + model_path);
    /* Initialize context */
    int ret = rknn_init(&this->ctx_,
                        model_data.data(),
                        size,
                        0,
                        nullptr);
    RUNTIME_CHECK(ret == RKNN_SUCC, "inference", "rknn_init failed");
    /* Query model information */
    this->QueryIOInfo();
    this->InitTensorAttrs();
}

void vuprs::RknnModel::QueryIOInfo()
{
    int ret = rknn_query(this->ctx_,
                         RKNN_QUERY_IN_OUT_NUM,
                         &this->io_num_,
                         sizeof(this->io_num_));
    RUNTIME_CHECK(ret == RKNN_SUCC, "inference", "rknn_query (IO_NUM) failed");
}

void vuprs::RknnModel::InitTensorAttrs()
{
    this->input_attrs_.resize(this->io_num_.n_input);
    for (uint32_t i = 0; i < this->io_num_.n_input; i++)
    {
        this->input_attrs_[i].index = i;
        int ret = rknn_query(this->ctx_,
                             RKNN_QUERY_INPUT_ATTR,
                             &this->input_attrs_[i],
                             sizeof(rknn_tensor_attr));
        RUNTIME_CHECK(ret == RKNN_SUCC,
                      "inference",
                      "rknn_query (INPUT_ATTR) failed for index " + std::to_string(i));
    }
    this->output_attrs_.resize(this->io_num_.n_output);
    for (uint32_t i = 0; i < this->io_num_.n_output; i++)
    {
        this->output_attrs_[i].index = i;
        int ret = rknn_query(this->ctx_,
                             RKNN_QUERY_OUTPUT_ATTR,
                             &this->output_attrs_[i],
                             sizeof(rknn_tensor_attr));
        RUNTIME_CHECK(ret == RKNN_SUCC,
                      "inference",
                      "rknn_query (OUTPUT_ATTR) failed for index " + std::to_string(i));
    }
}

void vuprs::RknnModel::SetInput(uint32_t index,
                                void *buffer,
                                uint32_t size,
                                _rknn_tensor_type type = _rknn_tensor_type::RKNN_TENSOR_UINT8,
                                _rknn_tensor_format layout = _rknn_tensor_format::RKNN_TENSOR_NCHW)
{
    RUNTIME_CHECK(index < this->io_num_.n_input, "inference", "Invalid input index.");
    rknn_input input;
    memset(&input, 0, sizeof(rknn_input));
    input.index = index;
    input.buf = buffer;
    input.size = size;
    input.pass_through = 0; /* adjust for certain model */
    input.type = type;      /* adjust for certain model */
    input.fmt = layout;     /* adjust for certain model */
    int ret = rknn_inputs_set(this->ctx_, 1, &input);
    RUNTIME_CHECK(ret == RKNN_SUCC, "inference", "rknn_inputs_set failed");
}

void vuprs::RknnModel::run()
{
    int ret = rknn_run(this->ctx_, nullptr);
    RUNTIME_CHECK(ret == RKNN_SUCC, "inference", "rknn_run failed");
}

void vuprs::RknnModel::GetOutput(uint32_t index, void *buffer, uint32_t size)
{
    RUNTIME_CHECK(index < this->io_num_.n_output, "inference", "Invalid output index.");
    rknn_output output;
    memset(&output, 0, sizeof(rknn_output));
    output.index = index;
    output.buf = buffer;
    output.size = size;
    output.want_float = 0;
    output.is_prealloc = 1;
    int ret = rknn_outputs_get(this->ctx_,
                               1,
                               &output,
                               nullptr);
    RUNTIME_CHECK(ret == RKNN_SUCC, "inference", "rknn_outputs_get failed for index " + std::to_string(index));
}
