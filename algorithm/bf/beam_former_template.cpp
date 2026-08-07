#include "config.h"
#include <algorithm>
#include "algorithm/bf/beam_former_template.h"
#include "logger/check.h"

vuprs::WidebandBeamformerTemplate::WidebandBeamformerTemplate()
{
    this->thread_pool = std::make_unique<vuprs::ThreadPool>(std::thread::hardware_concurrency());
    this->thread_pool_scan = std::make_unique<vuprs::ThreadPool>(std::thread::hardware_concurrency());
    this->ResetAll();
}

vuprs::WidebandBeamformerTemplate::~WidebandBeamformerTemplate()
{
    vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>>().swap(this->mean_cov_matrix);
    vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>>().swap(this->estimate_cov_matrix);
    vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>>().swap(this->scan_weight_cache);
}

void vuprs::WidebandBeamformerTemplate::ResetAll()
{
    this->fs = 0.0;
    this->signal_points = 0;

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

bool vuprs::WidebandBeamformerTemplate::ConfigArrayFromJson(const std::string &array_config_json_filename)
{
    if (this->array.LoadArrayFromJson(array_config_json_filename))
    {
        int array_size = this->array.size();
        this->element_channel_name.resize(array_size);
        this->element_predelay_time.resize(array_size);
        this->element_predelay_count.resize(array_size);
        for (int i = 0; i < array_size; i++)
        {
            this->element_channel_name[i] = this->array[i].adc_channel;
        }
        this->is_array_config_done = true;
        this->scan_array.LoadArrayFromJson(array_config_json_filename); /* Load scan array, which has same element position as array but different time delay */
        return true;
    }
    return false;
}

bool vuprs::WidebandBeamformerTemplate::ConfigDone() const
{
    return this->is_array_config_done;
}

bool vuprs::WidebandBeamformerTemplate::CalculateEnable() const
{
    return this->ConfigDone() && !this->is_signal_empty && !this->is_cov_matrix_empty;
}

int vuprs::WidebandBeamformerTemplate::ElementCount() const
{
    RUNTIME_CHECK(this->ConfigDone(), "bf", "Config not complete");
    return this->array.size();
}

void vuprs::WidebandBeamformerTemplate::InputSignal(const vuprs::SignalData &signal)
{
    RUNTIME_CHECK(this->ConfigDone(), "bf", "Config not complete");
    this->array.InputElementSignal(signal);
    this->is_signal_empty = false;
    if (signal.signal_points != this->signal_points && std::abs(signal.fs - this->fs) > 1e-3)
    {
        this->fs = signal.fs;
        this->signal_points = signal.signal_points;
        this->signal_frequency_list = vuprs::GenerateRealFrequencyList(this->signal_points, this->fs);
        this->signal_frequency_list_complex = vuprs::GenerateComplexFrequencyList(this->signal_points, this->fs);
        this->array.GetSteeringVectorMatrix(&this->steering_vectors); /* Get steering vectors */
    }
}

void vuprs::WidebandBeamformerTemplate::SetTargetDirection(double alt, double az, double wave_velocity)
{
    RUNTIME_CHECK(this->ConfigDone(), "bf", "Config not complete");
    this->array.UpdateTimeDelay(alt, az, wave_velocity); /* Update time delay */
    if (this->fs > 0.0 && this->signal_points > 0)
    {
        this->array.GetSteeringVectorMatrix(&this->steering_vectors); /* Get steering vectors */
    }
}

void vuprs::WidebandBeamformerTemplate::UpdateCovarianceMatrix()
{
    RUNTIME_CHECK(this->ConfigDone(), "bf", "Config not complete");
    RUNTIME_CHECK(!this->is_signal_empty, "bf", "Signal is empty.");
    this->array.GetArraySignalMatrix(&this->snap_signal_matrix_freq_domain, nullptr, true); /* Get array signal */
    int data_points = this->snap_signal_matrix_freq_domain.cols();
    if (this->estimate_cov_matrix.size() <= 0)
    {
        this->estimate_cov_matrix.resize(data_points);
    }
    else if (this->estimate_cov_matrix.size() != data_points)
    {
        RUNTIME_CHECK(false, "bf", "Data points in snapshot != latest");
    }
    if (this->mean_cov_matrix.size() <= 0)
    {
        this->mean_cov_matrix.resize(data_points);
    }
    else if (this->mean_cov_matrix.size() != data_points)
    {
        RUNTIME_CHECK(false, "bf", "Data points in snapshot != latest");
    }
    /* Update mean covariance matrix */
    for (int i = 0; i < data_points; i++)
    {
        Eigen::Matrix<Eigen::dcomplex, -1, 1> snapshot_freq_signal = this->snap_signal_matrix_freq_domain.col(i);           /* each col of Snapshot Signal Matrix */
        Eigen::Matrix<Eigen::dcomplex, -1, -1> snapshot_cov_matrix = snapshot_freq_signal * snapshot_freq_signal.adjoint(); /* cov matrix for each frequency band */
        if (this->first_snapshot)
        {
            this->mean_cov_matrix[i] = snapshot_cov_matrix;
        }
        else
        {
            this->mean_cov_matrix[i] = this->exp_weighed_moving_average_index * snapshot_cov_matrix +
                                       this->exp_weighed_moving_average_index_1 * this->mean_cov_matrix[i];
        }
    }
    /* Update estimate covariance matrix */
    for (int i = 0; i < data_points; i++)
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
    this->is_cov_matrix_empty = false;
    this->first_snapshot = false;
}

void vuprs::WidebandBeamformerTemplate::SetCovarianceMatrixFittingParam(double snaps_window_size, double adjacent_freq_average_index)
{
    this->covariance_snap_window_size = snaps_window_size;
    this->adjacent_freq_average_index = adjacent_freq_average_index;
    this->UpdateParameters();
}

void vuprs::WidebandBeamformerTemplate::UpdateParameters()
{
    this->exp_weighed_moving_average_index = 2.0 / (this->covariance_snap_window_size + 1.0); /* N = 2/a - 1 */
    this->exp_weighed_moving_average_index_1 = 1.0 - this->exp_weighed_moving_average_index;  /* 1 - a */
    this->adjacent_freq_average_index_1 = 0.5 * (1.0 - this->adjacent_freq_average_index);
}

void vuprs::WidebandBeamformerTemplate::ResetCovarianceMatrices()
{
    this->first_snapshot = true;
    this->is_signal_empty = true;
    this->is_cov_matrix_empty = true;
    this->mean_cov_matrix.clear();
    this->mean_cov_matrix.shrink_to_fit();
    this->estimate_cov_matrix.clear();
    this->estimate_cov_matrix.shrink_to_fit();
}

void vuprs::WidebandBeamformerTemplate::GetWeightVectorValues(Eigen::Matrix<Eigen::dcomplex, -1, -1> *dst) const
{
    *dst = this->result_weight_vectors;
}

void vuprs::WidebandBeamformerTemplate::GetFIRExpectedFrequencyResponse(Eigen::Matrix<Eigen::dcomplex, -1, -1> *dst, std::vector<std::string> *channel_name, bool consider_predelay) const
{
    RUNTIME_CHECK(dst && channel_name, "bf", "Destination cannot be NULL");
    RUNTIME_CHECK(!this->is_signal_empty, "bf", "No signal input");
    if (!consider_predelay)
    {
        *dst = this->result_weight_vectors;
        return;
    }
    uint64_t M = this->array.size();
    /* [T1, T2, ..., TM] */
    Eigen::Map<const Eigen::VectorXd> Tm_vec(this->element_predelay_time.data(), M);
    /* [ exp(j*2*pi*fk*Tm) ] */
    Eigen::Matrix<Eigen::dcomplex, -1, -1> exp_arg = (Tm_vec * this->signal_frequency_list_complex.transpose() * 2.0 * PI).array().exp();
    /* w*(fk) x exp(j*2*pi*fk*Tm) */
    *dst = this->result_weight_vectors.array().conjugate() * exp_arg.array();
    *channel_name = this->element_channel_name;
    /* set 0 & nyquist to real */
    int nyquist_idx = dst->cols() - 1;
    dst->col(0).imag().setZero();
    dst->col(nyquist_idx).imag().setZero();
}

void vuprs::WidebandBeamformerTemplate::UpdateElementPredelay_externalFS(double fir_length,
                                                                         double fs,
                                                                         bool include_fir_group_delay,
                                                                         std::vector<int> *element_predelay_count,
                                                                         std::vector<double> *element_predelay_time,
                                                                         std::vector<std::string> *channel_name)
{
    RUNTIME_CHECK(this->ConfigDone(), "bf", "No signal input");
    RUNTIME_CHECK(element_predelay_time && element_predelay_count && channel_name, "bf", "Predelay pointer is NULL");
    /* Calculate predelay */
    uint64_t M = this->array.size();
    double Ts = 1.0 / fs;
    int min_predelay_count = INT_MAX;
    for (uint64_t i = 0; i < M; i++)
    {
        this->element_predelay_count[i] = include_fir_group_delay ? -std::round(this->array[i].time_delay / Ts + (fir_length - 1.0) / 2.0) : -std::round(this->array[i].time_delay / Ts);
        min_predelay_count = std::min(this->element_predelay_count[i], min_predelay_count);
    }
    for (uint64_t i = 0; i < M; i++)
    {
        this->element_predelay_count[i] -= min_predelay_count;
        this->element_predelay_time[i] = this->element_predelay_count[i] * Ts;
    }
    *element_predelay_time = this->element_predelay_time;
    *element_predelay_count = this->element_predelay_count;
    *channel_name = this->element_channel_name;
}

void vuprs::WidebandBeamformerTemplate::UpdateAndGetElementPredelay(double fir_length,
                                                                    bool include_fir_group_delay,
                                                                    std::vector<int> *element_predelay_count,
                                                                    std::vector<double> *element_predelay_time,
                                                                    std::vector<std::string> *channel_name)
{
    RUNTIME_CHECK(this->ConfigDone(), "bf", "Signal is empty");
    this->UpdateElementPredelay_externalFS(fir_length,
                                           this->fs,
                                           include_fir_group_delay,
                                           element_predelay_count,
                                           element_predelay_time,
                                           channel_name);
}

void vuprs::WidebandBeamformerTemplate::UpdateAndGetElementPredelay(double fir_length,
                                                                    double fs,
                                                                    bool include_fir_group_delay,
                                                                    std::vector<int> *element_predelay_count,
                                                                    std::vector<double> *element_predelay_time,
                                                                    std::vector<std::string> *channel_name)
{
    this->UpdateElementPredelay_externalFS(fir_length,
                                           fs, include_fir_group_delay,
                                           element_predelay_count,
                                           element_predelay_time,
                                           channel_name);
}

bool vuprs::WidebandBeamformerTemplate::SnapshotScanCovariance(ScanCovarianceSnapshot *snapshot,
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

bool vuprs::WidebandBeamformerTemplate::ScanForPositionPower(std::vector<double> *res,
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
    int M = this->array.size();                                                       /* Element counts */
    int k = alt.size();                                                               /* Scan points counts */
    int f_count = snapshot.covariance.size();                                         /* In-band frequency counts */
    Eigen::Matrix<double, 1, -1> total_power = Eigen::Matrix<double, 1, -1>::Zero(k); /* Total power for each scan position */
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
            this->scan_timedelay = this->scan_array.GetTimeDelay(alt, az, wave_velocity);
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
         * Each chunk accumulates locally, then a single reduction is done after all tasks finish. */
        int n_chunks = std::min(std::max(1, static_cast<int>(std::thread::hardware_concurrency())), f_count);
        int chunk_size = (f_count + n_chunks - 1) / n_chunks;
        std::vector<Eigen::Matrix<double, 1, -1>> partials(n_chunks);
        for (int c = 0; c < n_chunks; c++)
        {
            partials[c] = Eigen::Matrix<double, 1, -1>::Zero(k);
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
            futures.emplace_back(this->thread_pool_scan->enqueue(
                [this, c, k, start, end, &partials, &snapshot]()
                {
                    Eigen::Matrix<double, 1, -1> acc = Eigen::Matrix<double, 1, -1>::Zero(k);
                    for (int i = start; i < end; i++)
                    {
                        const Eigen::Matrix<Eigen::dcomplex, -1, -1> &R = snapshot.covariance[i];
                        const Eigen::Matrix<Eigen::dcomplex, -1, -1> &W = this->scan_weight_cache[i];
                        Eigen::Matrix<Eigen::dcomplex, -1, -1> RW = R * W;
                        acc += (W.array().conjugate() * RW.array()).colwise().sum().real().matrix();
                    }
                    partials[c] = acc;
                }));
        }
        for (auto &f : futures)
            f.get();
        for (int c = 0; c < n_chunks; c++)
            total_power += partials[c];
        total_power /= (double)M * (double)M; /* CBF scan weight: w = ps / M, so power = ps.H * R * ps / M^2 */
    }
    /* Convert to result */
    double max_power = total_power.maxCoeff();
    double min_power = total_power.minCoeff();
    if (log)
    {
        if (min_power > 0.0)
            total_power = total_power.array().log10() * 20.0; /* Convert to dB */
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

void vuprs::WidebandBeamformerTemplate::CalculateBeamforming()
{
    RUNTIME_CHECK(this->CalculateEnable(), "bf", "Cannot calculate beam forming at that time");
    int num_freqs = this->estimate_cov_matrix.size(); /* = N / 2 + 1 */
    if (num_freqs == 0)
        return;
    int M = this->array.size();
    std::vector<std::future<void>> futures;
    this->result_weight_vectors.resize(M, num_freqs);
    futures.reserve(num_freqs);
    for (int i = 0; i < num_freqs; i++)
    {
        if (i == 0)
        {
            this->result_weight_vectors.col(i) *= 0;
            continue;
        }
        else if (i == num_freqs - 1) /* for Nyquist frequency */
        {
            Eigen::Matrix<Eigen::dcomplex, -1, 1> ps = this->steering_vectors.col(i); /* ps */
            this->result_weight_vectors.col(i) = ps / M;
            this->result_weight_vectors.col(i).imag().setZero();
            continue;
        }
        futures.emplace_back(this->thread_pool->enqueue(
            [this, i]()
            { this->CalculateBeamformingForOneFreq(i); }));
    }
    for (auto &f : futures)
        f.get();
}
