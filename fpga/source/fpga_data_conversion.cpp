#include "fpga_data_conversion.h"

bool vuprs::SignalData::contains(const std::string &channelName) const
{
    auto it = this->CHANNEL_NAME__TO__CHANNEL_INDEX.find(channelName);
    if (it != this->CHANNEL_NAME__TO__CHANNEL_INDEX.end()) {return true;}
    else {return false;}
}

void vuprs::SignalData::GetChannelData(const std::string &channelName, std::vector<std::complex<double>> *data) const
{
    auto it = this->CHANNEL_NAME__TO__CHANNEL_INDEX.find(channelName);
    if (it != this->CHANNEL_NAME__TO__CHANNEL_INDEX.end()) {*data = this->_channelData[it->second];}
    else {throw std::runtime_error("Invalid channel name: " + channelName);}
}

void vuprs::SignalData::_UpdataHashMap()
{
    if (this->_channelData.size() != this->_channelName.size()) {throw std::runtime_error("Unmatch channel & name size.");}
    int dataChannels = this->_channelData.size();
    this->CHANNEL_NAME__TO__CHANNEL_INDEX.clear();
    for (int i = 0; i < dataChannels; i++) {this->CHANNEL_NAME__TO__CHANNEL_INDEX.insert(std::make_pair(this->_channelName[i], i));}
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
        throw std::runtime_error("No channel data to export.");
    }
    
    std::ofstream file(outputFile);
    if (!file.is_open())
    {
        throw std::runtime_error("Cannot open output file: " + outputFile);
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

bool vuprs::FPGACircularBuffer2Frames(vuprs::AlignedBufferDMA *buffer, vuprs::SignalData *adcData, double samplingFrequency)
{
    if (!buffer->is_allocated())
    {
        throw std::runtime_error("Buffer is empty.");
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

    adcData->_channelName = {
        ADC_CHANNEL_NAME__A_1, ADC_CHANNEL_NAME__A_2, ADC_CHANNEL_NAME__A_3, ADC_CHANNEL_NAME__A_4,
        ADC_CHANNEL_NAME__A_5, ADC_CHANNEL_NAME__A_6, ADC_CHANNEL_NAME__A_7, ADC_CHANNEL_NAME__A_8,
        ADC_CHANNEL_NAME__B_1, ADC_CHANNEL_NAME__B_2, ADC_CHANNEL_NAME__B_3, ADC_CHANNEL_NAME__B_4,
        ADC_CHANNEL_NAME__B_5, ADC_CHANNEL_NAME__B_6, ADC_CHANNEL_NAME__B_7, ADC_CHANNEL_NAME__B_8
    };

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
                    int16_t currentChannelData = static_cast<int16_t>(rawBufferDataVector[headerPointer + ch + 2]);
                    std::complex<double> complexChannelData((double)currentChannelData / 32768.0, 0.0);
                    adcData->_channelData[ch].push_back(complexChannelData);
                }

                frameParsedSize++;
            }
            else
            {
                throw std::runtime_error("Error memory data.");
            }
        }
        headerPointer += ADC_FRAME_HALF_WORD_SIZE;
        tailerPointer += ADC_FRAME_HALF_WORD_SIZE;
    }

    adcData->samplingFrequency = samplingFrequency;
    adcData->samplingTime = (1.0 / samplingFrequency) * ((double)frameParsedSize - 1.0);
    adcData->signalPoints = frameParsedSize;

    return true;
}

bool vuprs::FPGAMemoryBuffer2Frames(vuprs::AlignedBufferDMA *buffer, std::vector<double> *beamformingResult, double samplingFrequency)
{
    if(!buffer->is_allocated())
    {
        throw std::runtime_error("Buffer is empty.");
    }
    std::vector<uint32_t> rawBufferDataVector = buffer->to_vector<uint32_t>();
    ssize_t vectorSize = rawBufferDataVector.size();
    beamformingResult->clear();
    beamformingResult->reserve(vectorSize);
    for (int i = 0; i < vectorSize; i++)
    {
        int32_t currentChannelData = static_cast<int32_t>(rawBufferDataVector[i]);
        beamformingResult->push_back((double)currentChannelData / 65536.0);  /* Q16.16 */
    }

    return true;
}
