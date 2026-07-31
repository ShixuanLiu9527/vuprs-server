#ifndef FPGA_DATA_CONVERSION_H
#define FPGA_DATA_CONVERSION_H

#include <vector>
#include <fstream>
#include <stdint.h>
#include <complex>
#include <unordered_map>
#include <algorithm>
#include "system_tools/aligned_buffer.h"
#include "system_tools/file_processing.h"
#include "fpga/fpga_data_type.h"

#define ADC_CHANNEL_NUMBER 16U
#define ADC_FRAME_WORD_SIZE 10U /* 10 words */

#define ADC_FRAME_HEADER (uint32_t)0x0000FFF0U
#define ADC_FRAME_TAILER (uint32_t)0x0000FF0FU

constexpr uint32_t ADC_FRAME_HALF_WORD_SIZE = (uint32_t)(ADC_FRAME_WORD_SIZE * 2);

constexpr uint16_t ADC_FRAME_HEADER__H = (uint16_t)((ADC_FRAME_HEADER >> 16) & 0xFFFF);
constexpr uint16_t ADC_FRAME_HEADER__L = (uint16_t)(ADC_FRAME_HEADER & 0xFFFF);

constexpr uint16_t ADC_FRAME_TAILER__H = (uint16_t)((ADC_FRAME_TAILER >> 16) & 0xFFFF);
constexpr uint16_t ADC_FRAME_TAILER__L = (uint16_t)(ADC_FRAME_TAILER & 0xFFFF);

#define FPGA_CBF_TO_DATA_POSITION(CBF) (int)(((int)(CBF) + sizeof(uint32_t)) / (sizeof(uint32_t) * ADC_FRAME_WORD_SIZE) - 1)

#define IS_FRAME_HEADER(VAL_L, VAL_H) (VAL_L == ADC_FRAME_HEADER__L && VAL_H == ADC_FRAME_HEADER__H)
#define IS_FRAME_TAILER(VAL_L, VAL_H) (VAL_L == ADC_FRAME_TAILER__L && VAL_H == ADC_FRAME_TAILER__H)

#define UINT16_SPLI_TO_UINT32(L, H) (uint32_t)((int32_t)((uint32_t)(H) << 16) | (uint32_t)(L)) /* {H, L} */

/* -------------------------------------------------------------------- */
/* ---------------- Circular Buffer - ADC Channel Index --------------- */
/* -------------------------------------------------------------------- */

#define ADC_CHANNEL__A_1 0U
#define ADC_CHANNEL__A_2 1U
#define ADC_CHANNEL__A_3 2U
#define ADC_CHANNEL__A_4 3U
#define ADC_CHANNEL__A_5 4U
#define ADC_CHANNEL__A_6 5U
#define ADC_CHANNEL__A_7 6U
#define ADC_CHANNEL__A_8 7U

#define ADC_CHANNEL__B_1 8U
#define ADC_CHANNEL__B_2 9U
#define ADC_CHANNEL__B_3 10U
#define ADC_CHANNEL__B_4 11U
#define ADC_CHANNEL__B_5 12U
#define ADC_CHANNEL__B_6 13U
#define ADC_CHANNEL__B_7 14U
#define ADC_CHANNEL__B_8 15U

/* -------------------------------------------------------------------- */
/* -------------- Circular Buffer - Channel Name Define --------------- */
/* -------------------------------------------------------------------- */

#define ADC_CHANNEL_NAME__A_1 "CH-A-1"
#define ADC_CHANNEL_NAME__A_2 "CH-A-2"
#define ADC_CHANNEL_NAME__A_3 "CH-A-3"
#define ADC_CHANNEL_NAME__A_4 "CH-A-4"
#define ADC_CHANNEL_NAME__A_5 "CH-A-5"
#define ADC_CHANNEL_NAME__A_6 "CH-A-6"
#define ADC_CHANNEL_NAME__A_7 "CH-A-7"
#define ADC_CHANNEL_NAME__A_8 "CH-A-8"

#define ADC_CHANNEL_NAME__B_1 "CH-B-1"
#define ADC_CHANNEL_NAME__B_2 "CH-B-2"
#define ADC_CHANNEL_NAME__B_3 "CH-B-3"
#define ADC_CHANNEL_NAME__B_4 "CH-B-4"
#define ADC_CHANNEL_NAME__B_5 "CH-B-5"
#define ADC_CHANNEL_NAME__B_6 "CH-B-6"
#define ADC_CHANNEL_NAME__B_7 "CH-B-7"
#define ADC_CHANNEL_NAME__B_8 "CH-B-8"

#define IS_ADC_CHANNEL_NAME(VAL)     \
    (VAL == ADC_CHANNEL_NAME__A_1 || \
     VAL == ADC_CHANNEL_NAME__A_2 || \
     VAL == ADC_CHANNEL_NAME__A_3 || \
     VAL == ADC_CHANNEL_NAME__A_4 || \
     VAL == ADC_CHANNEL_NAME__A_5 || \
     VAL == ADC_CHANNEL_NAME__A_6 || \
     VAL == ADC_CHANNEL_NAME__A_7 || \
     VAL == ADC_CHANNEL_NAME__A_8 || \
     VAL == ADC_CHANNEL_NAME__B_1 || \
     VAL == ADC_CHANNEL_NAME__B_2 || \
     VAL == ADC_CHANNEL_NAME__B_3 || \
     VAL == ADC_CHANNEL_NAME__B_4 || \
     VAL == ADC_CHANNEL_NAME__B_5 || \
     VAL == ADC_CHANNEL_NAME__B_6 || \
     VAL == ADC_CHANNEL_NAME__B_7 || \
     VAL == ADC_CHANNEL_NAME__B_8)

namespace vuprs
{
    const std::vector<std::string> ADC_CHANNEL_ADDR_MAP = {
        ADC_CHANNEL_NAME__A_1, ADC_CHANNEL_NAME__A_2, ADC_CHANNEL_NAME__A_3, ADC_CHANNEL_NAME__A_4,
        ADC_CHANNEL_NAME__A_5, ADC_CHANNEL_NAME__A_6, ADC_CHANNEL_NAME__A_7, ADC_CHANNEL_NAME__A_8,
        ADC_CHANNEL_NAME__B_1, ADC_CHANNEL_NAME__B_2, ADC_CHANNEL_NAME__B_3, ADC_CHANNEL_NAME__B_4,
        ADC_CHANNEL_NAME__B_5, ADC_CHANNEL_NAME__B_6, ADC_CHANNEL_NAME__B_7, ADC_CHANNEL_NAME__B_8}; /* addr in one frame: [0, 1, 2, ...], corresponding channel: [A1, A2, ...] */

    class SignalData
    {
    private:
        std::unordered_map<std::string, uint8_t> CHANNEL_NAME__TO__CHANNEL_INDEX;

    public:
        SignalData();

        std::vector<std::vector<std::complex<double>>> _channel_data;
        std::vector<std::string> _channel_name;

        double fs = 0.0, sampling_time = 0.0;
        int signal_points = 0; /* signal data point number for one channel */

        /**
         * @brief Indicate whether this channel is included.
         *
         * @param channel_name channel name, optional:
         *                    CH-A-1: channel-A1;
         *                    CH-A-2: channel-A2;
         *                    CH-A-3: channel-A3;
         *                    CH-A-4: channel-A4;
         *                    CH-A-5: channel-A5;
         *                    CH-A-6: channel-A6;
         *                    CH-A-7: channel-A7;
         *                    CH-A-8: channel-A8;
         *                    CH-B-1: channel-B1;
         *                    CH-B-2: channel-B2;
         *                    CH-B-3: channel-B3;
         *                    CH-B-4: channel-B4;
         *                    CH-B-5: channel-B5;
         *                    CH-B-6: channel-B6;
         *                    CH-B-7: channel-B7;
         *                    CH-B-8: channel-B8;
         *
         * @retval true: this channel is included.
         * @retval false: this channel is not included.
         */
        bool contains(const std::string &channel_name) const;

        /**
         * @brief Get channel data.
         *
         * @param channel_name channel name.
         * @param data output data.
         *
         * @throw std::runtime_error
         */
        void GetChannelData(const std::string &channel_name, std::vector<std::complex<double>> *data) const;

        /**
         * @brief Delete channel data.
         *
         * @param channel_name channel name.
         */
        void DeleteChannelData(const std::string &channel_name);

        /**
         * @brief Save all data to CSV file.
         *
         * @param output_file output file name.
         *
         * @throw std::runtime_error
         */
        void ToCSV(const std::string &output_file);

        void _UpdataHashMap();
    };

    /**
     * @brief Rotate data in circular buffer.
     *
     * @note e.g. Input: vec = [1, 2, 3, 4, 5, 6],
     * @note and current_pointer = 2,
     * @note then Output = [4, 5, 6, 1, 2, 3].
     */
    template <typename T>
    void RotateCircularBuffer(std::vector<T> *vec, uint32_t current_pointer)
    {
        if (vec->empty() || current_pointer >= vec->size() - 1)
        {
            return;
        }
        std::rotate(vec->begin(), vec->begin() + current_pointer + 1, vec->end());
    }

    /**
     * @brief Parse circular buffer data to adc_data.
     *
     * @param buffer Input aligned DMA buffer.
     * @param adc_data Output ADC data (ch1: adc_data[0], ch2: adc_data[1], ...).
     * @param fs sampling frequency.
     * @param v_scale ADC voltage scale (5.0 or 10.0 for AD7606)
     * @param point_pos_cbf current data pointer (= (CBF + sizeof(uint32_t)) / (sizeof(uint32_t) * ADC_FRAME_WORD_SIZE) - 1).
     *
     * @retval true: success.
     * @retval false: failed.
     */
    bool FPGACircularBuffer2Frames(vuprs::AlignedBufferDMA *buffer,
                                   vuprs::SignalData *adc_data,
                                   double fs,
                                   double v_scale,
                                   uint32_t point_pos_cbf);

    /**
     * @brief Convert memory data to signed float.
     *
     * @note 1st: uint32_t to signed, 2nd: signed to float.
     *
     * @param buffer Input aligned DMA buffer.
     * @param beamforming_result Output beam forming result.
     * @param fs sampling frequency.
     *
     * @retval true: success.
     * @retval false: failed.
     */
    bool FPGAMemoryBuffer2Frames(vuprs::AlignedBufferDMA *buffer,
                                 std::vector<double> *beamforming_result,
                                 double fs);
}

#endif
