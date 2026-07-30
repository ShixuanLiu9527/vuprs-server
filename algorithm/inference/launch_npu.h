#ifndef LAUNCH_NPU_H
#define LAUNCH_NPU_H

#include <string>
#include "3rdparty/rknpu2/librknn_api/include/rknn_api.h"

namespace vuprs
{
    class RknnModel
    {
    public:
        RknnModel(const std::string &model_path);
        RknnModel() : ctx_(0) {}
        ~RknnModel();

        RknnModel(const RknnModel &) = delete;
        RknnModel &operator=(const RknnModel &) = delete;
        RknnModel(RknnModel &&other) noexcept;
        RknnModel &operator=(RknnModel &&other) noexcept;

        void InitModel(const std::string &model_path);
        void run();
        void SetInput(uint32_t index, void *buffer, uint32_t size);
        void GetOutput(uint32_t index, void *buffer, uint32_t size);
        bool ModelReady() const { return this->ctx_ != 0; }

        uint32_t GetInputCount() const { return io_num_.n_input; }
        uint32_t GetOutputCount() const { return io_num_.n_output; }
        const std::vector<rknn_tensor_attr> &GetInputAttrs() const { return input_attrs_; }
        const std::vector<rknn_tensor_attr> &GetOutputAttrs() const { return output_attrs_; }

    private:
        rknn_context ctx_;                           /* RKNN context */
        rknn_input_output_num io_num_;               /* input & output number */
        std::vector<rknn_tensor_attr> input_attrs_;  /* attribute of input tensor */
        std::vector<rknn_tensor_attr> output_attrs_; /* attribute of output tensor */

        void QueryIOInfo();
        void InitTensorAttrs();
    };
}

#endif
