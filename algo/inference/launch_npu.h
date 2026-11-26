#ifndef LAUNCH_NPU_H
#define LAUNCH_NPU_H

#include <string>
#include "3rdparty/rknpu2/librknn_api/include/rknn_api.h"

namespace vuprs
{
    /**
     * @note Tensor layout must match the model (specified per-call in SetInput).
     */
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

        /**
         * @brief Init inference model.
         *
         * @param model_path Model path.
         */
        void InitModel(const std::string &model_path);

        /**
         * @brief Run inference.
         */
        void run();

        /**
         * @brief Set data to one input tensor of the model.
         *
         * @param index Index of input tensor (0, 1, ...).
         * @param buffer Pointer to the tensor data. The data must be arranged in
         *               memory according to the specified `layout` (e.g., NCHW or NHWC).
         *               The caller is responsible for ensuring the data layout matches
         *               the tensor's expected format; otherwise inference results may
         *               be incorrect or the API may fail.
         * @param size Size of the tensor data in bytes. Must exactly match the
         *             product of tensor dimensions, data type size, and channel count,
         *             considering the layout.
         * @param type Data type of each element (e.g., RKNN_TENSOR_UINT8, RKNN_TENSOR_FLOAT32).
         *             The buffer's memory layout must also respect this type's alignment.
         * @param layout Tensor layout (RKNN_TENSOR_NCHW or RKNN_TENSOR_NHWC). This
         *               defines the order of dimensions in the buffer. For NCHW,
         *               data is stored as [batch, channels, height, width] contiguous
         *               per channel; for NHWC, it is [batch, height, width, channels].
         *               Make sure the buffer is filled accordingly.
         */
        void SetInput(uint32_t index,
                      void *buffer,
                      uint32_t size,
                      rknn_tensor_type type = rknn_tensor_type::RKNN_TENSOR_UINT8,
                      rknn_tensor_format layout = rknn_tensor_format::RKNN_TENSOR_NCHW);

        /**
         * @brief Get output tensor of model.
         *
         * @param index Index of output tensor (0, 1, ...)
         * @param buffer Pointer of the output data.
         * @param size The size of output tensor (bytes).
         */
        void GetOutput(uint32_t index, void *buffer, uint32_t size);

        /**
         * @brief Check if the model ready to inference.
         */
        bool ModelReady() const { return this->ctx_ != 0; }

        uint32_t GetInputCount() const { return io_num_.n_input; }
        uint32_t GetOutputCount() const { return io_num_.n_output; }
        int64_t GetInferenceRuntime() const;
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
