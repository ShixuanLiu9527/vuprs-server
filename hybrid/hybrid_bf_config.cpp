#include <fstream>
#include "config.h"
#include "hybrid/hybrid_bf_config.h"
#include "misc/string_parse.h"
#include "logger/check.h"

void vuprs::HybridBeamformerConfig::SetDefault()
{
    this->fs = 50000.0;                        /* sampling frequency (unit: Hz) */
    this->bf_target__alt = 90.0;               /* altitude (unit: degree) beam former pointing target */
    this->bf_target__az = 0.0;                 /* azimuth (unit: degree) beam former pointing target */
    this->bf_wave_velocity = 346.0;            /* wave velocity of sound */
    this->bf_freq__lo = 500.0;                 /* lower boundary of beam former work frequency (unit: Hz) */
    this->bf_freq__hi = 3000.0;                /* upper boundary of beam former work frequency (unit: Hz) */
    this->bf_cov_snapshots_window_size = 30.0; /* Snapshots window size (to fit covariance matrix) */
    this->bf_cov_freq_average_index = 0.8;     /* frequency average index (to fit covariance matrix) */
    this->dma__buffer_points = 8192;           /* Sampling points for each AXI DMA buffer */
    this->dma__buffer_count = 20;              /* AXI DMA descriptor buffer count */
    this->queue__cbuf_queue_size = 4;          /* [internal param] depth of multi-channel data queue */
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
        vuprs::__JsonStringParseFLOAT<double>(&this->fs,
                                              json_data["fs"], "value", true);
        this->mask.m_fs = true;
    }
    if (json_data.contains("bf_target__alt"))
    {
        vuprs::__JsonStringParseFLOAT<double>(&this->bf_target__alt,
                                              json_data["bf_target__alt"], "value", true);
        this->mask.m_bf_target__alt = true;
    }
    if (json_data.contains("bf_target__az"))
    {
        vuprs::__JsonStringParseFLOAT<double>(&this->bf_target__az,
                                              json_data["bf_target__az"], "value", true);
        this->mask.m_bf_target__az = true;
    }
    if (json_data.contains("bf_wave_velocity"))
    {
        vuprs::__JsonStringParseFLOAT<double>(&this->bf_wave_velocity,
                                              json_data["bf_wave_velocity"], "value", true);
        this->mask.m_bf_wave_velocity = true;
    }
    if (json_data.contains("bf_freq__lo"))
    {
        vuprs::__JsonStringParseFLOAT<double>(&this->bf_freq__lo,
                                              json_data["bf_freq__lo"], "value", true);
        this->mask.m_bf_freq__lo = true;
    }
    if (json_data.contains("bf_freq__hi"))
    {
        vuprs::__JsonStringParseFLOAT<double>(&this->bf_freq__hi,
                                              json_data["bf_freq__hi"], "value", true);
        this->mask.m_bf_freq__hi = true;
    }
    if (json_data.contains("bf_cov_snapshots_window_size"))
    {
        vuprs::__JsonStringParseFLOAT<double>(&this->bf_cov_snapshots_window_size,
                                              json_data["bf_cov_snapshots_window_size"], "value", true);
        this->mask.m_bf_cov_snapshots_window_size = true;
    }
    if (json_data.contains("bf_cov_freq_average_index"))
    {
        vuprs::__JsonStringParseFLOAT<double>(&this->bf_cov_freq_average_index,
                                              json_data["bf_cov_freq_average_index"], "value", true);
        this->mask.m_bf_cov_freq_average_index = true;
    }
    if (json_data.contains("dma__buffer_points"))
    {
        vuprs::__JsonStringParseINT<uint32_t>(&this->dma__buffer_points,
                                              json_data["dma__buffer_points"], "value", true);
        this->mask.m_dma__buffer_points = true;
    }
    if (json_data.contains("dma__buffer_count"))
    {
        vuprs::__JsonStringParseINT<uint32_t>(&this->dma__buffer_count,
                                              json_data["dma__buffer_count"], "value", true);
        this->mask.m_dma__buffer_count = true;
    }
    if (json_data.contains("queue__cbuf_queue_size"))
    {
        vuprs::__JsonStringParseINT<uint32_t>(&this->queue__cbuf_queue_size,
                                              json_data["queue__cbuf_queue_size"], "value", true);
        this->mask.m_queue__cbuf_queue_size = true;
    }
    return true;
}

void vuprs::HybridBeamformerConfigMask::Reset(bool val)
{
    this->m_fs = val;
    this->m_bf_target__alt = val;
    this->m_bf_target__az = val;
    this->m_bf_wave_velocity = val;
    this->m_bf_freq__lo = val;
    this->m_bf_freq__hi = val;
    this->m_bf_cov_snapshots_window_size = val;
    this->m_bf_cov_freq_average_index = val;
    this->m_dma__buffer_points = val;
    this->m_dma__buffer_count = val;
    this->m_queue__cbuf_queue_size = val;
}

void vuprs::ScanningConfigMask::Reset(bool val)
{
    this->m_alt_min = val;
    this->m_points_in_hemisphere = val;
}
