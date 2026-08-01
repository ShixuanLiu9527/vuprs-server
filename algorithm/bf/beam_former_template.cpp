#include "config.h"
#include "algorithm/bf/beam_former_template.h"
#include "logger/check.h"

vuprs::WidebandBeamformerTemplate::WidebandBeamformerTemplate()
{
    this->thread_pool = std::make_unique<vuprs::ThreadPool>(std::thread::hardware_concurrency());
    this->ResetAll();
}

vuprs::WidebandBeamformerTemplate::~WidebandBeamformerTemplate()
{
    vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>>().swap(this->mean_cov_matrix);
    vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>>().swap(this->estimate_cov_matrix);
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

void vuprs::WidebandBeamformerTemplate::SetCovarianceMatrixFittingParam(int snaps_window_size, double adjacent_freq_average_index)
{
    this->covariance_snap_window_size = snaps_window_size;
    this->adjacent_freq_average_index = adjacent_freq_average_index;
    this->UpdateParameters();
}

void vuprs::WidebandBeamformerTemplate::UpdateParameters()
{
    this->exp_weighed_moving_average_index = 2.0 / double(this->covariance_snap_window_size + 1); /* N = 2/a - 1 */
    this->exp_weighed_moving_average_index_1 = 1.0 - this->exp_weighed_moving_average_index;      /* 1 - a */
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

bool vuprs::WidebandBeamformerTemplate::ScanForPositionPower(std::vector<double> *res,
                                                             double *max_value,
                                                             double *min_value,
                                                             const std::vector<double> &alt,
                                                             const std::vector<double> &az,
                                                             double wave_velocity,
                                                             bool need_regenerate, bool log)
{
    RUNTIME_CHECK(res, "bf", "Result pointer cannot be NULL");
    RUNTIME_CHECK(this->ConfigDone(), "bf", "Config not complete");
    RUNTIME_CHECK(alt.size() == az.size(), "bf", "Altitude and azimuth size mismatch");
    if (this->is_cov_matrix_empty)
    {
        return false;
    }

    /* STEP 0: Prepare for data */
    double M = this->array.size();                                                    /* Element counts */
    int k = alt.size();                                                               /* Scan points counts */
    int f_count = this->estimate_cov_matrix.size();                                   /* Frequency counts */
    Eigen::Matrix<double, 1, -1> total_power = Eigen::Matrix<double, 1, -1>::Zero(k); /* Total power for each scan position */
    if (need_regenerate)
    {
        std::lock_guard<std::mutex> lock(this->mut_scan);
        this->imag_timedelay = this->scan_array.GetImagTimedelay(alt, az, wave_velocity);
    }
    std::mutex mut_total; /* Mutex for total_power */
    std::vector<std::future<void>> futures;
    for (int i = 0; i < f_count; i++)
    {
        /* Calculate power (in each alt & az) for frequency index i */
        futures.emplace_back(this->thread_pool->enqueue(
            [this, i, M, &total_power, &mut_total]()
            {
                /* STEP 1: Get covariance matrix for the specified frequency */
                double f;
                Eigen::Matrix<Eigen::dcomplex, -1, -1> R;
                {
                    std::lock_guard<std::mutex> lock(this->mut);
                    f = this->signal_frequency_list(i);
                    R = this->estimate_cov_matrix[i];
                }
                /* STEP 2: Eigenvalue decomposition */
                Eigen::Matrix<Eigen::dcomplex, -1, -1> B; /* R = B * B.H */
                vuprs::CholeskyDecomposition(R, &B);
                /* STEP 3: Get steering vectors and weights for all alt & az in frequency i */
                Eigen::Matrix<Eigen::dcomplex, -1, -1> weights;
                {
                    std::lock_guard<std::mutex> lock(this->mut_scan);
                    weights = -2.0 * PI * f * this->imag_timedelay;
                }
                weights.array() = weights.array().exp(); /* ps = [..., exp(-j * 2 * pi * f * Tm), ...] */
                weights /= M;                            /* for CBF: w = ps/M */
                /* STEP 4: Calculate power for each alt & az */
                Eigen::Matrix<Eigen::dcomplex, -1, -1> Y = B.adjoint() * weights; /* Y = B.H * w */
                Eigen::Matrix<double, 1, -1> power = Y.colwise().squaredNorm();   /* power(i) = ||Y.col(i)||^2 */
                /* STEP 5: add to result */
                {
                    std::lock_guard<std::mutex> lock(mut_total);
                    total_power += power;
                }
            }));
    }

    for (auto &f : futures)
        f.get();

    /* Convert to result */
    double max_power = total_power.maxCoeff();
    double min_power = total_power.minCoeff();
    if (log)
    {
        if (min_power > 0.0)
        {
            total_power = total_power.array().log10() * 20.0; /* Convert to dB */
        }
        else
        {
            total_power.setZero();
        }
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
