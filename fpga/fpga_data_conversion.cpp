#include "fpga_data_conversion.h"

vuprs::SignalData::SignalData()
{
    this->_channelName = vuprs::ADC_CHANNEL_ADDR_MAP;
}

bool vuprs::SignalData::contains(const std::string &channelName) const
{
    auto it = this->CHANNEL_NAME__TO__CHANNEL_INDEX.find(channelName);
    if (it != this->CHANNEL_NAME__TO__CHANNEL_INDEX.end()) {return true;}
    else {return false;}
}

void vuprs::SignalData::GetChannelData(const std::string &channelName, std::vector<std::complex<double>> *data) const
{
    auto it = this->CHANNEL_NAME__TO__CHANNEL_INDEX.find(channelName);

    if (it != this->CHANNEL_NAME__TO__CHANNEL_INDEX.end()) 
    {
        data->assign(this->_channelData[it->second].begin(), this->_channelData[it->second].end());
    }
    else
    {
        throw std::runtime_error("in [SignalData::GetChannelData] Invalid channel name: " + channelName);
    }
}

void vuprs::SignalData::_UpdataHashMap()
{
    if (this->_channelData.size() != this->_channelName.size()) 
    {
        throw std::runtime_error("in [SignalData::_UpdataHashMap] Unmatch channel & name size.");
    }
    int dataChannels = this->_channelData.size();
    this->CHANNEL_NAME__TO__CHANNEL_INDEX.clear();
    for (int i = 0; i < dataChannels; i++)
    {
        this->CHANNEL_NAME__TO__CHANNEL_INDEX.insert(std::make_pair(this->_channelName[i], i));
    }
}

void vuprs::SignalData::DeleteChannelData(const std::string &channelName)
{
    this->CHANNEL_NAME__TO__CHANNEL_INDEX.erase(channelName);
}

void vuprs::SignalData::ToCSV(const std::string &outputFile)
{
    this->_UpdataHashMap();
    
    if (this->_channelData.empty() || this->_channelName.empty()) 
    {
        throw std::runtime_error("in [SignalData::ToCSV] No channel data to export.");
    }

    std::string dir;
    vuprs::SplitFile(outputFile, &dir, nullptr, nullptr);

    if (!vuprs::PathExist(dir))
    {
        vuprs::MakeDir(dir);
    }
    
    std::ofstream file(outputFile);
    if (!file.is_open())
    {
        throw std::runtime_error("in [SignalData::ToCSV] Cannot open output file: " + outputFile);
    }

    uint64_t channelNumber = this->_channelData.size();
    
    /* First line: time, ch1, ch2, ..., chN */

    file << "time,";
    for (size_t i = 0; i < channelNumber; ++i)
    {
        file << this->_channelName[i];
        if (i != channelNumber - 1)
        {
            file << ",";
        }
    }
    file << "\n";
    
    for (int point = 0; point < this->signalPoints; ++point) 
    {
        double time = (double)point / this->samplingFrequency;
        file << time; file << ",";
        for (size_t channel = 0; channel < channelNumber; ++channel) 
        {
            file << this->_channelData[channel][point].real();
            
            if (channel != channelNumber - 1) 
            {
                file << ",";
            }
        }
        file << "\n";
    }
    
    file.close();
}

bool vuprs::FPGACircularBuffer2Frames(vuprs::AlignedBufferDMA *buffer, vuprs::SignalData *adcData, double fs, double v_scale, uint32_t pointPosCBF)
{
    if (!buffer->is_allocated())
    {
        throw std::runtime_error("in [vuprs::FPGACircularBuffer2Frames] Buffer is empty.");
    }

    std::vector<uint16_t> rawBufferDataVector = buffer->to_vector<uint16_t>();
    ssize_t vectorSize = rawBufferDataVector.size(), frameParsedSize = 0;
    uint32_t headerPointer = 0, tailerPointer = ADC_FRAME_HALF_WORD_SIZE - 2;

    adcData->_channelData.clear();
    adcData->_channelData.resize(ADC_CHANNEL_NUMBER);
    
    for (int i = 0; i < ADC_CHANNEL_NUMBER; i++)
    {
        adcData->_channelData[i].reserve(vectorSize / ADC_FRAME_HALF_WORD_SIZE + 1);
    }

    while ((tailerPointer + 1) < vectorSize)
    {
        if ((headerPointer + 1) < vectorSize && (tailerPointer + 1) < vectorSize)
        {
            uint16_t header_l = rawBufferDataVector[headerPointer];
            uint16_t header_h = rawBufferDataVector[headerPointer+1];
            uint16_t tailer_l = rawBufferDataVector[tailerPointer];
            uint16_t tailer_h = rawBufferDataVector[tailerPointer+1];
            
            if (IS_FRAME_HEADER(header_l, header_h) && IS_FRAME_TAILER(tailer_l, tailer_h))
            {
                /* Receive Data */
                for (int ch = 0; ch < ADC_CHANNEL_NUMBER; ch++)
                {
                    adcData->_channelData[ch].emplace_back(
                        std::complex<double>(vuprs::Q15__ADC_UINT16_TO_DOUBLE(
                            rawBufferDataVector[headerPointer + ch + 2], v_scale
                        ), 0.0)
                    );
                }
                frameParsedSize++;
            }
            else
            {
                throw std::runtime_error("in [vuprs::FPGACircularBuffer2Frames] Error memory data.");
            }
        }
        headerPointer += ADC_FRAME_HALF_WORD_SIZE;
        tailerPointer += ADC_FRAME_HALF_WORD_SIZE;
    }

    for (int ch = 0; ch < ADC_CHANNEL_NUMBER; ch++)
    {
        vuprs::RotateCircularBuffer<std::complex<double>>(&adcData->_channelData[ch], pointPosCBF);
    }

    adcData->samplingFrequency = fs;
    adcData->samplingTime = (1.0 / fs) * ((double)frameParsedSize - 1.0);
    adcData->signalPoints = frameParsedSize;

    adcData->_UpdataHashMap();

    return true;
}

bool vuprs::FPGAMemoryBuffer2Frames(vuprs::AlignedBufferDMA *buffer, std::vector<double> *beamformingResult, double samplingFrequency)
{
    if(!buffer->is_allocated())
    {
        throw std::runtime_error("in [vuprs::FPGAMemoryBuffer2Frames] Buffer is empty.");
    }

    std::vector<uint32_t> rawBufferDataVector = buffer->to_vector<uint32_t>();
    ssize_t vectorSize = rawBufferDataVector.size();

    beamformingResult->clear();
    beamformingResult->reserve(vectorSize);

    for (int i = 0; i < vectorSize; i++)
    {
        beamformingResult->push_back(vuprs::Q16__UINT32_TO_DOUBLE(rawBufferDataVector[i]));  /* Q16.16 */
    }

    return true;
}
