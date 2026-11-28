#ifndef HYBRID_BF_CONFIG_H
#define HYBRID_BF_CONFIG_H

#include <stdint.h>
#include <string>
#include <vector>
#include "config.h"

namespace vuprs
{
    /**
     * @brief Collaboration config mask.
     *
     * @note false as default.
     */
    struct HybridBeamformerConfigMask
    {
        bool m_fs;                           /* Mask of [fs] */
        bool m_bf_target__alt;               /* Mask of [bf_target__alt] */
        bool m_bf_target__az;                /* Mask of [bf_target__az] */
        bool m_bf_wave_velocity;             /* Mask of [bf_wave_velocity] */
        bool m_bf_freq__lo;                  /* Mask of [bf_freq__lo] */
        bool m_bf_freq__hi;                  /* Mask of [bf_freq__hi] */
        bool m_bf_cov_snapshots_window_size; /* Mask of [bf_cov_snapshots_window_size] */
        bool m_bf_cov_freq_average_index;    /* Mask of [bf_cov_freq_average_index] */
        bool m_dma__buffer_points;           /* Mask of [dma__buffer_points] */
        bool m_dma__buffer_count;            /* Mask of [dma__buffer_count] */
        bool m_queue__cbuf_queue_size;       /* Mask of [queue__cbuf_queue_size] */

        HybridBeamformerConfigMask() { this->Reset(false); }

        /**
         * @brief Reset all mask to val (false as default).
         */
        void Reset(bool val = false);
    };

    class HybridBeamformerConfig
    {
    private:
        void SetDefault();

    public:
        /*
            Note: The sampling time for each package is:
            dma__buffer_points / fs (seconds)
         */
        double fs;                           /* sampling frequency (unit: Hz), the valid range is [10, 120000] Hz */
        double bf_target__alt;               /* altitude (unit: degree) of beam former pointing target */
        double bf_target__az;                /* azimuth (unit: degree) of beam former pointing target */
        double bf_wave_velocity;             /* wave velocity (m/s). e.g. 346.0 for speed of sound in air */
        double bf_freq__lo;                  /* lower boundary of beam former work frequency (unit: Hz), the valid range is [10, 120000] Hz */
        double bf_freq__hi;                  /* upper boundary of beam former work frequency (unit: Hz), the valid range is [10, 120000] Hz */
        double bf_cov_snapshots_window_size; /* Snapshots window size (to fit covariance matrix) */
        double bf_cov_freq_average_index;    /* frequency average index (to fit covariance matrix) */
        uint32_t dma__buffer_points;         /* Sampling points for each AXI DMA buffer */
        uint32_t dma__buffer_count;          /* [internal param] AXI DMA descriptor buffer count */
        uint32_t queue__cbuf_queue_size;     /* [internal param] MAX size of circular buffer data queue */
        vuprs::HybridBeamformerConfigMask mask;

        HybridBeamformerConfig() { this->SetDefault(); }

        bool FromJson(const std::string &json_filename);

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
            this->bf_wave_velocity = other.mask.m_bf_wave_velocity ? other.bf_wave_velocity : this->bf_wave_velocity;
            this->bf_freq__lo = other.mask.m_bf_freq__lo ? other.bf_freq__lo : this->bf_freq__lo;
            this->bf_freq__hi = other.mask.m_bf_freq__hi ? other.bf_freq__hi : this->bf_freq__hi;
            this->bf_cov_snapshots_window_size = other.mask.m_bf_cov_snapshots_window_size ? other.bf_cov_snapshots_window_size : this->bf_cov_snapshots_window_size;
            this->bf_cov_freq_average_index = other.mask.m_bf_cov_freq_average_index ? other.bf_cov_freq_average_index : this->bf_cov_freq_average_index;
            this->dma__buffer_points = other.mask.m_dma__buffer_points ? other.dma__buffer_points : this->dma__buffer_points;
            this->dma__buffer_count = other.mask.m_dma__buffer_count ? other.dma__buffer_count : this->dma__buffer_count;
            this->queue__cbuf_queue_size = other.mask.m_queue__cbuf_queue_size ? other.queue__cbuf_queue_size : this->queue__cbuf_queue_size;
        }

        /**
         * @brief Reset all fields to default value, and reset mask.
         */
        void ResetMask(bool val = false) { this->mask.Reset(val); }
    };

    struct ScanningConfigMask
    {
        bool m_points_in_hemisphere;
        bool m_alt_min;
        ScanningConfigMask() { this->Reset(false); }
        void Reset(bool val = false);
    };

    struct ScanningConfig
    {
        int points_in_hemisphere;
        double alt_min;
        ScanningConfigMask mask;
        ScanningConfig() : points_in_hemisphere(DEFAULT_SCANNING_POINTS_IN_HALF),
                           alt_min(DEFAULT_SCANNING_ALTITUDE_MIN) {}
        void operator+=(const ScanningConfig &other)
        {
            this->points_in_hemisphere = other.mask.m_points_in_hemisphere ? other.points_in_hemisphere : this->points_in_hemisphere;
            this->alt_min = other.mask.m_alt_min ? other.alt_min : this->alt_min;
        }
        void ResetMask(bool val = false) { this->mask.Reset(val); }
    };

    struct ScanResult
    {
        std::vector<uint16_t> scan_result; /* scan result in power, unit: dB */
        double min_power_db = 0.0;         /* minimum power in dB for scan result */
        double max_power_db = 0.0;         /* maximum power in dB for scan result */
        int points_in_hemisphere = 0;      /* grid half-count used for this result */
        double alt_min = 0.0;              /* grid minimum altitude used for this result */
    };
}

#endif
