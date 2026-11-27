#include "config.h"
#include <algorithm>
#include "algo/bf/bf_engine.h"
#include "algo/bf/bf_algo.h"
#include "logger/check.h"

vuprs::BeamFormingEngine::BeamFormingEngine()
{
    this->thread_pool = std::make_shared<vuprs::ThreadPool>(std::thread::hardware_concurrency());
    this->ResetAll();
}

void vuprs::BeamFormingEngine::SetThreadPool(std::shared_ptr<ThreadPool> pool)
{
    RUNTIME_CHECK(pool != nullptr, "bf", "Thread pool cannot be NULL");
    this->thread_pool = std::move(pool);
    this->array.SetThreadPool(this->thread_pool);
}

vuprs::BeamFormingEngine::~BeamFormingEngine()
{
    vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>>().swap(this->mean_cov_matrix);
    vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>>().swap(this->estimate_cov_matrix);
    vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>>().swap(this->scan_weight_cache);
}

void vuprs::BeamFormingEngine::SetStrategy(std::unique_ptr<BeamFormingAlgo> strategy)
{
    RUNTIME_CHECK(strategy != nullptr, "bf", "Beam forming strategy cannot be NULL");
    /* Drop the previous algorithm's output so it cannot be mistaken for the new one's. */
    this->result_weight_vectors.resize(0, 0);
    this->strategy_ = std::move(strategy);
}

void vuprs::BeamFormingEngine::ResetAll()
{
    this->fs = 0.0;
    this->signal_points = 0;
    this->work_freq_lower = 0.0;
    this->work_freq_upper = 0.0;
    this->work_bin_lo = 0;
    this->work_bin_hi = -1;
    /* flags */
    this->is_array_config_done = false;
    this->is_signal_empty = true;
    this->is_cov_matrix_empty = true;
    this->first_snapshot = true;
    this->covariance_snap_window_size = DEFAULT_COVARIANCE_SNAP_WINDOW_SIZE;
    this->adjacent_freq_average_index = DEFAULT_ADJACENT_FREQ_AVERAGE_INDEX;
    this->scan_cache_valid = false;
    this->ResetCovarianceMatrices();
    this->UpdateParameters();
}

bool vuprs::BeamFormingEngine::ConfigArrayFromJson(const std::string &array_config_json_filename)
{
    if (this->array.LoadArrayFromJson(array_config_json_filename))
    {
        size_t array_size = this->array.size();
        this->element_predelay_time.resize(array_size);
        this->element_predelay_count.resize(array_size);
        this->is_array_config_done = true;
        /* Array geometry may have changed: any existing covariance is no longer valid. */
        this->ResetCovarianceMatrices();
        return true;
    }
    return false;
}

bool vuprs::BeamFormingEngine::ConfigDone() const
{
    return this->is_array_config_done;
}

bool vuprs::BeamFormingEngine::CalculateEnable() const
{
    return this->ConfigDone() && !this->is_signal_empty && !this->is_cov_matrix_empty;
}

int vuprs::BeamFormingEngine::ElementCount() const
{
    RUNTIME_CHECK(this->ConfigDone(), "bf", "Config not complete");
    return this->array.size();
}

void vuprs::BeamFormingEngine::InputSignal(vuprs::SignalData &&signal)
{
    RUNTIME_CHECK(this->ConfigDone(), "bf", "Config not complete");
    const double signal_fs = signal.fs;
    const int signal_points = signal.signal_points;
    this->array.InputElementSignal(std::move(signal));
    this->is_signal_empty = false;
    if (signal_points != this->signal_points || std::abs(signal_fs - this->fs) > 1e-5)
    {
        /* Frequency grid changed: the covariance matrices from the previous grid are invalid. */
        this->ResetCovarianceMatrices();
        this->is_signal_empty = false;
        this->fs = signal_fs;
        this->signal_points = signal_points;
        this->signal_frequency_list = vuprs::GenerateRealFrequencyList(this->signal_points, this->fs);
        this->signal_frequency_list_complex = vuprs::GenerateComplexFrequencyList(this->signal_points, this->fs);
        this->UpdatePredelayExpMatrix();                              /* update matrix here */
        this->array.GetSteeringVectorMatrix(&this->steering_vectors); /* Get steering vectors */
        this->UpdateWorkBinRange();
    }
}

void vuprs::BeamFormingEngine::SetTargetDirection(double alt, double az, double wave_velocity)
{
    RUNTIME_CHECK(this->ConfigDone(), "bf", "Config not complete");
    this->array.UpdateTimeDelay(alt, az, wave_velocity); /* Update time delay */
    if (this->fs > 0.0 && this->signal_points > 0)
        this->array.GetSteeringVectorMatrix(&this->steering_vectors); /* Get steering vectors */
}

void vuprs::BeamFormingEngine::UpdateCovarianceMatrix()
{
    RUNTIME_CHECK(this->ConfigDone(), "bf", "Config not complete");
    RUNTIME_CHECK(!this->is_signal_empty, "bf", "Signal is empty.");
    this->array.GetArraySignalMatrix(&this->snap_signal_matrix_freq_domain, nullptr, true); /* Get array signal */
    int data_points = this->snap_signal_matrix_freq_domain.cols();
    if (this->estimate_cov_matrix.empty())
        this->estimate_cov_matrix.resize(data_points);
    RUNTIME_CHECK(this->estimate_cov_matrix.size() == static_cast<size_t>(data_points), "bf", "Data points in snapshot != latest");
    if (this->mean_cov_matrix.empty())
        this->mean_cov_matrix.resize(data_points);
    RUNTIME_CHECK(this->mean_cov_matrix.size() == static_cast<size_t>(data_points), "bf", "Data points in snapshot != latest");
    /* Chunked parallel update. */
    /* - Phase 1: mean_cov[i] = a * x_i * x_i.H + (1 - a) * mean_cov[i], where x_i is the i-th column of the snapshot signal matrix. -*/
    /* - Each chunk keeps one preallocated M x M scratch matrix so the bin loop performs no heap allocation. - */
    int M = this->snap_signal_matrix_freq_domain.rows();
    int n_chunks = std::max(1, std::min(std::max(1, static_cast<int>(std::thread::hardware_concurrency())), data_points));
    int chunk_size = (data_points + n_chunks - 1) / n_chunks;
    if (static_cast<int>(this->covariance_update_scratch.size()) != n_chunks)
    {
        this->covariance_update_scratch.resize(n_chunks);
    }
    for (auto &scratch : this->covariance_update_scratch)
    {
        scratch.resize(M, M);
    }
    {
        std::vector<std::future<void>> futures;
        futures.reserve(n_chunks);
        for (int c = 0; c < n_chunks; c++)
        {
            int start = c * chunk_size;
            int end = std::min((c + 1) * chunk_size, data_points);
            if (start >= end)
            {
                break;
            }
            futures.emplace_back(this->thread_pool->enqueue(
                [this, c, start, end]()
                {
                    Eigen::Matrix<Eigen::dcomplex, -1, -1> &xcov = this->covariance_update_scratch[c]; /* cov matrix for each frequency band */
                    for (int i = start; i < end; i++)
                    {
                        xcov.noalias() = this->snap_signal_matrix_freq_domain.col(i) *
                                         this->snap_signal_matrix_freq_domain.col(i).adjoint();
                        if (this->first_snapshot)
                        {
                            this->mean_cov_matrix[i] = xcov;
                        }
                        else
                        {
                            /* in-place EWMA (same rounding as a * xcov + (1 - a) * mean) */
                            this->mean_cov_matrix[i] *= this->exp_weighed_moving_average_index_1;
                            this->mean_cov_matrix[i] += this->exp_weighed_moving_average_index * xcov;
                        }
                    }
                }));
        }
        vuprs::WaitForAllFutures(futures);
    }
    /* - Phase 2: estimate_cov[i] = b * mean[i - 1] + a * mean[i] + b * mean[i + 1]. -*/
    /* - All mean matrices are ready after phase 1 (barrier), so the chunks are independent again. - */
    {
        std::vector<std::future<void>> futures;
        futures.reserve(n_chunks);
        for (int c = 0; c < n_chunks; c++)
        {
            int start = c * chunk_size;
            int end = std::min((c + 1) * chunk_size, data_points);
            if (start >= end)
            {
                break;
            }
            futures.emplace_back(this->thread_pool->enqueue(
                [this, start, end, data_points]()
                {
                    for (int i = start; i < end; i++)
                    {
                        if (i <= 1 || i == data_points - 1)
                        {
                            this->estimate_cov_matrix[i] = this->mean_cov_matrix[i];
                            continue;
                        }
                        this->estimate_cov_matrix[i] = this->adjacent_freq_average_index_1 * this->mean_cov_matrix[i - 1] +
                                                       this->adjacent_freq_average_index * this->mean_cov_matrix[i] +
                                                       this->adjacent_freq_average_index_1 * this->mean_cov_matrix[i + 1];
                    }
                }));
        }
        vuprs::WaitForAllFutures(futures);
    }
    this->is_cov_matrix_empty = false;
    this->first_snapshot = false;
    /* Cap at M: only the transition to "enough snapshots" matters. */
    if (this->covariance_update_count < M)
        ++this->covariance_update_count;
}

void vuprs::BeamFormingEngine::SetCovarianceMatrixFittingParam(double snaps_window_size, double adjacent_freq_average_index)
{
    this->covariance_snap_window_size = snaps_window_size;
    this->adjacent_freq_average_index = adjacent_freq_average_index;
    this->UpdateParameters();
}

void vuprs::BeamFormingEngine::SetWorkFrequencyRange(double lower, double upper)
{
    this->work_freq_lower = lower;
    this->work_freq_upper = upper;
    this->UpdateWorkBinRange();
}

void vuprs::BeamFormingEngine::UpdateWorkBinRange()
{
    int f_total = static_cast<int>(this->signal_frequency_list.rows());
    if (f_total <= 0 || this->fs <= 0.0 || this->signal_points <= 0)
    {
        this->work_bin_lo = 0;
        this->work_bin_hi = -1;
        return;
    }
    if (this->work_freq_lower <= 0.0 && this->work_freq_upper <= 0.0)
    {
        this->work_bin_lo = 0;
        this->work_bin_hi = f_total - 1;
        return;
    }
    /* Cover the FIR bandpass transition band (10 bins) so no bin with non-zero gain is skipped. */
    double df = this->fs / static_cast<double>(this->signal_points);
    double pad = 10.0 * df;
    double f_lo = (this->work_freq_lower > 0.0) ? std::max(0.0, this->work_freq_lower - pad) : 0.0;
    double f_hi = (this->work_freq_upper > 0.0) ? std::min(this->fs / 2.0, this->work_freq_upper + pad)
                                                : this->fs / 2.0;
    const double *freq = this->signal_frequency_list.data();
    int lo = static_cast<int>(std::lower_bound(freq, freq + f_total, f_lo) - freq);
    int hi = static_cast<int>(std::upper_bound(freq, freq + f_total, f_hi) - freq) - 1;
    lo = std::max(0, std::min(lo, f_total - 1));
    hi = std::max(0, std::min(hi, f_total - 1));
    if (hi < lo)
    {
        lo = 0;
        hi = f_total - 1;
    }
    this->work_bin_lo = lo;
    this->work_bin_hi = hi;
}

void vuprs::BeamFormingEngine::UpdateParameters()
{
    this->exp_weighed_moving_average_index = 2.0 / (this->covariance_snap_window_size + 1.0); /* N = 2/a - 1 */
    this->exp_weighed_moving_average_index_1 = 1.0 - this->exp_weighed_moving_average_index;  /* 1 - a */
    this->adjacent_freq_average_index_1 = 0.5 * (1.0 - this->adjacent_freq_average_index);
}

void vuprs::BeamFormingEngine::ResetCovarianceMatrices()
{
    this->first_snapshot = true;
    this->is_signal_empty = true;
    this->is_cov_matrix_empty = true;
    this->covariance_update_count = 0;
    this->mean_cov_matrix.clear();
    this->mean_cov_matrix.shrink_to_fit();
    this->estimate_cov_matrix.clear();
    this->estimate_cov_matrix.shrink_to_fit();
}

void vuprs::BeamFormingEngine::GetWeightVectorValues(Eigen::Matrix<Eigen::dcomplex, -1, -1> *dst) const
{
    *dst = this->result_weight_vectors;
}

void vuprs::BeamFormingEngine::GetFIRExpectedFrequencyResponse(Eigen::Matrix<Eigen::dcomplex, -1, -1> *dst, bool consider_predelay) const
{
    RUNTIME_CHECK(dst, "bf", "Destination cannot be NULL");
    RUNTIME_CHECK(!this->is_signal_empty, "bf", "No signal input");
    if (!consider_predelay)
    {
        *dst = this->result_weight_vectors;
        return;
    }
    /* w*(fk) x exp(j*2*pi*fk*Tm); rows are already in ADC channel order. */
    *dst = this->result_weight_vectors.array().conjugate() * this->pre_delay_exp_arg.array();
    /* set 0 & nyquist to real */
    int nyquist_idx = dst->cols() - 1;
    dst->col(0).imag().setZero();
    dst->col(nyquist_idx).imag().setZero();
}

void vuprs::BeamFormingEngine::UpdatePredelayExpMatrix()
{
    uint64_t M = this->array.size();
    /* [T1, T2, ..., TM] */
    Eigen::Map<const Eigen::VectorXd> Tm_vec(this->element_predelay_time.data(), M);
    /* [ exp(j*2*pi*fk*Tm) ] */
    this->pre_delay_exp_arg = (Tm_vec * this->signal_frequency_list_complex.transpose() * 2.0 * PI).array().exp();
}

void vuprs::BeamFormingEngine::UpdateAndGetElementPredelay(double fir_length,
                                                           double fs,
                                                           bool include_fir_group_delay,
                                                           std::vector<int> *element_predelay_count,
                                                           std::vector<double> *element_predelay_time)
{
    RUNTIME_CHECK(this->ConfigDone(), "bf", "Config not complete");
    RUNTIME_CHECK(fs > 0.0, "bf", "Invalid sampling frequency");
    RUNTIME_CHECK(element_predelay_count, "bf", "Predelay output pointer is NULL");
    uint64_t M = this->array.size();
    double Ts = 1.0 / fs;
    int min_predelay_count = INT_MAX;
    const double group_delay_count = (fir_length - 1.0) / 2.0;
    for (uint64_t i = 0; i < M; i++)
    {
        const double time_delay_count = this->array[i].time_delay / Ts;
        this->element_predelay_count[i] = include_fir_group_delay ? -std::round(time_delay_count + group_delay_count)
                                                                  : -std::round(time_delay_count);
        min_predelay_count = std::min(this->element_predelay_count[i], min_predelay_count);
    }
    for (uint64_t i = 0; i < M; i++)
    {
        this->element_predelay_count[i] -= min_predelay_count;
        this->element_predelay_time[i] = this->element_predelay_count[i] * Ts;
    }
    *element_predelay_count = this->element_predelay_count;
    if (element_predelay_time != nullptr)
        *element_predelay_time = this->element_predelay_time;
    /* Refresh FIR predelay phases; InputSignal() only does this when fs/N change. */
    if (!this->element_predelay_time.empty() && this->signal_frequency_list_complex.size() > 0)
        this->UpdatePredelayExpMatrix();
}

bool vuprs::BeamFormingEngine::SnapshotScanCovariance(ScanCovarianceSnapshot *snapshot,
                                                      double freq_lower,
                                                      double freq_upper)
{
    RUNTIME_CHECK(snapshot, "bf", "Snapshot pointer cannot be NULL");
    RUNTIME_CHECK(this->ConfigDone(), "bf", "Config not complete");
    if (this->is_cov_matrix_empty)
    {
        return false;
    }
    int f_total = this->estimate_cov_matrix.size();
    /* Select the in-band frequency bin range [f_lo, f_hi] (closed interval).
     * Bins outside the bandpass only contain noise, so they are skipped. */
    int f_lo = 0;
    int f_hi = f_total - 1;
    if (freq_lower > 0.0 || freq_upper > 0.0)
    {
        if (freq_lower > 0.0)
        {
            f_lo = std::lower_bound(this->signal_frequency_list.data(),
                                    this->signal_frequency_list.data() + f_total,
                                    freq_lower) -
                   this->signal_frequency_list.data();
        }
        if (freq_upper > 0.0)
        {
            f_hi = std::upper_bound(this->signal_frequency_list.data(),
                                    this->signal_frequency_list.data() + f_total,
                                    freq_upper) -
                   this->signal_frequency_list.data() - 1;
        }
    }
    int f_count = f_hi - f_lo + 1;
    /* Copy the in-band covariance matrices and their frequencies.
     * NOTE: must be called while the caller guarantees that estimate_cov_matrix /
     * signal_frequency_list / fs are stable (e.g. under the caller's lock). */
    snapshot->covariance.resize(f_count);
    snapshot->frequency_list.resize(f_count);
    for (int i = 0; i < f_count; i++)
    {
        snapshot->covariance[i] = this->estimate_cov_matrix[f_lo + i];
        snapshot->frequency_list(i) = this->signal_frequency_list(f_lo + i);
    }
    snapshot->fs = this->fs;
    return true;
}

bool vuprs::BeamFormingEngine::ScanForPositionPower(std::vector<double> *res,
                                                    double *max_value,
                                                    double *min_value,
                                                    const std::vector<double> &alt,
                                                    const std::vector<double> &az,
                                                    double wave_velocity,
                                                    bool need_regenerate,
                                                    bool log,
                                                    const ScanCovarianceSnapshot &snapshot)
{
    RUNTIME_CHECK(res, "bf", "Result pointer cannot be NULL");
    RUNTIME_CHECK(this->ConfigDone(), "bf", "Config not complete");
    RUNTIME_CHECK(alt.size() == az.size(), "bf", "Altitude and azimuth size mismatch");

    /* STEP 0: Prepare for data */
    int M = this->array.size();               /* Element counts */
    int k = alt.size();                       /* Scan points counts */
    int f_count = snapshot.covariance.size(); /* In-band frequency counts */
    /* Reuse per-call accumulators (single scan caller at a time). */
    Eigen::Matrix<double, 1, -1> &total_power = this->scan_total_power; /* Total power for each scan position */
    if (total_power.cols() != k)
        total_power = Eigen::Matrix<double, 1, -1>::Zero(k);
    else
        total_power.setZero();
    if (f_count > 0)
    {
        /* STEP 1: (Re)generate scan time delay & weight cache when needed.
         * Scan weight W{f, s} = exp(-j * 2 * pi * f * T{s}) depends only on (fs, scan geometry,
         * wave velocity) and is independent of the signal, so it is cached across scan cycles.
         * The cache is filled here (before tasks are enqueued) and only read inside tasks. */
        bool cache_valid = this->scan_cache_valid &&
                           this->scan_cache_fs == snapshot.fs &&
                           this->scan_cache_wave_velocity == wave_velocity &&
                           this->scan_cache_alt == alt &&
                           this->scan_cache_az == az &&
                           this->scan_cache_freq_list.size() == snapshot.frequency_list.size() &&
                           (this->scan_cache_freq_list.array() == snapshot.frequency_list.array()).all();
        if (need_regenerate || !cache_valid)
        {
            std::lock_guard<std::mutex> lock(this->mut_scan);
            this->scan_timedelay = this->array.GetTimeDelay(alt, az, wave_velocity);
            this->scan_weight_cache.resize(f_count);
            Eigen::Matrix<double, -1, -1> angles(M, k);
            for (int i = 0; i < f_count; i++)
            {
                double f = snapshot.frequency_list(i);
                angles = -2.0 * PI * f * this->scan_timedelay;
                Eigen::Matrix<Eigen::dcomplex, -1, -1> weights(M, k);
                weights.real() = angles.array().cos().matrix();
                weights.imag() = angles.array().sin().matrix();
                this->scan_weight_cache[i] = std::move(weights);
            }
            /* Update cache key */
            this->scan_cache_valid = true;
            this->scan_cache_fs = snapshot.fs;
            this->scan_cache_wave_velocity = wave_velocity;
            this->scan_cache_alt = alt;
            this->scan_cache_az = az;
            this->scan_cache_freq_list = snapshot.frequency_list;
        }
        /* STEP 2: Chunked parallel power calculation.
         * power(f, s) = w.H * R * w = Re( sum_m conj(W{m, s}) * (R * W){m, s} ) / M^2,
         * which needs no eigenvalue decomposition (B in R = B * B.H was a pure factorisation).
         * Each chunk accumulates into a reused buffer, then a single reduction is done
         * after all tasks finish. All buffers are members, so no per-call allocations. */
        int n_chunks = std::min(std::max(1, static_cast<int>(std::thread::hardware_concurrency())), f_count);
        int chunk_size = (f_count + n_chunks - 1) / n_chunks;
        if (static_cast<int>(this->scan_partial_power.size()) != n_chunks)
            this->scan_partial_power.resize(n_chunks);
        if (static_cast<int>(this->scan_rw_scratch.size()) != n_chunks)
            this->scan_rw_scratch.resize(n_chunks);
        for (int c = 0; c < n_chunks; c++)
        {
            if (this->scan_partial_power[c].cols() != k)
                this->scan_partial_power[c] = Eigen::Matrix<double, 1, -1>::Zero(k);
            else
                this->scan_partial_power[c].setZero();
            if (this->scan_rw_scratch[c].rows() != M || this->scan_rw_scratch[c].cols() != k)
                this->scan_rw_scratch[c].resize(M, k);
        }
        std::vector<std::future<void>> futures;
        futures.reserve(n_chunks);
        for (int c = 0; c < n_chunks; c++)
        {
            int start = c * chunk_size;
            int end = std::min((c + 1) * chunk_size, f_count);
            if (start >= end)
            {
                break;
            }
            futures.emplace_back(this->thread_pool->enqueue(
                [this, c, start, end, &snapshot]()
                {
                    Eigen::Matrix<double, 1, -1> &acc = this->scan_partial_power[c];
                    Eigen::Matrix<Eigen::dcomplex, -1, -1> &RW = this->scan_rw_scratch[c];
                    for (int i = start; i < end; i++)
                    {
                        const Eigen::Matrix<Eigen::dcomplex, -1, -1> &R = snapshot.covariance[i];
                        const Eigen::Matrix<Eigen::dcomplex, -1, -1> &W = this->scan_weight_cache[i];
                        RW.noalias() = R * W;
                        acc += (W.array().conjugate() * RW.array()).colwise().sum().real().matrix();
                    }
                }));
        }
        vuprs::WaitForAllFutures(futures);
        for (int c = 0; c < n_chunks; c++)
            total_power += this->scan_partial_power[c];
        total_power /= (double)M * (double)M; /* CBF scan weight: w = ps / M, so power = ps.H * R * ps / M^2 */
    }
    /* Convert to result */
    double max_power = total_power.maxCoeff();
    double min_power = total_power.minCoeff();
    if (log)
    {
        if (min_power > 0.0)
            total_power = total_power.array().log10() * 10.0; /* power -> dB */
        else
            total_power.setZero();
        max_power = total_power.maxCoeff();
        min_power = total_power.minCoeff();
    }
    vuprs::eigenRow2stdVector(total_power, res);
    if (max_value != nullptr)
        *max_value = max_power;
    if (min_value != nullptr)
        *min_value = min_power;
    return true;
}

void vuprs::BeamFormingEngine::CalculateBeamforming()
{
    RUNTIME_CHECK(this->CalculateEnable(), "bf", "Cannot calculate beam forming at that time");
    RUNTIME_CHECK(this->strategy_ != nullptr, "bf", "Beam forming strategy not set");
    int num_freqs = this->estimate_cov_matrix.size(); /* = N / 2 + 1 */
    if (num_freqs <= 0)
        return;
    int M = this->array.size();
    this->result_weight_vectors.resize(M, num_freqs);
    this->strategy_->Prepare(num_freqs, M);
    /* - for DC response */
    this->result_weight_vectors.col(0).setZero();
    /* - for Nyquist frequency */
    this->result_weight_vectors.col(num_freqs - 1) = this->steering_vectors.col(num_freqs - 1) / M;
    this->result_weight_vectors.col(num_freqs - 1).imag().setZero();
    if (num_freqs <= 2)
        return;
    /* Only compute the work band (plus the FIR transition band); everything else is zeroed. */
    int bin_lo = std::max(1, this->work_bin_lo);
    int bin_hi = std::min(num_freqs - 2, this->work_bin_hi);
    if (bin_hi < bin_lo)
    {
        bin_lo = 1;
        bin_hi = num_freqs - 2;
    }
    for (int i = 1; i < num_freqs - 1; ++i)
    {
        if (i < bin_lo || i > bin_hi)
            this->result_weight_vectors.col(i).setZero();
    }
    const int bin_count = bin_hi - bin_lo + 1;
    if (bin_count <= 0)
        return;
    /* A sum of K rank-one snapshots has rank <= K. Until the covariance has been
     * updated at least M times (M = element count) it is rank-deficient, so keep
     * CBF weights and do not attempt a covariance-based solve (DCRCB/MVDR). */
    if (this->covariance_update_count < M)
    {
        for (int i = bin_lo; i <= bin_hi; ++i)
            this->result_weight_vectors.col(i) = this->steering_vectors.col(i) / M;
        return;
    }
    /* Chunk iteration (never spawn more chunks than bins, and never divide by zero). */
    unsigned hw = std::max(1u, std::thread::hardware_concurrency());
    int chunk_number = std::min<int>(static_cast<int>(hw), bin_count);
    int chunk_size = (bin_count + chunk_number - 1) / chunk_number;
    std::vector<std::future<void>> futures;
    futures.reserve(chunk_number);
    for (int chunk_idx = 0; chunk_idx < chunk_number; chunk_idx++)
    {
        int start_idx = bin_lo + chunk_idx * chunk_size;
        int end_idx = std::min(start_idx + chunk_size, bin_hi + 1);
        if (start_idx >= end_idx)
            break;
        futures.emplace_back(this->thread_pool->enqueue(
            [this, start_idx, end_idx, M, chunk_idx]()
            {
                for (int global_idx = start_idx; global_idx < end_idx; global_idx++)
                {
                    try
                    {
                        this->strategy_->CalculateOneFrequency(global_idx,
                                                               chunk_idx,
                                                               this->array,
                                                               this->steering_vectors,
                                                               this->estimate_cov_matrix,
                                                               this->signal_frequency_list,
                                                               &this->result_weight_vectors);
                    }
                    catch (...)
                    {
                        /* Fall back to CBF weights at this frequency. */
                        this->result_weight_vectors.col(global_idx) = this->steering_vectors.col(global_idx) / M;
                    }
                }
            }));
    }
    vuprs::WaitForAllFutures(futures);
}
