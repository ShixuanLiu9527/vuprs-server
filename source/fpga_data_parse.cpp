#include "fpga_data_parse.h"

vuprs::CRC8List globalCRCList(CRC8_POLYNOMIAL_CDMA2000);

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

    /* ------------------------- Security Check End -------------------------- */

    /* Convert to uint32_t vector */

    std::vector<uint32_t> originData;
    std::vector<vuprs::ADCFrame> adcFrames;
    vuprs::ADCFrame oneADCFrame;
    uint64_t wordsElements = 0, dataHeaderPointer = 0, dataTailerPointer = 0, adcFrameElements = 0;
    int16_t signedValue = 0;

    const std::vector<uint64_t> CHANNEL_MAPPING = {
        ADC_CHANNEL__A_1, ADC_CHANNEL__A_2, ADC_CHANNEL__A_3, ADC_CHANNEL__A_4,
        ADC_CHANNEL__A_5, ADC_CHANNEL__A_6, ADC_CHANNEL__A_7, ADC_CHANNEL__A_8,
        ADC_CHANNEL__B_1, ADC_CHANNEL__B_2, ADC_CHANNEL__B_3, ADC_CHANNEL__B_4,
        ADC_CHANNEL__B_5, ADC_CHANNEL__B_6, ADC_CHANNEL__B_7, ADC_CHANNEL__B_8
    };

    const double LSB_VALUE = pow(2, ADC_DATAWIDTH) / 2.0;

    result->clear();
    result->resize(ADC_CHANNELS);

    originData = buffer->to_vector<uint32_t>();

    if (originData.size() == 0)
    {
        return false;
    }

    wordsElements = originData.size();
    adcFrames.reserve(wordsElements / (ADC_FRAME_WORD_LENGTH) + 1);

    /* Check frame, find the process data */

    dataHeaderPointer = 0;
    dataTailerPointer = 0;

    while (dataHeaderPointer < wordsElements)
    {
        if (originData[dataHeaderPointer] == ADC_DATA_HEADER)  /* Find header */
        {
            dataTailerPointer = dataHeaderPointer + ADC_FRAME_WORD_LENGTH - 1;
            if (dataTailerPointer < wordsElements)
            {
                if (originData[dataTailerPointer] == ADC_DATA_TAILER)
                {
                    /* Push data */
                    for (uint64_t i = 0; i < (ADC_FRAME_WORD_LENGTH - 2); i++)
                    {
                        oneADCFrame.UpdateData(i, originData[dataHeaderPointer + i]);
                    }
                    adcFrames.push_back(oneADCFrame);
                }
            }
        
            /* Update pointer */
            
            dataHeaderPointer = dataTailerPointer + 1;
            if (dataHeaderPointer >= wordsElements) break;
        }
        else
        {
            dataHeaderPointer++;
            if (dataHeaderPointer >= wordsElements) break;
        }
    }

    if (adcFrames.size() == 0)
    {
        return false;
    }

    /* Calculate voltage */

    adcFrameElements = adcFrames.size();

    for (uint64_t i = 0; i < adcFrameElements; i++)
    {
        (*result)[i].resize(adcFrameElements);  /* Resize each channel */

        for (uint64_t j = 0; j < ADC_CHANNELS; j++)
        {
            if (adcFrames[i].CheckCRC(CHANNEL_MAPPING[j]))
            {
                signedValue = static_cast<int16_t>(adcFrames[i].GetChannelValue(CHANNEL_MAPPING[j]));
                (*result)[j][i] = static_cast<double>(signedValue) * adcFeatures.adcVoltageRangeRadius / LSB_VALUE;
            }
            else
            {
                (*result)[j][i] = adcFeatures.adcVoltageRangeRadius;
            }
        }
    }

    return true;
}

/* --------------------------------------------------------------------------------------------------------------- */
/* ------------------------------------------------- CRC List ---------------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

uint8_t vuprs::CRC8List::CalculateCRC(const uint8_t &source, const uint16_t &crcPolynomialCode)
{
    uint8_t crc;
    uint8_t CRC8_CDMA2000 = CRC8_POLYNOMIAL_CDMA2000 & (0xFF);
    crc = source;
    
    for (int i = 0; i < 8; i++) 
    {
        if(crc & 0x80)
        {
            crc = (crc << 1) ^ CRC8_POLYNOMIAL_CDMA2000;
        }
        else
        {
            crc = (crc << 1);
        }
    }

    return crc;
}

vuprs::CRC8List::CRC8List(const uint16_t &crcPolynomialCode)
{
    this->crcList.resize(256);

    for (int i = 0; i < this->crcList.size(); i++)
    {
        this->crcList[i] = this->CalculateCRC((uint8_t)i, crcPolynomialCode);
    }
}

vuprs::CRC8List::~CRC8List()
{
    this->crcList.clear();
}

uint8_t vuprs::CRC8List::CRCValue(const uint8_t &source)
{
    return this->crcList[source];
}

/* --------------------------------------------------------------------------------------------------------------- */
/* ------------------------------------------------ ADC Frame ---------------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

vuprs::ADCFrame::ADCFrame()
{
    this->adcData.resize(ADC_FRAME_WORD_LENGTH - 2);

    this->crcDataH.resize(ADC_FRAME_WORD_LENGTH - 2);
    this->crcDataL.resize(ADC_FRAME_WORD_LENGTH - 2);
}

vuprs::ADCFrame::~ADCFrame()
{
    this->adcData.clear();

    this->crcDataH.clear();
    this->crcDataL.clear();
}

bool vuprs::ADCFrame::CheckCRC(const int &channel)
{
    if (IS_ADC_CHANNEL(channel))
    {
        uint8_t adcDataH = (uint8_t)((this->adcData[channel] & 0xFF00) >> 8);
        uint8_t adcDataL = (uint8_t)((this->adcData[channel] & 0x00FF));

        if (globalCRCList.CRCValue(adcDataH) == this->crcDataH[channel] &&
            globalCRCList.CRCValue(adcDataL) == this->crcDataL[channel])
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        throw std::range_error(("Invalid ADC channel: " + std::to_string(channel)).c_str());
    }
}

void vuprs::ADCFrame::UpdateData(const int &index, const uint32_t &data)
{
    if (index < (ADC_FRAME_WORD_LENGTH - 2))
    {
        this->adcData[index] = (uint16_t)((data & 0xFFFF0000) >> 16);
        this->crcDataH[index] = (uint8_t)((data & 0x0000FF00) >> 8);
        this->crcDataL[index] = (uint8_t)((data & 0x000000FF));
    }
    else
    {
        throw std::range_error(("Invalid index: " + std::to_string(index)).c_str());
    }
}

uint16_t vuprs::ADCFrame::GetChannelValue(const int &channel)
{
    if (IS_ADC_CHANNEL(channel))
    {
        return this->adcData[channel];
    }
    else
    {
        throw std::range_error(("Invalid ADC channel: " + std::to_string(channel)).c_str());
    }
}
