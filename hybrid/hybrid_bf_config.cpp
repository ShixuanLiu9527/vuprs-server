#include "config.h"
#include "hybrid/hybrid_bf_config.h"
#include "logger/check.h"

void vuprs::HybridBeamformerConfig::SetDefault()
{
    this->fs = 10000.0;                               /* sampling frequency (unit: Hz) */
    this->bf_target__alt = 90.0;                      /* altitude (unit: degree) beam former pointing target */
    this->bf_target__az = 0.0;                        /* azimuth (unit: degree) beam former pointing target */
    this->bf_wave_velocity = 346.0;                   /* wave velocity of sound */
    this->bf_freq__lower = 500.0;                     /* lower boundary of beam former work frequency (unit: Hz) */
    this->bf_freq__upper = 3000.0;                    /* upper boundary of beam former work frequency (unit: Hz) */
    this->bf_cov_snapshots_window_size = 200;         /* Snapshots window size (to fit covariance matrix) >= 200 */
    this->bf_cov_freq_average_index = 0.8;            /* frequency average index (to fit covariance matrix) */
    this->dma__buffer_size = 32768;                   /* [internal param] AXI DMA descriptor buffer size */
    this->dma__buffer_count = 20;                     /* [internal param] AXI DMA descriptor buffer count */
    this->queue__circular_buffer_queue_size_max = 10; /* [internal param] depth of multi-channel data queue */
    this->queue__result_queue_size_max = 10;          /* [internal param] depth of result data queue */
}

bool vuprs::HybridBeamformerConfig::FromJson(const std::string &json_filename)
{
    std::ifstream f;
    f.open(json_filename);
    RUNTIME_CHECK(f.is_open(), "hybrid", "Cannot open file: " + json_filename);
    nlohmann::json json_data;
    try
    {
        f >> json_data;
    }
    catch (const std::exception &e)
    {
        RUNTIME_CHECK(false, "hybrid", "Failed to data from: " + json_filename);
    }
    /* Parse each field, set the corresponding mask if found. */
    this->SetDefault();
    this->ResetMask(false);
    if (json_data.contains("fs"))
    {
        vuprs::__JsonStringParseFLOAT<double>(&this->fs, json_data["fs"], "value", true);
        this->mask.m_fs = true;
    }
    if (json_data.contains("bf_target__alt"))
    {
        vuprs::__JsonStringParseFLOAT<double>(&this->bf_target__alt, json_data["bf_target__alt"], "value", true);
        this->mask.m_bf_target__alt = true;
    }
    if (json_data.contains("bf_target__az"))
    {
        vuprs::__JsonStringParseFLOAT<double>(&this->bf_target__az, json_data["bf_target__az"], "value", true);
        this->mask.m_bf_target__az = true;
    }
    if (json_data.contains("bf_wave_velocity"))
    {
        vuprs::__JsonStringParseFLOAT<double>(&this->bf_wave_velocity, json_data["bf_wave_velocity"], "value", true);
        this->mask.m_bf_waveVelocity = true;
    }
    if (json_data.contains("bf_freq__lower"))
    {
        vuprs::__JsonStringParseFLOAT<double>(&this->bf_freq__lower, json_data["bf_freq__lower"], "value", true);
        this->mask.m_bf_freq__lower = true;
    }
    if (json_data.contains("bf_freq__upper"))
    {
        vuprs::__JsonStringParseFLOAT<double>(&this->bf_freq__upper, json_data["bf_freq__upper"], "value", true);
        this->mask.m_bf_freq__upper = true;
    }
    if (json_data.contains("bf_cov_snapshots_window_size"))
    {
        vuprs::__JsonStringParseINT<int>(&this->bf_cov_snapshots_window_size, json_data["bf_cov_snapshots_window_size"], "value", true);
        this->mask.m_bf_cov_snapshots_window_size = true;
    }
    if (json_data.contains("bf_cov_freq_average_index"))
    {
        vuprs::__JsonStringParseFLOAT<double>(&this->bf_cov_freq_average_index, json_data["bf_cov_freq_average_index"], "value", true);
        this->mask.m_bf_cov_freq_average_index = true;
    }
    if (json_data.contains("dma__buffer_size"))
    {
        vuprs::__JsonStringParseINT<uint32_t>(&this->dma__buffer_size, json_data["dma__buffer_size"], "value", true);
        this->mask.m_dma__bufferSize = true;
    }
    if (json_data.contains("dma__buffer_count"))
    {
        vuprs::__JsonStringParseINT<uint32_t>(&this->dma__buffer_count, json_data["dma__buffer_count"], "value", true);
        this->mask.m_dma__bufferCount = true;
    }
    if (json_data.contains("queue__circular_buffer_queue_size_max"))
    {
        vuprs::__JsonStringParseINT<uint32_t>(&this->queue__circular_buffer_queue_size_max, json_data["queue__circular_buffer_queue_size_max"], "value", true);
        this->mask.m_queue__circular_buffer_queue_size_max = true;
    }
    if (json_data.contains("queue__result_queue_size_max"))
    {
        vuprs::__JsonStringParseINT<uint32_t>(&this->queue__result_queue_size_max, json_data["queue__result_queue_size_max"], "value", true);
        this->mask.m_queue__result_queue_size_max = true;
    }
    return true;
}

void vuprs::HybridBeamformerConfigMask::Reset(bool val)
{
    this->m_fs = val;
    this->m_bf_target__alt = val;
    this->m_bf_target__az = val;
    this->m_bf_waveVelocity = val;
    this->m_bf_freq__lower = val;
    this->m_bf_freq__upper = val;
    this->m_bf_cov_snapshots_window_size = val;
    this->m_bf_cov_freq_average_index = val;
    this->m_dma__bufferSize = val;
    this->m_dma__bufferCount = val;
    this->m_queue__circular_buffer_queue_size_max = val;
    this->m_queue__result_queue_size_max = val;
}
