#ifndef BEAM_FORMER_TEMPLATE_H
#define BEAM_FORMER_TEMPLATE_H

#include <mutex>
#include "algorithm/bf/beam_forming_basic.h"
#include "algorithm/bf/beam_forming_algorithm.h"

namespace vuprs
{
    /**
     * @brief Scan covariance snapshot.
     *
     * @note Captured by WidebandBeamformerTemplate::SnapshotScanCovariance() while the caller
     * @note holds its own lock (fast copy), then consumed by ScanForPositionPower() which
     * @note reads no shared state and therefore needs no lock.
     */
    struct ScanCovarianceSnapshot
    {
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>> covariance; /* In-band covariance matrices, size = f_count */
        Eigen::Matrix<double, -1, 1> frequency_list;                                  /* In-band bin frequencies, size = f_count */
        double fs;                                                                    /* Sampling frequency at snapshot time */
    };

    class WidebandBeamformerTemplate
    {
    private:
        std::unique_ptr<ThreadPool> thread_pool;
        std::unique_ptr<ThreadPool> thread_pool_scan;
        double covariance_snap_window_size;
        double adjacent_freq_average_index;
        double adjacent_freq_average_index_1;
        double exp_weighed_moving_average_index;
        double exp_weighed_moving_average_index_1;

        void UpdateParameters();

        /**
         * @brief Get element predelay parameters.
         *
         * @note Check PredelayEnable() in advance.
         *
         * @param fir_length FIR filter bank length.
         * @param fs sampling frequency
         * @param include_fir_group_delay true: include FIR group delay, false: exclude FIR group delay.
         * @param element_predelay_count integer delay. (count = -round[delay{m}/Ts + (L-1)/2])
         * @param elementPredelay integer delay time. (Tm = -round[delay{m}/Ts + (L-1)/2] * Ts)
         * @param channel_name corresponding channel name.
         */
        void UpdateElementPredelay_externalFS(
            double fir_length, double fs, bool include_fir_group_delay,
            std::vector<int> *element_predelay_count,
            std::vector<double> *element_predelay_time,
            std::vector<std::string> *channel_name);

    protected:
        bool first_snapshot;
        bool is_array_config_done;
        bool is_signal_empty, is_cov_matrix_empty;
        std::mutex mut_scan;
        Eigen::Matrix<double, -1, -1> scan_timedelay;                                          /* Size: M x numScans, real time delay T{m, s} = pos{m}.dot(pv{s}) / v, controlled by mut_scan */
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>> scan_weight_cache;   /* Size: numScans x numBands(in-band), scan weight W{f, s} = exp(-j * 2 * pi * f * T{m, s}), controlled by mut_scan */
        bool scan_cache_valid;                                                                 /* Scan weight cache valid flag, controlled by mut_scan */
        double scan_cache_fs;                                                                  /* Cache key: sampling frequency, controlled by mut_scan */
        double scan_cache_wave_velocity;                                                       /* Cache key: wave velocity, controlled by mut_scan */
        std::vector<double> scan_cache_alt;                                                    /* Cache key: altitude list, controlled by mut_scan */
        std::vector<double> scan_cache_az;                                                     /* Cache key: azimuth list, controlled by mut_scan */
        Eigen::Matrix<double, -1, 1> scan_cache_freq_list;                                     /* Cache key: in-band frequency list, controlled by mut_scan */
        vuprs::BeamFormingArray array;                                                         /* beamforming array */
        vuprs::BeamFormingScanArray scan_array;                                                /* for scanning, which has same element position as array but different time delay */
        Eigen::Matrix<Eigen::dcomplex, -1, -1> snap_signal_matrix_freq_domain;                 /* Size: (M) x (N / 2 + 1) */
        Eigen::Matrix<Eigen::dcomplex, -1, -1> steering_vectors;                               /* Size: (M) x (N / 2 + 1) */
        Eigen::Matrix<Eigen::dcomplex, -1, -1> result_weight_vectors;                          /* Size: (M) x (N / 2 + 1) */
        Eigen::Matrix<double, -1, 1> signal_frequency_list;                                    /* [F0, F1, ..., FN/2] Size: (N / 2 + 1) */
        Eigen::Matrix<Eigen::dcomplex, -1, 1> signal_frequency_list_complex;                   /* [jF0, jF1, ..., jFN/2] Size: (N / 2 + 1) */
        double fs;                                                                             /* Current sampling frequency */
        int signal_points;                                                                     /* Current signal points */
        std::vector<int> element_predelay_count;                                               /* Predelay count (size = M) */
        std::vector<double> element_predelay_time;                                             /* Predelay time = count * Ts (size = M) */
        std::vector<std::string> element_channel_name;                                         /* element channel name list (size = M) */
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>> mean_cov_matrix;     /* Size: N / 2 + 1, cov_matrix[i] is the mean cov matrix in band [i] */
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>> estimate_cov_matrix; /* Size: N / 2 + 1, cov_matrix[i] is the mean cov matrix in band [i] */

        /**
         * @brief Calculate beamforming for one frequency.
         *
         * @note for frequency index [i]: corresponding steering vector = steering_vectors[i];
         * @note                          corresponding covariance matrix = estimate_cov_matrix[i];
         * @note                          result weight vector = result_weight_vectors.col(i);
         *
         * @param freq_index frequency index
         */
        virtual void CalculateBeamformingForOneFreq(int freq_index) = 0;

    public:
        WidebandBeamformerTemplate();

        virtual ~WidebandBeamformerTemplate();

        /* STEP 1: CONFIG */

        /**
         * @brief Config beam forming array from JSON file.
         *
         * @note Check ConfigDone() in advance.
         *
         * @param array_config_json_filename JSON file name.
         */
        bool ConfigArrayFromJson(const std::string &array_config_json_filename);

        /* STEP 2: INPUT SIGNAL */

        /**
         * @brief Input signal.
         *
         * @note Will not update covariance matrix.
         */
        void InputSignal(const vuprs::SignalData &signal);

        /* STEP 3: UPDATE COVARIANCE MATRIX */

        /**
         * @brief Update covariance matrix.
         */
        void UpdateCovarianceMatrix();

        /**
         * @brief Set target direction.
         *
         * @note Step 1: Update time delay.
         * @note Step 2: Update steering vector.
         */
        void SetTargetDirection(double alt,
                                double az,
                                double wave_velocity);

        /* STEP 5: DO BEAM FORMING CALCULATION */

        /**
         * @brief Do beam forming calculation.
         *
         * @note Check CalculateEnable() in advance.
         */
        void CalculateBeamforming();

        /* STEP 6: GET RESULT */

        /**
         * @brief Get weight vector list for all frequency.
         *
         * @param dst output weight vector list.
         */
        void GetWeightVectorValues(Eigen::Matrix<Eigen::dcomplex, -1, -1> *dst) const;

        /**
         * @brief Get expected frequency response of FIR filter bank.
         *
         * @param dst output expected frequency response.
         * @param channel_name corresponding channel name.
         * @param consider_predelay if consider predelay in frequency response.
         */
        void GetFIRExpectedFrequencyResponse(Eigen::Matrix<Eigen::dcomplex, -1, -1> *dst,
                                             std::vector<std::string> *channel_name,
                                             bool consider_predelay) const;

        /**
         * @brief Set covariance matrix fitting parameters.
         */
        void SetCovarianceMatrixFittingParam(double snaps_window_size = 100.0, double adjacent_freq_average_index = 0.8);

        /**
         * @brief Reset covariance matrices.
         */
        void ResetCovarianceMatrices();

        /**
         * @brief Get element predelay parameters.
         *
         * @note Use internal sampling frequency (set by certain signal).
         * @note The output predelay count will be aligned to 0.
         * @note e.g. predelay count = [13, 12, 23]
         * @note then: output predelay count = [13 - 12, 12 - 12, 23 - 12] = [1, 0, 11].
         *
         * @param fir_length FIR filter bank length.
         * @param include_fir_group_delay true: include FIR group delay, false: exclude FIR group delay.
         * @param element_predelay_count (cannot be NULL) integer delay. (count = -round[delay{m}/Ts + (L-1)/2])
         * @param element_predelay_time (cannot be NULL) integer delay time. (Tm = -round[delay{m}/Ts + (L-1)/2] * Ts)
         * @param channel_name (cannot be NULL) corresponding channel name.
         */
        void UpdateAndGetElementPredelay(double fir_length,
                                         bool include_fir_group_delay,
                                         std::vector<int> *element_predelay_count,
                                         std::vector<double> *element_predelay_time,
                                         std::vector<std::string> *channel_name);

        /**
         * @brief Get element predelay parameters.
         *
         * @note The output predelay count will be aligned to 0.
         * @note e.g. predelay count = [13, 12, 23]
         * @note then: output predelay count = [13 - 12, 12 - 12, 23 - 12] = [1, 0, 11].
         *
         * @param fir_length FIR filter bank length.
         * @param fs sampling frequency.
         * @param include_fir_group_delay true: include FIR group delay, false: exclude FIR group delay.
         * @param element_predelay_count (cannot be NULL) integer delay. (count = -round[delay{m}/Ts + (L-1)/2])
         * @param element_predelay_time (cannot be NULL) integer delay time. (Tm = -round[delay{m}/Ts + (L-1)/2] * Ts)
         * @param channel_name (cannot be NULL) corresponding channel name.
         */
        void UpdateAndGetElementPredelay(double fir_length,
                                         double fs,
                                         bool include_fir_group_delay,
                                         std::vector<int> *element_predelay_count,
                                         std::vector<double> *element_predelay_time,
                                         std::vector<std::string> *channel_name);

        bool ConfigDone() const;
        bool CalculateEnable() const;

        /**
         * @brief Snapshot the in-band covariance matrices and their frequencies.
         *
         * @note Must be called while the caller guarantees that estimate_cov_matrix /
         * @note signal_frequency_list / fs are stable (e.g. under the caller's lock).
         * @note The copy itself is fast, so the lock is only held for the duration of this call.
         *
         * @param snapshot output snapshot.
         * @param freq_lower lower boundary of the scanned frequency band (unit: Hz). <= 0 means no lower bound.
         * @param freq_upper upper boundary of the scanned frequency band (unit: Hz). <= 0 means no upper bound.
         *
         * @retval true: success.
         * @retval false: failed (e.g. covariance matrix not ready).
         */
        bool SnapshotScanCovariance(ScanCovarianceSnapshot *snapshot,
                                    double freq_lower,
                                    double freq_upper);

        /**
         * @brief Scan for position power.
         *
         * @note Power map = sum over in-band bins of w(f,s).H * R(f) * w(f,s), where w is the CBF scan weight.
         * @note Scan weights only depend on (fs, scan geometry, wave velocity) and are cached; they are
         * @note NOT recomputed unless `need_regenerate` is set or the cache key (fs / alt / az / wave_velocity /
         * @note frequency list) has changed.
         * @note Lock-free: reads only the caller-provided `snapshot` and the scan-only weight cache, so it
         * @note can be called outside any lock as long as scan calls do not overlap.
         *
         * @param res output position power result. Size: alt.size()
         * @param max_value output max power value in scan result. (optional, can be NULL)
         * @param min_value output min power value in scan result. (optional, can be NULL)
         * @param alt altitude list (degree).
         * @param az azimuth list (degree).
         * @param wave_velocity wave velocity (m/s).
         * @param need_regenerate true: regenerate scan weights, false: do not regenerate scan weights.
         * @param log true: log, false: do not log.
         * @param snapshot covariance snapshot captured by SnapshotScanCovariance().
         *
         * @retval true: success.
         * @retval false: failed.
         */
        bool ScanForPositionPower(std::vector<double> *res,
                                  double *max_value,
                                  double *min_value,
                                  const std::vector<double> &alt,
                                  const std::vector<double> &az,
                                  double wave_velocity,
                                  bool need_regenerate,
                                  bool log,
                                  const ScanCovarianceSnapshot &snapshot);

        /**
         * @brief Reset all.
         */
        void ResetAll();

        /**
         * @brief Beam forming element count.
         */
        int ElementCount() const;

        EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    };
}

#endif
