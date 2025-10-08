#include "fpga_data_parse.h"

bool vuprs::BufferData2ADCChannels(const vuprs::AlignedBufferDMA *buffer, std::vector<std::vector<double>> *result, const vuprs::FPGAhardwareConfigADC &adcFeatures)
{
    /* ------------------------ Security Check Start ------------------------- */

    if (buffer->is_allocated() || buffer->size() == 0)
    {
        throw std::runtime_error("Buffer is empty, convert disabled");
    }

    if (!adcFeatures.configdown)
    {
        throw std::runtime_error("Do not find ADC features, convert disabled");
    }

    if (buffer->size() % adcFeatures.adcFrameSizeBytes != 0)
    {
        throw std::runtime_error("Buffer data is damaged, convert disabled");
    }

    /* ------------------------- Security Check End -------------------------- */

    /* Convert to uint32_t vector */

    std::vector<uint32_t> originData, processedData;
    uint64_t frameElements = 0, interval1Position = 0, interval2Position = 0;
    originData = buffer->to_vector<uint32_t>();

    /* Check first frame */

    frameElements = adcFeatures.adcFrameSizeBytes / sizeof(uint32_t);
    interval1Position = frameElements - 3;
    interval2Position = frameElements - 1;

    if (originData[interval1Position] == 0xFFFFFFFF && originData[interval2Position] == 0xFFFFFFFF)
    {
        processedData = originData;
    }
    
}