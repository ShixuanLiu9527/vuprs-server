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

void vuprs::HybridBeamformerConfigMask::Reset()
{
    this->m_fs = false;
    this->m_bf_target__alt = false;
    this->m_bf_target__az = false;
    this->m_bf_waveVelocity = false;
    this->m_bf_freq__lower = false;
    this->m_bf_freq__upper = false;
    this->m_bf_cov_snapshots_window_size = false;
    this->m_bf_cov_freq_average_index = false;
    this->m_dma__bufferSize = false;
    this->m_dma__bufferCount = false;
    this->m_queue__circular_buffer_queue_size_max = false;
    this->m_queue__result_queue_size_max = false;
}

bool vuprs::CheckCollaborationBeamformerConfigValid(vuprs::FPGAController *controller, const vuprs::HybridBeamformerConfig &config)
{
    bool retval = true;
    PARAM_CHECK(controller->ConfigDone(), "hybrid_bf", " in [CheckCollaborationBeamformerConfigValid] FPGA config not complete.");
    retval &= (config.fs > 0 && config.fs < controller->dev__adc_controller.MaxSamplingFrequency());
    retval &= (config.bf_freq__lower < config.fs / 2.0);
    retval &= (config.bf_freq__upper < config.fs / 2.0);
    retval &= (config.bf_freq__lower < config.bf_freq__upper);
    retval &= (config.bf_cov_freq_average_index < 1.0);
    retval &= (config.dma__buffer_size % DMA_BUFFER_ALIGNMENT_1_WORD == 0);
    retval &= ((config.dma__buffer_size * config.dma__buffer_count) < controller->mem__ddr.MaxSizeBytes());
    return retval;
}
