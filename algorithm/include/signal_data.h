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

            /**
             * @brief Indicate whether this channel is included.
             * 
             * @param channelName channel name, optional:
             *                    CH-A-1: channel-A1;
             *                    CH-A-2: channel-A2;
             *                    CH-A-3: channel-A3;
             *                    CH-A-4: channel-A4;
             *                    CH-A-5: channel-A5;
             *                    CH-A-6: channel-A6;
             *                    CH-A-7: channel-A7;
             *                    CH-A-8: channel-A8;
             *                    CH-B-1: channel-B1;
             *                    CH-B-2: channel-B2;
             *                    CH-B-3: channel-B3;
             *                    CH-B-4: channel-B4;
             *                    CH-B-5: channel-B5;
             *                    CH-B-6: channel-B6;
             *                    CH-B-7: channel-B7;
             *                    CH-B-8: channel-B8;
             * 
             * @retval true: this channel is included.
             * @retval false: this channel is not included.
             */
            bool contains(const std::string &channelName) const;

            /**
             * @brief Get channel data.
             * 
             * @param channelName channel name.
             * @param data output data.
             * 
             * @throw std::runtime_error
             */
            void GetChannelData(const std::string &channelName, std::vector<std::complex<double>> *data) const;

            /**
             * @brief Delete channel data.
             * 
             * @param channelName channel name.
             */
            void DeleteChannelData(const std::string &channelName);

            /**
             * @brief Save all data to CSV file.
             * 
             * @param outputFile output file name.
             * 
             * @throw std::runtime_error
             */
            void ToCSV(const std::string &outputFile);

            void _UpdataHashMap();
    };
}

#endif