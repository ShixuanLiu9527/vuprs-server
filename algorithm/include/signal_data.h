#ifndef SIGNAL_DATA_H
#define SIGNAL_DATA_H

#include <vector>
#include <fstream>
#include <stdint.h>
#include <complex>
#include <unordered_map>

#include "aligned_buffer.h"

namespace vuprs
{
    class SignalData
    {
        private:

            std::unordered_map<std::string, uint8_t> CHANNEL_NAME__TO__CHANNEL_INDEX;

        public:

            std::vector<std::vector<std::complex<double>>> _channelData;
            std::vector<std::string> _channelName;

            double samplingFrequency = 0.0, samplingTime = 0.0;
            int signalPoints = 0;  /* signal data point number for one channel */

            bool contains(const std::string &channelName) const;
            void GetChannelData(const std::string &channelName, std::vector<std::complex<double>> *data) const;
            void DeleteChannelData(const std::string &channelName);

            void ToCSV(const std::string &outputFile);

            void _UpdataHashMap();
    };
}

#endif