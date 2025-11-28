#include "fpga_data_parse.h"

static const std::unordered_map<uint8_t, std::string> ADC_CHANNEL__TO__ADC_CHANNEL_NAME = \
{
    {ADC_CHANNEL__A_1, ADC_CHANNEL_NAME__A_1},
    {ADC_CHANNEL__A_2, ADC_CHANNEL_NAME__A_2},
    {ADC_CHANNEL__A_3, ADC_CHANNEL_NAME__A_3},
    {ADC_CHANNEL__A_4, ADC_CHANNEL_NAME__A_4},
    {ADC_CHANNEL__A_5, ADC_CHANNEL_NAME__A_5},
    {ADC_CHANNEL__A_6, ADC_CHANNEL_NAME__A_6},
    {ADC_CHANNEL__A_7, ADC_CHANNEL_NAME__A_7},
    {ADC_CHANNEL__A_8, ADC_CHANNEL_NAME__A_8},

    {ADC_CHANNEL__B_1, ADC_CHANNEL_NAME__B_1},
    {ADC_CHANNEL__B_2, ADC_CHANNEL_NAME__B_2},
    {ADC_CHANNEL__B_3, ADC_CHANNEL_NAME__B_3},
    {ADC_CHANNEL__B_4, ADC_CHANNEL_NAME__B_4},
    {ADC_CHANNEL__B_5, ADC_CHANNEL_NAME__B_5},
    {ADC_CHANNEL__B_6, ADC_CHANNEL_NAME__B_6},
    {ADC_CHANNEL__B_7, ADC_CHANNEL_NAME__B_7},
    {ADC_CHANNEL__B_8, ADC_CHANNEL_NAME__B_8}
};  /* ADC channel index to channel name */

static const std::unordered_map<uint8_t, uint8_t> STORAGE_POSITION__TO__ADC_CHANNEL = \
{
    {ADC_CHANNEL_POSITION__A_1, ADC_CHANNEL__A_1},
    {ADC_CHANNEL_POSITION__A_2, ADC_CHANNEL__A_2},
    {ADC_CHANNEL_POSITION__A_3, ADC_CHANNEL__A_3},
    {ADC_CHANNEL_POSITION__A_4, ADC_CHANNEL__A_4},
    {ADC_CHANNEL_POSITION__A_5, ADC_CHANNEL__A_5},
    {ADC_CHANNEL_POSITION__A_6, ADC_CHANNEL__A_6},
    {ADC_CHANNEL_POSITION__A_7, ADC_CHANNEL__A_7},
    {ADC_CHANNEL_POSITION__A_8, ADC_CHANNEL__A_8},

    {ADC_CHANNEL_POSITION__B_1, ADC_CHANNEL__B_1},
    {ADC_CHANNEL_POSITION__B_2, ADC_CHANNEL__B_2},
    {ADC_CHANNEL_POSITION__B_3, ADC_CHANNEL__B_3},
    {ADC_CHANNEL_POSITION__B_4, ADC_CHANNEL__B_4},
    {ADC_CHANNEL_POSITION__B_5, ADC_CHANNEL__B_5},
    {ADC_CHANNEL_POSITION__B_6, ADC_CHANNEL__B_6},
    {ADC_CHANNEL_POSITION__B_7, ADC_CHANNEL__B_7},
    {ADC_CHANNEL_POSITION__B_8, ADC_CHANNEL__B_8}
};  /* Storage position to channel index */

const double LSB_VALUE = pow(2, ADC_DATAWIDTH) / 2.0;

vuprs::CRC8List globalCRCList(CRC8_POLYNOMIAL_CDMA2000);

bool vuprs::BufferData2ADCChannels(const vuprs::AlignedBufferServer *buffer, const vuprs::FPGAhardwareConfigADC &adcFeatures, double samplingFrequency, vuprs::SignalData *adcData)
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

    adcData->_channelData.clear();
    adcData->_channelData.resize(ADC_CHANNELS);

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

    /* Get frame data */

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
                        oneADCFrame.InputPositionData(i, originData[dataHeaderPointer + i]);
                    }
                    adcFrames.push_back(oneADCFrame);
                }
                else  /* Stop */
                {
                    break;
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

    if (adcFrames.size() <= 0)
    {
        return false;
    }

    /* Channel name */

    adcData->_channelName.resize(ADC_CHANNELS);

    for (int position = 0; position < ADC_CHANNELS; position++)
    {
        auto it1 = STORAGE_POSITION__TO__ADC_CHANNEL.find(position);
        if (it1 != STORAGE_POSITION__TO__ADC_CHANNEL.end())
        {
            uint8_t channel = it1->second;
            auto it2 = ADC_CHANNEL__TO__ADC_CHANNEL_NAME.find(channel);
            if (it2 != ADC_CHANNEL__TO__ADC_CHANNEL_NAME.end())
            {
                adcData->_channelName[position] = it2->second;
            }
            else
            {
                throw std::runtime_error("Invalid channel: " + std::to_string(channel));
            }
        }
        else
        {
            throw std::runtime_error("Invalid storage position: " + std::to_string(position));
        }
    }

    /* Calculate voltage */

    adcFrameElements = adcFrames.size();
    if (adcFrameElements % 2 != 0) adcFrameElements--;

    for (int i = 0; i < ADC_CHANNELS; i++)
    {
        adcData->_channelData[i].resize(adcFrameElements);  /* Resize each channel */
    }

    for (uint64_t i = 0; i < adcFrameElements; i++)
    {
        for (uint64_t position = 0; position < ADC_CHANNELS; position++)  /* storage position */
        {
            if (adcFrames[i].CheckCRC(position))
            {
                signedValue = static_cast<int16_t>(adcFrames[i].GetPositionValue(position));
                adcData->_channelData[position][i] = static_cast<double>(signedValue) * adcFeatures.adcVoltageRangeRadius / LSB_VALUE;
            }
            else
            {
                adcData->_channelData[position][i] = adcFeatures.adcVoltageRangeRadius;
            }
        }
    }

    adcData->_UpdataHashMap();
    adcData->samplingFrequency = samplingFrequency;
    adcData->samplingTime = (adcFrameElements - 1.0) / samplingFrequency;
    adcData->signalPoints = adcFrameElements;

    return true;
}

/* --------------------------------------------------------------------------------------------------------------- */
/* ------------------------------------------------- CRC List ---------------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

uint8_t vuprs::CRC8List::CalculateCRC(uint8_t source, uint16_t crcPolynomialCode)
{
    uint8_t crc;
    uint8_t CRC8_CDMA2000 = crcPolynomialCode & (0xFF);
    crc = source;
    
    for (int i = 0; i < 8; i++) 
    {
        if(crc & 0x80)
        {
            crc = (crc << 1) ^ crcPolynomialCode;
        }
        else
        {
            crc = (crc << 1);
        }
    }

    return crc;
}

vuprs::CRC8List::CRC8List(uint16_t crcPolynomialCode)
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

uint8_t vuprs::CRC8List::CRCValue(uint8_t source)
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

bool vuprs::ADCFrame::CheckCRC(int storagePosition)
{
    if (IS_ADC_CHANNEL(storagePosition))
    {
        uint8_t adcDataH = (uint8_t)((this->adcData[storagePosition] & 0xFF00) >> 8);
        uint8_t adcDataL = (uint8_t)((this->adcData[storagePosition] & 0x00FF));

        if (globalCRCList.CRCValue(adcDataH) == this->crcDataH[storagePosition] &&
            globalCRCList.CRCValue(adcDataL) == this->crcDataL[storagePosition])
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
        throw std::range_error(("Invalid ADC channel: " + std::to_string(storagePosition)).c_str());
    }
}

void vuprs::ADCFrame::InputPositionData(int storagePosition, uint32_t data)
{
    if (storagePosition < (ADC_FRAME_WORD_LENGTH - 2))
    {
        this->adcData[storagePosition] = (uint16_t)((data & 0xFFFF0000) >> 16);
        this->crcDataH[storagePosition] = (uint8_t)((data & 0x0000FF00) >> 8);
        this->crcDataL[storagePosition] = (uint8_t)((data & 0x000000FF));
    }
    else
    {
        throw std::range_error(("Invalid index: " + std::to_string(storagePosition)).c_str());
    }
}

uint16_t vuprs::ADCFrame::GetPositionValue(int storagePosition)
{
    if (IS_ADC_CHANNEL(storagePosition))
    {
        return this->adcData[storagePosition];
    }
    else
    {
        throw std::range_error(("Invalid ADC channel: " + std::to_string(storagePosition)).c_str());
    }
}
