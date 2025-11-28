#include "signal_data.h"

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
    
    for (size_t i = 0; i < this->_channelName.size(); ++i) 
    {
        file << this->_channelName[i];
        if (i != this->_channelName.size() - 1) 
        {
            file << ",";
        }
    }
    file << "\n";
    
    for (int point = 0; point < this->signalPoints; ++point) 
    {
        for (size_t channel = 0; channel < this->_channelData.size(); ++channel) 
        {
            file << this->_channelData[channel][point].real();
            
            if (channel != this->_channelData.size() - 1) 
            {
                file << ",";
            }
        }
        file << "\n";
    }
    
    file.close();
}
