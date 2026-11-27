#ifndef FPGA_DATA_CONVERSION_H
#define FPGA_DATA_CONVERSION_H

#include <vector>
#include <fstream>
#include <stdint.h>
#include <complex>
#include <algorithm>
#include "misc/aligned_buffer.h"
#include "misc/file_processing.h"
#include "fpga/fpga_data_type.h"

#define ADC_CHANNEL_NUMBER 16U

/**
 * @header
 * <4B> [header]
 *
 * @data ADC-A
 * <2B> [ch-A1]
 * <2B> [ch-A2]
 * ...
 * <2B> [ch-A8]
 *
 * @data ADC-B
 * <2B> [ch-B1]
 * <2B> [ch-B2]
 * ...
 * <2B> [ch-B8]
 *
 * @tailer
 * <4B> [tailer]
 */
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
/* -------------- Circular Buffer - Channel Name Define --------------- */
/* -------------------------------------------------------------------- */

#define ADC_CHANNEL_NAME__A1 "A1"
#define ADC_CHANNEL_NAME__A2 "A2"
#define ADC_CHANNEL_NAME__A3 "A3"
#define ADC_CHANNEL_NAME__A4 "A4"
#define ADC_CHANNEL_NAME__A5 "A5"
#define ADC_CHANNEL_NAME__A6 "A6"
#define ADC_CHANNEL_NAME__A7 "A7"
#define ADC_CHANNEL_NAME__A8 "A8"

#define ADC_CHANNEL_NAME__B1 "B1"
#define ADC_CHANNEL_NAME__B2 "B2"
#define ADC_CHANNEL_NAME__B3 "B3"
#define ADC_CHANNEL_NAME__B4 "B4"
#define ADC_CHANNEL_NAME__B5 "B5"
#define ADC_CHANNEL_NAME__B6 "B6"
#define ADC_CHANNEL_NAME__B7 "B7"
#define ADC_CHANNEL_NAME__B8 "B8"

#define IS_ADC_CHANNEL_NAME(VAL)                                   \
    (VAL == ADC_CHANNEL_NAME__A1 || VAL == ADC_CHANNEL_NAME__A2 || \
     VAL == ADC_CHANNEL_NAME__A3 || VAL == ADC_CHANNEL_NAME__A4 || \
     VAL == ADC_CHANNEL_NAME__A5 || VAL == ADC_CHANNEL_NAME__A6 || \
     VAL == ADC_CHANNEL_NAME__A7 || VAL == ADC_CHANNEL_NAME__A8 || \
     VAL == ADC_CHANNEL_NAME__B1 || VAL == ADC_CHANNEL_NAME__B2 || \
     VAL == ADC_CHANNEL_NAME__B3 || VAL == ADC_CHANNEL_NAME__B4 || \
     VAL == ADC_CHANNEL_NAME__B5 || VAL == ADC_CHANNEL_NAME__B6 || \
     VAL == ADC_CHANNEL_NAME__B7 || VAL == ADC_CHANNEL_NAME__B8)

namespace vuprs
{
    const std::vector<std::string> ADC_CHANNEL_ADDR_MAP = {
        ADC_CHANNEL_NAME__A1, ADC_CHANNEL_NAME__A2, ADC_CHANNEL_NAME__A3, ADC_CHANNEL_NAME__A4,
        ADC_CHANNEL_NAME__A5, ADC_CHANNEL_NAME__A6, ADC_CHANNEL_NAME__A7, ADC_CHANNEL_NAME__A8,
        ADC_CHANNEL_NAME__B1, ADC_CHANNEL_NAME__B2, ADC_CHANNEL_NAME__B3, ADC_CHANNEL_NAME__B4,
        ADC_CHANNEL_NAME__B5, ADC_CHANNEL_NAME__B6, ADC_CHANNEL_NAME__B7, ADC_CHANNEL_NAME__B8}; /* addr in one frame: [0, 1, 2, ...], corresponding channel: [A1, A2, ...] */

    /**
     * @brief Multi-channel signal data.
     *
     * @note _channel_data[i] always corresponds to ADC channel i (A1..A8, B1..B8).
     */
    class SignalData
    {
    public:
        std::vector<std::vector<std::complex<double>>> _channel_data; /* [ADC channel][point] */
        std::vector<std::string> _channel_name;                       /* channel name for each ADC address */
        double fs = 0.0, sampling_time = 0.0;
        int signal_points = 0; /* signal data point number for one channel */

        SignalData();

        /**
         * @brief Save all data to CSV file.
         *
         * @param output_file output file name.
         *
         * @throw std::runtime_error
         */
        void ToCSV(const std::string &output_file);
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
            return;
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
}

#endif
