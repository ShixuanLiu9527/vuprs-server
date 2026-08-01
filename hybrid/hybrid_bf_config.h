#ifndef HYBRID_BF_CONFIG_H
#define HYBRID_BF_CONFIG_H

#include <stdint.h>
#include <vector>
#include "config.h"
#include "fpga/fpga_controller.h"

namespace vuprs
{
    /**
     * @brief Collaboration config mask.
     *
     * @note false as default.
     */
    struct HybridBeamformerConfigMask
    {
        bool m_fs;                                    /* Mask of [fs] */
        bool m_bf_target__alt;                        /* Mask of [bf_target__alt] */
        bool m_bf_target__az;                         /* Mask of [bf_target__az] */
        bool m_bf_waveVelocity;                       /* Mask of [bf_wave_velocity] */
        bool m_bf_freq__lower;                        /* Mask of [bf_freq__lower] */
        bool m_bf_freq__upper;                        /* Mask of [bf_freq_upper] */
        bool m_bf_cov_snapshots_window_size;          /* Mask of [bf_cov_snapshots_window_size] */
        bool m_bf_cov_freq_average_index;             /* Mask of [bf_cov_freq_average_index] */
        bool m_dma__bufferSize;                       /* Mask of [dma__buffer_size] */
        bool m_dma__bufferCount;                      /* Mask of [dma__buffer_count] */
        bool m_queue__circular_buffer_queue_size_max; /* Mask of [queue__circular_buffer_queue_size_max] */
        bool m_queue__result_queue_size_max;          /* Mask of [queue__result_queue_size_max] */

        HybridBeamformerConfigMask() { this->Reset(); }

        /**
         * @brief Reset all mask to false.
         */
        void Reset();
    };

    class HybridBeamformerConfig
    {
    private:
        void SetDefault();

    public:
        /*
            Note: The sampling time for each package is:
            dma__buffer_size / (sizeof(uint32_t)) / fs (seconds)
         */
        double fs;                                      /* sampling frequency (unit: Hz), the valid range is [10, 120000] Hz */
        double bf_target__alt;                          /* altitude (unit: degree) of beam former pointing target */
        double bf_target__az;                           /* azimuth (unit: degree) of beam former pointing target */
        double bf_wave_velocity;                        /* wave velocity (m/s). e.g. 346.0 for speed of sound in air */
        double bf_freq__lower;                          /* lower boundary of beam former work frequency (unit: Hz), the valid range is [10, 120000] Hz */
        double bf_freq__upper;                          /* upper boundary of beam former work frequency (unit: Hz), the valid range is [10, 120000] Hz */
        int bf_cov_snapshots_window_size;               /* Snapshots window size (to fit covariance matrix) */
        double bf_cov_freq_average_index;               /* frequency average index (to fit covariance matrix) */
        uint32_t dma__buffer_size;                      /* [internal param] AXI DMA descriptor buffer size in bytes */
        uint32_t dma__buffer_count;                     /* [internal param] AXI DMA descriptor buffer count */
        uint32_t queue__circular_buffer_queue_size_max; /* [internal param] MAX size of circular buffer data queue */
        uint32_t queue__result_queue_size_max;          /* [internal param] MAX size of result data queue */
        vuprs::HybridBeamformerConfigMask mask;

        HybridBeamformerConfig() { this->SetDefault(); }

        /**
         * @brief Merger config with another config.
         *
         * @note For each field, if the corresponding mask in other.mask is true,
         * @note then use the value in other, otherwise keep the current value.
         */
        void operator+=(const HybridBeamformerConfig &other)
        {
            this->fs = other.mask.m_fs ? other.fs : this->fs;
            this->bf_target__alt = other.mask.m_bf_target__alt ? other.bf_target__alt : this->bf_target__alt;
            this->bf_target__az = other.mask.m_bf_target__az ? other.bf_target__az : this->bf_target__az;
            this->bf_wave_velocity = other.mask.m_bf_waveVelocity ? other.bf_wave_velocity : this->bf_wave_velocity;
            this->bf_freq__lower = other.mask.m_bf_freq__lower ? other.bf_freq__lower : this->bf_freq__lower;
            this->bf_freq__upper = other.mask.m_bf_freq__upper ? other.bf_freq__upper : this->bf_freq__upper;
            this->bf_cov_snapshots_window_size = other.mask.m_bf_cov_snapshots_window_size ? other.bf_cov_snapshots_window_size : this->bf_cov_snapshots_window_size;
            this->bf_cov_freq_average_index = other.mask.m_bf_cov_freq_average_index ? other.bf_cov_freq_average_index : this->bf_cov_freq_average_index;
            this->dma__buffer_size = other.mask.m_dma__bufferSize ? other.dma__buffer_size : this->dma__buffer_size;
            this->dma__buffer_count = other.mask.m_dma__bufferCount ? other.dma__buffer_count : this->dma__buffer_count;
            this->queue__circular_buffer_queue_size_max = other.mask.m_queue__circular_buffer_queue_size_max ? other.queue__circular_buffer_queue_size_max : this->queue__circular_buffer_queue_size_max;
            this->queue__result_queue_size_max = other.mask.m_queue__result_queue_size_max ? other.queue__result_queue_size_max : this->queue__result_queue_size_max;
        }

        /**
         * @brief Reset all fields to default value, and reset mask.
         */
        void ResetMask() { this->mask.Reset(); }
    };

    struct ScanningConfig
    {
        int points_in_hemisphere;
        double alt_min;
        bool need_regenerate_position_points;
        ScanningConfig() : points_in_hemisphere(DEFAULT_SCANNING_POINTS_IN_HALF),
                           alt_min(DEFAULT_SCANNING_ALTITUDE_MIN),
                           need_regenerate_position_points(true) {}
    };

    struct ScanResult
    {
        std::vector<uint16_t> scan_result; /* scan result in power, unit: dB */
        double min_power_db;               /* minimum power in dB for scan result */
        double max_power_db;               /* maximum power in dB for scan result */
    };

    bool CheckCollaborationBeamformerConfigValid(vuprs::FPGAController *controller,
                                                 const vuprs::HybridBeamformerConfig &config);
}

#endif
