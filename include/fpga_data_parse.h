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

#include "fpga_config.h"
#include "aligned_data_structure.h"

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
 *      ADC_CHANNEL__A_1                 2U
 *      ADC_CHANNEL__A_2                 0U
 *      ADC_CHANNEL__A_3                 1U
 *      ...                              ...
 * 
 * -----------------------------------------------------------------
 *      ADC Channel                      Storage position
 * -----------------------------------------------------------------
 * 
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

/**
 * -----------------------------------------------------------------
 * -----------------------------------------------------------------
 */

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
     * @param buffer data buffer, must be written in advance.
     * @param result result list, result[c][d] means: channel is 'c' & data pointer is 'd'.
     *               result[0]: data list of ADC-CH-A1;
     *               result[1]: data list of ADC-CH-A2;
     *               ...        ...
     *               result[7]: data list of ADC-CH-A8;
     *               result[8]: data list of ADC-CH-B1;
     *               result[9]: data list of ADC-CH-B2;
     *               ...        ...
     *               result[15]: data list of ADC-CH-B8;
     * @param adcFeatures adc features, must be load in advance (from JSON file).
     * @retval true: convert success;
     *         false: convert failed (do not find data in the buffer).
     * @throw 1. std::runtime_error("Buffer is empty, convert disabled"), when buffer is empty;
     *        2. std::runtime_error("Do not find ADC features, convert disabled"), when adc features are empty.
     */
    bool BufferData2ADCChannels(const vuprs::AlignedBufferDMA *buffer, std::vector<std::vector<double>> *result, const vuprs::FPGAhardwareConfigADC &adcFeatures);

    class CRC8List
    {
        private:
    
            std::vector<uint8_t> crcList;
            uint8_t CalculateCRC(const uint8_t &source, const uint16_t &crcPolynomialCode);

        public:

            CRC8List(const uint16_t &crcPolynomialCode);
            ~CRC8List();
            uint8_t CRCValue(const uint8_t &source);
    };

    class ADCFrame
    {
        private:

            std::vector<uint16_t> adcData;
            std::vector<uint8_t> crcDataH, crcDataL;

        public:

            ADCFrame();
            ~ADCFrame();

            void UpdateData(const int &index, const uint32_t &data);

            bool CheckCRC(const int &channel);
            uint16_t GetChannelValue(const int &channel);

    };
}

#endif
