#include "collaboration_configs.h"

void vuprs::CollaborationBeamformerConfig::SetDefault()
{
    this->fs = 10000.0;  /* sampling frequency (unit: Hz) */
    this->bf_target__alt = 90.0;  /* altitude (unit: degree) beam former pointing target */
    this->bf_target__az = 0.0;  /* azimuth (unit: degree) beam former pointing target */
    this->bf_waveVelocity = 346.0;
    this->bf_freq__lower = 100.0;  /* lower boundary of beam former work frequency (unit: Hz) */
    this->bf_freq__upper = 5000.0;  /* upper boundary of beam former work frequency (unit: Hz) */
    this->bf_cov_snapshotsWindowSize = 50;  /* Snapshots window size (to fit covariance matrix) */
    this->bf_cov_freqAverageIndex = 0.8;  /* frequency average index (to fit covariance matrix) */
    this->dma__bufferSize = 32768;  /* AXI DMA descriptor buffer size */
    this->dma__bufferCount = 20;  /* AXI DMA descriptor buffer count */
    this->queue__circularBufferQueueSizeMAX = 10;
    this->queue__resultQueueSizeMAX = 10;
}

void vuprs::CollaborationBeamformerConfigMask::Reset()
{
    this->m_fs = false;
    this->m_bf_target__alt = false;
    this->m_bf_target__az = false;
    this->m_bf_waveVelocity = false;
    this->m_bf_freq__lower = false;
    this->m_bf_freq__upper = false;
    this->m_bf_cov_snapshotsWindowSize = false;
    this->m_bf_cov_freqAverageIndex = false;
    this->m_dma__bufferSize = false;
    this->m_dma__bufferCount = false;
    this->m_queue__circularBufferQueueSizeMAX = false;
    this->m_queue__resultQueueSizeMAX = false;
}

bool vuprs::CheckCollaborationBeamformerConfigValid(vuprs::FPGAController *controller, const vuprs::CollaborationBeamformerConfig &config)
{
    bool retval = true;

    if (!controller->ConfigDown())
    {
        throw std::runtime_error("in [CheckCollaborationBeamformerConfigValid] FPGA config not complete.");
    }

    retval &= (config.fs > 0 && config.fs < controller->dev__ADC_Controller.MaxSamplingFrequency());
    retval &= (config.bf_freq__lower < config.fs / 2.0);
    retval &= (config.bf_freq__upper < config.fs / 2.0);
    retval &= (config.bf_freq__lower < config.bf_freq__upper);
    retval &= (config.bf_cov_freqAverageIndex < 1.0);
    retval &= (config.dma__bufferSize % DMA_BUFFER_ALIGNMENT_1_WORD == 0);
    retval &= ((config.dma__bufferSize * config.dma__bufferCount) < controller->mem__DDR.MaxSizeBytes());

    return retval;
}
