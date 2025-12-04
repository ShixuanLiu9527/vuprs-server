/**
 * @brief   This document is the data parsing interface for FPGA data.
 * @version 1.0
 * @author  Shixuan Liu, Tongji University
 * @date    2025-9
 */

#ifndef FPGA_DATA_PARSE_H
#define FPGA_DATA_PARSE_H

#include <vector>
#include <fstream>
#include <stdint.h>
#include <complex>

#include "fpga_config.h"
#include "aligned_buffer.h"
#include "signal_data.h"

/**
 * 
 * Define the storage order of ADC channels here.
 * 
 * e.g. If the data is stored in the following order (word is 32 bit):
 * 
 *      word 0:  ADC-CH-A2
 *      word 1:  ADC-CH-A3
 *      word 2:  ADC-CH-A1
 *      ...      ...
 * 
 * then the storage order must be defined in the following way:
 * 
 *      ADC_CHANNEL_POSITION__A_1                 2U
 *      ADC_CHANNEL_POSITION__A_2                 0U
 *      ADC_CHANNEL_POSITION__A_3                 1U
 *      ...                                       ...
 * 
 * -----------------------------------------------------------------
 *      ADC Channel                               Storage position
 * -----------------------------------------------------------------
 * 
 */

#define ADC_CHANNEL_POSITION__A_1                 0U
#define ADC_CHANNEL_POSITION__A_2                 1U
#define ADC_CHANNEL_POSITION__A_3                 2U
#define ADC_CHANNEL_POSITION__A_4                 3U
#define ADC_CHANNEL_POSITION__A_5                 4U
#define ADC_CHANNEL_POSITION__A_6                 5U
#define ADC_CHANNEL_POSITION__A_7                 6U
#define ADC_CHANNEL_POSITION__A_8                 7U

#define ADC_CHANNEL_POSITION__B_1                 8U
#define ADC_CHANNEL_POSITION__B_2                 9U
#define ADC_CHANNEL_POSITION__B_3                 10U
#define ADC_CHANNEL_POSITION__B_4                 11U
#define ADC_CHANNEL_POSITION__B_5                 12U
#define ADC_CHANNEL_POSITION__B_6                 13U
#define ADC_CHANNEL_POSITION__B_7                 14U
#define ADC_CHANNEL_POSITION__B_8                 15U

/**
 * -----------------------------------------------------------------
 * -----------------------------------------------------------------
 */

#define IS_ADC_CHANNEL_POSITION(VAL) \
(VAL == ADC_CHANNEL_POSITION__A_1                 || \
 VAL == ADC_CHANNEL_POSITION__A_2                 || \
 VAL == ADC_CHANNEL_POSITION__A_3                 || \
 VAL == ADC_CHANNEL_POSITION__A_4                 || \
 VAL == ADC_CHANNEL_POSITION__A_5                 || \
 VAL == ADC_CHANNEL_POSITION__A_6                 || \
 VAL == ADC_CHANNEL_POSITION__A_7                 || \
 VAL == ADC_CHANNEL_POSITION__A_8                 || \
 VAL == ADC_CHANNEL_POSITION__B_1                 || \
 VAL == ADC_CHANNEL_POSITION__B_2                 || \
 VAL == ADC_CHANNEL_POSITION__B_3                 || \
 VAL == ADC_CHANNEL_POSITION__B_4                 || \
 VAL == ADC_CHANNEL_POSITION__B_5                 || \
 VAL == ADC_CHANNEL_POSITION__B_6                 || \
 VAL == ADC_CHANNEL_POSITION__B_7                 || \
 VAL == ADC_CHANNEL_POSITION__B_8)

/**
 * ADC channel
 */

#define ADC_CHANNEL__A_1                 0U
#define ADC_CHANNEL__A_2                 1U
#define ADC_CHANNEL__A_3                 2U
#define ADC_CHANNEL__A_4                 3U
#define ADC_CHANNEL__A_5                 4U
#define ADC_CHANNEL__A_6                 5U
#define ADC_CHANNEL__A_7                 6U
#define ADC_CHANNEL__A_8                 7U

#define ADC_CHANNEL__B_1                 8U
#define ADC_CHANNEL__B_2                 9U
#define ADC_CHANNEL__B_3                 10U
#define ADC_CHANNEL__B_4                 11U
#define ADC_CHANNEL__B_5                 12U
#define ADC_CHANNEL__B_6                 13U
#define ADC_CHANNEL__B_7                 14U
#define ADC_CHANNEL__B_8                 15U

#define IS_ADC_CHANNEL(VAL) \
(VAL == ADC_CHANNEL__A_1                 || \
 VAL == ADC_CHANNEL__A_2                 || \
 VAL == ADC_CHANNEL__A_3                 || \
 VAL == ADC_CHANNEL__A_4                 || \
 VAL == ADC_CHANNEL__A_5                 || \
 VAL == ADC_CHANNEL__A_6                 || \
 VAL == ADC_CHANNEL__A_7                 || \
 VAL == ADC_CHANNEL__A_8                 || \
 VAL == ADC_CHANNEL__B_1                 || \
 VAL == ADC_CHANNEL__B_2                 || \
 VAL == ADC_CHANNEL__B_3                 || \
 VAL == ADC_CHANNEL__B_4                 || \
 VAL == ADC_CHANNEL__B_5                 || \
 VAL == ADC_CHANNEL__B_6                 || \
 VAL == ADC_CHANNEL__B_7                 || \
 VAL == ADC_CHANNEL__B_8)

/**
 * Channel name define
 */

#define ADC_CHANNEL_NAME__A_1            "CH-A-1"
#define ADC_CHANNEL_NAME__A_2            "CH-A-2"
#define ADC_CHANNEL_NAME__A_3            "CH-A-3"
#define ADC_CHANNEL_NAME__A_4            "CH-A-4"
#define ADC_CHANNEL_NAME__A_5            "CH-A-5"
#define ADC_CHANNEL_NAME__A_6            "CH-A-6"
#define ADC_CHANNEL_NAME__A_7            "CH-A-7"
#define ADC_CHANNEL_NAME__A_8            "CH-A-8"

#define ADC_CHANNEL_NAME__B_1            "CH-B-1"
#define ADC_CHANNEL_NAME__B_2            "CH-B-2"
#define ADC_CHANNEL_NAME__B_3            "CH-B-3"
#define ADC_CHANNEL_NAME__B_4            "CH-B-4"
#define ADC_CHANNEL_NAME__B_5            "CH-B-5"
#define ADC_CHANNEL_NAME__B_6            "CH-B-6"
#define ADC_CHANNEL_NAME__B_7            "CH-B-7"
#define ADC_CHANNEL_NAME__B_8            "CH-B-8"

#define IS_ADC_CHANNEL_NAME(VAL) \
(VAL == ADC_CHANNEL_NAME__A_1             || \
 VAL == ADC_CHANNEL_NAME__A_2             || \
 VAL == ADC_CHANNEL_NAME__A_3             || \
 VAL == ADC_CHANNEL_NAME__A_4             || \
 VAL == ADC_CHANNEL_NAME__A_5             || \
 VAL == ADC_CHANNEL_NAME__A_6             || \
 VAL == ADC_CHANNEL_NAME__A_7             || \
 VAL == ADC_CHANNEL_NAME__A_8             || \
 VAL == ADC_CHANNEL_NAME__B_1             || \
 VAL == ADC_CHANNEL_NAME__B_2             || \
 VAL == ADC_CHANNEL_NAME__B_3             || \
 VAL == ADC_CHANNEL_NAME__B_4             || \
 VAL == ADC_CHANNEL_NAME__B_5             || \
 VAL == ADC_CHANNEL_NAME__B_6             || \
 VAL == ADC_CHANNEL_NAME__B_7             || \
 VAL == ADC_CHANNEL_NAME__B_8)

/**
 * Data header & Data Tailer
 */

#define ADC_DATA_HEADER                  0x0000FFF0
#define ADC_DATA_TAILER                  0x0000FF0F

/**
 * Frame Features & ADC Features define
 */

#define ADC_FRAME_WORD_LENGTH            18U
#define ADC_CHANNELS                     16U
#define ADC_DATAWIDTH                    16U

/**
 * CRC Code (CRC8_CDMA2000)
 * CRC p(x) = 1+x^1+x^3+x^4+x^7+x^8 (9'b110011011)
 */

#define CRC8_POLYNOMIAL_CDMA2000         0x19B

namespace vuprs
{
    /**
     * @brief Convert buffer data to ADC Channels.
     * 
     * @param buffer data buffer, must be written in advance.
     * @param adcFeatures adc features, must be load in advance (from JSON file).
     * @param channelData result list, result[c][d] means: channel is 'c' & data pointer is 'd'.
     *               result[0]: data list of ADC-CH-A1;
     *               result[1]: data list of ADC-CH-A2;
     *               ...        ...
     *               result[7]: data list of ADC-CH-A8;
     *               result[8]: data list of ADC-CH-B1;
     *               result[9]: data list of ADC-CH-B2;
     *               ...        ...
     *               result[15]: data list of ADC-CH-B8;
     * @param channelName channel name corresponding to data index in [channelData]
     * 
     * @retval true: convert success;
     *         false: convert failed (do not find data in the buffer).
     * 
     * @throw 1. std::runtime_error("Buffer is empty, convert disabled"), when buffer is empty;
     *        2. std::runtime_error("Do not find ADC features, convert disabled"), when adc features are empty.
     */
    bool BufferData2ADCChannels(const vuprs::AlignedBufferServer *buffer, 
                                const vuprs::FPGAhardwareConfigADC &adcFeatures, 
                                double samplingFrequency,
                                vuprs::SignalData *adcData);

    /**
     * @brief Convert buffer data to ADC Channels.
     * 
     * @param buffer data buffer, must be written in advance.
     * @param adcFeatures adc features, must be load in advance (from JSON file).
     * @param samplingFrequency sampling frequency.
     * @param status calculate status.
     * 
     * @retval signal data object.
     * 
     * @throw 1. std::runtime_error("Buffer is empty, convert disabled"), when buffer is empty;
     *        2. std::runtime_error("Do not find ADC features, convert disabled"), when adc features are empty.
     */
    vuprs::SignalData BufferData2ADCChannels(const vuprs::AlignedBuffer *buffer, const vuprs::FPGAhardwareConfigADC &adcFeatures, 
                                             double samplingFrequency = 1.0, bool *status = nullptr);

    class CRC8List
    {
        private:
    
            std::vector<uint8_t> crcList;
            uint8_t CalculateCRC(uint8_t source, uint16_t crcPolynomialCode);

        public:

            CRC8List(uint16_t crcPolynomialCode);
            ~CRC8List();
            uint8_t CRCValue(uint8_t source);
    };

    class ADCFrame
    {
        private:

            std::vector<uint16_t> adcData;
            std::vector<uint8_t> crcDataH, crcDataL;

        public:

            ADCFrame();
            ~ADCFrame();

            void InputPositionData(int storagePosition, uint32_t data);

            bool CheckCRC(int storagePosition);
            uint16_t GetPositionValue(int storagePosition);

    };
}

#endif
