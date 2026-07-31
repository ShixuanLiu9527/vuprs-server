#include "fpga/fpga_data_conversion.h"
#include "logger/log_manager.h"

vuprs::SignalData::SignalData()
{
    this->_channel_name = vuprs::ADC_CHANNEL_ADDR_MAP;
}

bool vuprs::SignalData::contains(const std::string &channel_name) const
{
    auto it = this->CHANNEL_NAME__TO__CHANNEL_INDEX.find(channel_name);
    if (it != this->CHANNEL_NAME__TO__CHANNEL_INDEX.end())
    {
        return true;
    }
    else
    {
        return false;
    }
}

void vuprs::SignalData::GetChannelData(const std::string &channel_name, std::vector<std::complex<double>> *data) const
{
    auto it = this->CHANNEL_NAME__TO__CHANNEL_INDEX.find(channel_name);
    PARAM_CHECK(it != this->CHANNEL_NAME__TO__CHANNEL_INDEX.end(), "fpga", " in [SignalData::GetChannelData] Invalid channel name: " + channel_name);
    data->assign(this->_channel_data[it->second].begin(), this->_channel_data[it->second].end());
}

void vuprs::SignalData::_UpdataHashMap()
{
    PARAM_CHECK(this->_channel_data.size() == this->_channel_name.size(), "fpga", " in [SignalData::_UpdataHashMap] Unmatch channel & name size.");
    int data_channels = this->_channel_data.size();
    this->CHANNEL_NAME__TO__CHANNEL_INDEX.clear();
    for (int i = 0; i < data_channels; i++)
    {
        this->CHANNEL_NAME__TO__CHANNEL_INDEX.insert(std::make_pair(this->_channel_name[i], i));
    }
}

void vuprs::SignalData::DeleteChannelData(const std::string &channel_name)
{
    this->CHANNEL_NAME__TO__CHANNEL_INDEX.erase(channel_name);
}

void vuprs::SignalData::ToCSV(const std::string &output_file)
{
    this->_UpdataHashMap();
    PARAM_CHECK(!(this->_channel_data.empty() || this->_channel_name.empty()), "fpga", " in [SignalData::ToCSV] No channel data to export.");
    std::string dir;
    vuprs::SplitFile(output_file, &dir, nullptr, nullptr);
    if (!vuprs::PathExist(dir))
    {
        vuprs::MakeDir(dir);
    }
    std::ofstream file(output_file);
    RUNTIME_CHECK(file.is_open(), "fpga", " in [SignalData::ToCSV] Cannot open output file: " + output_file);
    uint64_t channel_number = this->_channel_data.size();
    /* First line: time, ch1, ch2, ..., chN */
    file << "time,";
    for (size_t i = 0; i < channel_number; ++i)
    {
        file << this->_channel_name[i];
        if (i != channel_number - 1)
        {
            file << ",";
        }
    }
    file << "\n";
    for (int point = 0; point < this->signal_points; ++point)
    {
        double time = (double)point / this->fs;
        file << time;
        file << ",";
        for (size_t channel = 0; channel < channel_number; ++channel)
        {
            file << this->_channel_data[channel][point].real();

            if (channel != channel_number - 1)
            {
                file << ",";
            }
        }
        file << "\n";
    }

    file.close();
}

bool vuprs::FPGACircularBuffer2Frames(vuprs::AlignedBufferDMA *buffer, vuprs::SignalData *adc_data, double fs, double v_scale, uint32_t point_pos_cbf)
{
    PARAM_CHECK(buffer->is_allocated(), "fpga", " in [vuprs::FPGACircularBuffer2Frames] Buffer is empty.");

    std::vector<uint16_t> raw_buffer_data_vector = buffer->to_vector<uint16_t>();
    ssize_t vector_size = raw_buffer_data_vector.size(), frame_parsed_size = 0;
    uint32_t header_ptr = 0, tailer_ptr = ADC_FRAME_HALF_WORD_SIZE - 2;
    adc_data->_channel_data.clear();
    adc_data->_channel_data.resize(ADC_CHANNEL_NUMBER);
    for (int i = 0; i < ADC_CHANNEL_NUMBER; i++)
    {
        adc_data->_channel_data[i].reserve(vector_size / ADC_FRAME_HALF_WORD_SIZE + 1);
    }
    while ((tailer_ptr + 1) < vector_size)
    {
        if ((header_ptr + 1) < vector_size && (tailer_ptr + 1) < vector_size)
        {
            uint16_t header_l = raw_buffer_data_vector[header_ptr];
            uint16_t header_h = raw_buffer_data_vector[header_ptr + 1];
            uint16_t tailer_l = raw_buffer_data_vector[tailer_ptr];
            uint16_t tailer_h = raw_buffer_data_vector[tailer_ptr + 1];
            RUNTIME_CHECK(IS_FRAME_HEADER(header_l, header_h) && IS_FRAME_TAILER(tailer_l, tailer_h), "fpga", " in [vuprs::FPGACircularBuffer2Frames] Error memory data.");
            /* Receive Data */
            for (int ch = 0; ch < ADC_CHANNEL_NUMBER; ch++)
            {
                adc_data->_channel_data[ch].emplace_back(
                    std::complex<double>(
                        vuprs::Q15__ADC_UINT16_TO_DOUBLE(raw_buffer_data_vector[header_ptr + ch + 2],
                                                         v_scale),
                        0.0));
            }
            frame_parsed_size++;
        }
        header_ptr += ADC_FRAME_HALF_WORD_SIZE;
        tailer_ptr += ADC_FRAME_HALF_WORD_SIZE;
    }

    for (int ch = 0; ch < ADC_CHANNEL_NUMBER; ch++)
    {
        vuprs::RotateCircularBuffer<std::complex<double>>(&adc_data->_channel_data[ch], point_pos_cbf);
    }
    adc_data->fs = fs;
    adc_data->sampling_time = (1.0 / fs) * ((double)frame_parsed_size - 1.0);
    adc_data->signal_points = frame_parsed_size;
    adc_data->_UpdataHashMap();

    return true;
}

bool vuprs::FPGAMemoryBuffer2Frames(vuprs::AlignedBufferDMA *buffer, std::vector<double> *beamforming_result, double fs)
{
    PARAM_CHECK(buffer->is_allocated(), "fpga", " in [vuprs::FPGAMemoryBuffer2Frames] Buffer is empty.");
    std::vector<uint32_t> raw_buffer_data_vector = buffer->to_vector<uint32_t>();
    ssize_t vector_size = raw_buffer_data_vector.size();
    beamforming_result->clear();
    beamforming_result->reserve(vector_size);
    for (int i = 0; i < vector_size; i++)
    {
        beamforming_result->push_back(vuprs::Q16__UINT32_TO_DOUBLE(raw_buffer_data_vector[i])); /* Q16.16 */
    }
    return true;
}
