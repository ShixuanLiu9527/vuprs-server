#ifndef COLLABORATION_CONFIGS_H
#define COLLABORATION_CONFIGS_H

#include <stdint.h>
#include <vector>
#include "fpga/fpga_controller.h"

#define DEFAULT_SCANNING_POINTS_IN_HALF 100
#define DEFAULT_SCANNING_ALTITUDE_MIN 15.0
#define DEFAULT_SCANNING_WAVE_VELOCITY 346.0

namespace vuprs
{
    /**
     * @brief Collaboration config mask.
     *
     * @note false as default.
     */
    struct CollaborationBeamformerConfigMask
    {
        bool m_fs;                                /* Mask of [fs] */
        bool m_bf_target__alt;                    /* Mask of [bf_target__alt] */
        bool m_bf_target__az;                     /* Mask of [bf_target__az] */
        bool m_bf_waveVelocity;                   /* Mask of [bf_waveVelocity] */
        bool m_bf_freq__lower;                    /* Mask of [bf_freq__lower] */
        bool m_bf_freq__upper;                    /* Mask of [bf_freq_upper] */
        bool m_bf_cov_snapshotsWindowSize;        /* Mask of [bf_cov_snapshotsWindowSize] */
        bool m_bf_cov_freqAverageIndex;           /* Mask of [bf_cov_freqAverageIndex] */
        bool m_dma__bufferSize;                   /* Mask of [dma__bufferSize] */
        bool m_dma__bufferCount;                  /* Mask of [dma__bufferCount] */
        bool m_queue__circularBufferQueueSizeMAX; /* Mask of [queue__circularBufferQueueSizeMAX] */
        bool m_queue__resultQueueSizeMAX;         /* Mask of [queue__resultQueueSizeMAX] */
        CollaborationBeamformerConfigMask() { this->Reset(); }

        /**
         * @brief Reset all mask to false.
         */
        void Reset();
    };

    class CollaborationBeamformerConfig
    {
    private:
        void SetDefault();

    public:
        double fs;                                  /* sampling frequency (unit: Hz), the valid range is [10, 120000] Hz */
        double bf_target__alt;                      /* altitude (unit: degree) of beam former pointing target */
        double bf_target__az;                       /* azimuth (unit: degree) of beam former pointing target */
        double bf_waveVelocity;                     /* wave velocity (m/s). e.g. 346.0 for speed of sound in air */
        double bf_freq__lower;                      /* lower boundary of beam former work frequency (unit: Hz), the valid range is [10, 120000] Hz */
        double bf_freq__upper;                      /* upper boundary of beam former work frequency (unit: Hz), the valid range is [10, 120000] Hz */
        int bf_cov_snapshotsWindowSize;             /* Snapshots window size (to fit covariance matrix) */
        double bf_cov_freqAverageIndex;             /* frequency average index (to fit covariance matrix) */
        uint32_t dma__bufferSize;                   /* AXI DMA descriptor buffer size in bytes */
        uint32_t dma__bufferCount;                  /* AXI DMA descriptor buffer count */
        uint32_t queue__circularBufferQueueSizeMAX; /* MAX size of circular buffer data queue */
        uint32_t queue__resultQueueSizeMAX;         /* MAX size of result data queue */
        vuprs::CollaborationBeamformerConfigMask mask;

        CollaborationBeamformerConfig() { this->SetDefault(); }

        /**
         * @brief Merger config with another config.
         *
         * @note For each field, if the corresponding mask in other.mask is true,
         * @note then use the value in other, otherwise keep the current value.
         */
        void operator+=(const CollaborationBeamformerConfig &other)
        {
            this->fs = other.mask.m_fs ? other.fs : this->fs;
            this->bf_target__alt = other.mask.m_bf_target__alt ? other.bf_target__alt : this->bf_target__alt;
            this->bf_target__az = other.mask.m_bf_target__az ? other.bf_target__az : this->bf_target__az;
            this->bf_waveVelocity = other.mask.m_bf_waveVelocity ? other.bf_waveVelocity : this->bf_waveVelocity;
            this->bf_freq__lower = other.mask.m_bf_freq__lower ? other.bf_freq__lower : this->bf_freq__lower;
            this->bf_freq__upper = other.mask.m_bf_freq__upper ? other.bf_freq__upper : this->bf_freq__upper;
            this->bf_cov_snapshotsWindowSize = other.mask.m_bf_cov_snapshotsWindowSize ? other.bf_cov_snapshotsWindowSize : this->bf_cov_snapshotsWindowSize;
            this->bf_cov_freqAverageIndex = other.mask.m_bf_cov_freqAverageIndex ? other.bf_cov_freqAverageIndex : this->bf_cov_freqAverageIndex;
            this->dma__bufferSize = other.mask.m_dma__bufferSize ? other.dma__bufferSize : this->dma__bufferSize;
            this->dma__bufferCount = other.mask.m_dma__bufferCount ? other.dma__bufferCount : this->dma__bufferCount;
            this->queue__circularBufferQueueSizeMAX = other.mask.m_queue__circularBufferQueueSizeMAX ? other.queue__circularBufferQueueSizeMAX : this->queue__circularBufferQueueSizeMAX;
            this->queue__resultQueueSizeMAX = other.mask.m_queue__resultQueueSizeMAX ? other.queue__resultQueueSizeMAX : this->queue__resultQueueSizeMAX;
        }

        /**
         * @brief Reset all fields to default value, and reset mask.
         */
        void ResetMask() { this->mask.Reset(); }
    };

    struct ScanningConfig
    {
        int pointsInHalf;
        double alt_min;
        bool needRegeneratePositionPoints;

        ScanningConfig() : pointsInHalf(DEFAULT_SCANNING_POINTS_IN_HALF),
                           alt_min(DEFAULT_SCANNING_ALTITUDE_MIN),
                           needRegeneratePositionPoints(true) {}
    };

    struct ScanResult
    {
        std::vector<uint16_t> scanResult; /* scan result in power, unit: dB */
        double minPowerDB;                /* minimum power in dB for scan result */
        double maxPowerDB;                /* maximum power in dB for scan result */
    };

    bool CheckCollaborationBeamformerConfigValid(vuprs::FPGAController *controller,
                                                 const vuprs::CollaborationBeamformerConfig &config);
}

#endif
