#ifndef FPGA_DATA_CONVERSION_H
#define FPGA_DATA_CONVERSION_H

#include <vector>
#include <fstream>
#include <stdint.h>
#include <complex>
#include <unordered_map>

#include "aligned_buffer.h"
#include "fpga_data_conv.h"

#define ADC_CHANNEL_NUMBER               16U
#define ADC_FRAME_HALF_WORD_SIZE         20U  /* 10 words */

#define ADC_FRAME_HEADER                 (uint32_t)0x0000FFF0U
#define ADC_FRAME_TAILER                 (uint32_t)0x0000FF0FU

constexpr uint16_t ADC_FRAME_HEADER__H = (uint16_t)((ADC_FRAME_HEADER >> 16) & 0xFFFF);
constexpr uint16_t ADC_FRAME_HEADER__L = (uint16_t)(ADC_FRAME_HEADER & 0xFFFF);

constexpr uint16_t ADC_FRAME_TAILER__H = (uint16_t)((ADC_FRAME_TAILER >> 16) & 0xFFFF);
constexpr uint16_t ADC_FRAME_TAILER__L = (uint16_t)(ADC_FRAME_TAILER & 0xFFFF);

#define IS_FRAME_HEADER(VAL_L, VAL_H) (VAL_L == ADC_FRAME_HEADER__L && VAL_H == ADC_FRAME_HEADER__H)
#define IS_FRAME_TAILER(VAL_L, VAL_H) (VAL_L == ADC_FRAME_TAILER__L && VAL_H == ADC_FRAME_TAILER__H)

/* -------------------------------------------------------------------- */
/* ---------------- Circular Buffer - ADC Channel Index --------------- */
/* -------------------------------------------------------------------- */

#define ADC_CHANNEL__A_1                         0U
#define ADC_CHANNEL__A_2                         1U
#define ADC_CHANNEL__A_3                         2U
#define ADC_CHANNEL__A_4                         3U
#define ADC_CHANNEL__A_5                         4U
#define ADC_CHANNEL__A_6                         5U
#define ADC_CHANNEL__A_7                         6U
#define ADC_CHANNEL__A_8                         7U

#define ADC_CHANNEL__B_1                         8U
#define ADC_CHANNEL__B_2                         9U
#define ADC_CHANNEL__B_3                         10U
#define ADC_CHANNEL__B_4                         11U
#define ADC_CHANNEL__B_5                         12U
#define ADC_CHANNEL__B_6                         13U
#define ADC_CHANNEL__B_7                         14U
#define ADC_CHANNEL__B_8                         15U

/* -------------------------------------------------------------------- */
/* -------------- Circular Buffer - Channel Name Define --------------- */
/* -------------------------------------------------------------------- */

#define ADC_CHANNEL_NAME__A_1                     "CH-A-1"
#define ADC_CHANNEL_NAME__A_2                     "CH-A-2"
#define ADC_CHANNEL_NAME__A_3                     "CH-A-3"
#define ADC_CHANNEL_NAME__A_4                     "CH-A-4"
#define ADC_CHANNEL_NAME__A_5                     "CH-A-5"
#define ADC_CHANNEL_NAME__A_6                     "CH-A-6"
#define ADC_CHANNEL_NAME__A_7                     "CH-A-7"
#define ADC_CHANNEL_NAME__A_8                     "CH-A-8"

#define ADC_CHANNEL_NAME__B_1                     "CH-B-1"
#define ADC_CHANNEL_NAME__B_2                     "CH-B-2"
#define ADC_CHANNEL_NAME__B_3                     "CH-B-3"
#define ADC_CHANNEL_NAME__B_4                     "CH-B-4"
#define ADC_CHANNEL_NAME__B_5                     "CH-B-5"
#define ADC_CHANNEL_NAME__B_6                     "CH-B-6"
#define ADC_CHANNEL_NAME__B_7                     "CH-B-7"
#define ADC_CHANNEL_NAME__B_8                     "CH-B-8"

namespace vuprs
{
    const std::vector<std::string> ADC_CHANNEL_ADDR_MAP = {
        ADC_CHANNEL_NAME__A_1, ADC_CHANNEL_NAME__A_2, ADC_CHANNEL_NAME__A_3, ADC_CHANNEL_NAME__A_4,
        ADC_CHANNEL_NAME__A_5, ADC_CHANNEL_NAME__A_6, ADC_CHANNEL_NAME__A_7, ADC_CHANNEL_NAME__A_8,
        ADC_CHANNEL_NAME__B_1, ADC_CHANNEL_NAME__B_2, ADC_CHANNEL_NAME__B_3, ADC_CHANNEL_NAME__B_4,
        ADC_CHANNEL_NAME__B_5, ADC_CHANNEL_NAME__B_6, ADC_CHANNEL_NAME__B_7, ADC_CHANNEL_NAME__B_8
    };  /* addr in one frame: [0, 1, 2, ...], corresponding channel: [A1, A2, ...] */

    class SignalData
    {
        private:

            std::unordered_map<std::string, uint8_t> CHANNEL_NAME__TO__CHANNEL_INDEX;

        public:

            std::vector<std::vector<std::complex<double>>> _channelData;
            std::vector<std::string> _channelName;

            double samplingFrequency = 0.0, samplingTime = 0.0;
            int signalPoints = 0;  /* signal data point number for one channel */

            /**
             * @brief Indicate whether this channel is included.
             * 
             * @param channelName channel name, optional:
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
            bool contains(const std::string &channelName) const;

            /**
             * @brief Get channel data.
             * 
             * @param channelName channel name.
             * @param data output data.
             * 
             * @throw std::runtime_error
             */
            void GetChannelData(const std::string &channelName, std::vector<std::complex<double>> *data) const;

            /**
             * @brief Delete channel data.
             * 
             * @param channelName channel name.
             */
            void DeleteChannelData(const std::string &channelName);

            /**
             * @brief Save all data to CSV file.
             * 
             * @param outputFile output file name.
             * 
             * @throw std::runtime_error
             */
            void ToCSV(const std::string &outputFile);

            void _UpdataHashMap();
    };

    /**
     * @brief Convert circular buffer data to adcData.
     * 
     * @param buffer Input aligned DMA buffer.
     * @param adcData Output ADC data (ch1: adcData[0], ch2: adcData[1], ...).
     * @param fs sampling frequency.
     * @param v_scale ADC voltage scale (5.0 or 10.0 for AD7606)
     * 
     * @retval true: success.
     * @retval false: failed.
     */
    bool FPGACircularBuffer2Frames(vuprs::AlignedBufferDMA *buffer, vuprs::SignalData *adcData, double fs = 1.0, double v_scale = 1.0);

    /**
     * @brief Convert memory data to signed float.
     * 
     * @note 1st: uint32_t to signed, 2nd: signed to float.
     * 
     * @param buffer Input aligned DMA buffer.
     * @param beamformingResult Output beam forming result.
     * 
     * @retval true: success.
     * @retval false: failed.
     */
    bool FPGAMemoryBuffer2Frames(vuprs::AlignedBufferDMA *buffer, std::vector<double> *beamformingResult, double samplingFrequency = 1.0);
}

#endif
