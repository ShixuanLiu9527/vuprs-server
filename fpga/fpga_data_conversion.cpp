#include <fstream>
#include "fpga/fpga_data_conversion.h"
#include "fpga/fpga_data_type.h"
#include "misc/aligned_buffer.h"
#include "misc/file_processing.h"
#include "logger/check.h"

vuprs::SignalData::SignalData()
{
    this->_channel_name = vuprs::ADC_CHANNEL_ADDR_MAP;
}

void vuprs::SignalData::ToCSV(const std::string &output_file)
{
    PARAM_CHECK(!(this->_channel_data.empty() || this->_channel_name.empty()), "fpga", " No channel data to export.");
    std::string dir;
    vuprs::SplitFile(output_file, &dir, nullptr, nullptr);
    if (!vuprs::PathExist(dir))
    {
        vuprs::MakeDir(dir);
    }
    std::ofstream file(output_file);
    RUNTIME_CHECK(file.is_open(), "fpga", " Cannot open output file: " + output_file);
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
    PARAM_CHECK(buffer->is_allocated(), "fpga", " Buffer is empty.");
    const uint16_t *raw_buffer_data = buffer->as<const uint16_t>();
    ssize_t vector_size = static_cast<ssize_t>(buffer->size() / sizeof(uint16_t)), frame_parsed_size = 0;
    uint32_t header_ptr = 0, tailer_ptr = ADC_FRAME_HALF_WORD_SIZE - 2;
    adc_data->_channel_data.clear();
    adc_data->_channel_data.resize(ADC_CHANNEL_NUMBER);
    for (uint32_t i = 0; i < ADC_CHANNEL_NUMBER; i++)
        adc_data->_channel_data[i].reserve(vector_size / ADC_FRAME_HALF_WORD_SIZE + 1);
    while ((tailer_ptr + 1) < vector_size)
    {
        if ((header_ptr + 1) < vector_size && (tailer_ptr + 1) < vector_size)
        {
            uint16_t header_l = raw_buffer_data[header_ptr];
            uint16_t header_h = raw_buffer_data[header_ptr + 1];
            uint16_t tailer_l = raw_buffer_data[tailer_ptr];
            uint16_t tailer_h = raw_buffer_data[tailer_ptr + 1];
            RUNTIME_CHECK(IS_FRAME_HEADER(header_l, header_h) && IS_FRAME_TAILER(tailer_l, tailer_h), "fpga", " Error memory data.");
            /* Receive Data */
            for (uint32_t ch = 0; ch < ADC_CHANNEL_NUMBER; ch++)
            {
                adc_data->_channel_data[ch].emplace_back(
                    std::complex<double>(
                        vuprs::Q15__ADC_UINT16_TO_DOUBLE(raw_buffer_data[header_ptr + ch + 2], v_scale),
                        0.0));
            }
            frame_parsed_size++;
        }
        header_ptr += ADC_FRAME_HALF_WORD_SIZE;
        tailer_ptr += ADC_FRAME_HALF_WORD_SIZE;
    }
    /* - Rotate - */
    for (uint32_t ch = 0; ch < ADC_CHANNEL_NUMBER; ch++)
        vuprs::RotateCircularBuffer<std::complex<double>>(&adc_data->_channel_data[ch], point_pos_cbf);
    adc_data->fs = fs;
    adc_data->sampling_time = (1.0 / fs) * ((double)frame_parsed_size - 1.0);
    adc_data->signal_points = frame_parsed_size;
    return true;
}
