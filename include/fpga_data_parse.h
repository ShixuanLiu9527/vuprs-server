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

#define ADC_CHANNEL__V__A1                0
#define ADC_CHANNEL__V__A2                1
#define ADC_CHANNEL__V__A3                2
#define ADC_CHANNEL__V__A4                3
#define ADC_CHANNEL__V__A5                4
#define ADC_CHANNEL__V__A6                5
#define ADC_CHANNEL__V__A7                6
#define ADC_CHANNEL__V__A8                7
#define ADC_CHANNEL__V__B1                8
#define ADC_CHANNEL__V__B2                9
#define ADC_CHANNEL__V__B3                10
#define ADC_CHANNEL__V__B4                11
#define ADC_CHANNEL__V__B5                12
#define ADC_CHANNEL__V__B6                13
#define ADC_CHANNEL__V__B7                14
#define ADC_CHANNEL__V__B8                15

namespace vuprs
{
    bool BufferData2ADCChannels(const vuprs::AlignedBufferDMA *buffer, std::vector<std::vector<double>> *result, const vuprs::FPGAhardwareConfigADC &adcFeatures);
}

#endif
