#include "config.h"
#include "collab_bf/collaboration_configs.h"
#include "logger/log_manager.h"

void vuprs::CollaborationBeamformerConfig::SetDefault()
{
    this->fs = 10000.0;                           /* sampling frequency (unit: Hz) */
    this->bf_target__alt = 90.0;                  /* altitude (unit: degree) beam former pointing target */
    this->bf_target__az = 0.0;                    /* azimuth (unit: degree) beam former pointing target */
    this->bf_waveVelocity = 346.0;                /* wave velocity of sound */
    this->bf_freq__lower = 500.0;                 /* lower boundary of beam former work frequency (unit: Hz) */
    this->bf_freq__upper = 3000.0;                /* upper boundary of beam former work frequency (unit: Hz) */
    this->bf_cov_snapshotsWindowSize = 200;       /* Snapshots window size (to fit covariance matrix) >= 200 */
    this->bf_cov_freqAverageIndex = 0.8;          /* frequency average index (to fit covariance matrix) */
    this->dma__bufferSize = 32768;                /* [internal param] AXI DMA descriptor buffer size */
    this->dma__bufferCount = 20;                  /* [internal param] AXI DMA descriptor buffer count */
    this->queue__circularBufferQueueSizeMAX = 10; /* [internal param] depth of multi-channel data queue */
    this->queue__resultQueueSizeMAX = 10;         /* [internal param] depth of result data queue */
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
    PARAM_CHECK(controller->ConfigDown(), "collab_bf", " in [CheckCollaborationBeamformerConfigValid] FPGA config not complete.");
    retval &= (config.fs > 0 && config.fs < controller->dev__ADC_Controller.MaxSamplingFrequency());
    retval &= (config.bf_freq__lower < config.fs / 2.0);
    retval &= (config.bf_freq__upper < config.fs / 2.0);
    retval &= (config.bf_freq__lower < config.bf_freq__upper);
    retval &= (config.bf_cov_freqAverageIndex < 1.0);
    retval &= (config.dma__bufferSize % DMA_BUFFER_ALIGNMENT_1_WORD == 0);
    retval &= ((config.dma__bufferSize * config.dma__bufferCount) < controller->mem__DDR.MaxSizeBytes());
    return retval;
}

vuprs::ScanningConfig::ScanningConfig() : pointsInHalf(DEFAULT_SCANNING_POINTS_IN_HALF),
                                          alt_min(DEFAULT_SCANNING_ALTITUDE_MIN),
                                          needRegeneratePositionPoints(true) {}
