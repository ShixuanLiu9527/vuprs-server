#include <algorithm>
#include "algorithm/inference/feature_extraction.h"
#include "logger/log_manager.h"

void vuprs::MelFilterBank::SetFilterParameters(double f_l,
                                               double f_u,
                                               double fs,
                                               uint32_t N,
                                               uint32_t K,
                                               uint32_t L)
{
    PARAM_CHECK(N > 0 && N % 2 == 0, "inference", "N must greater than zero and must be even.");
    PARAM_CHECK(K > 0, "inference", "K must greater than zero.");
    PARAM_CHECK(L > 0 && L <= K, "inference", "L must be in range (0, K].");
    PARAM_CHECK(f_u > 0.0 && f_l > 0.0 && fs > 0.0, "inference", "frequency threshold and fs must greater than zero.");
    PARAM_CHECK(f_u > f_l, "inference", "");
    PARAM_CHECK(f_u <= fs / 2.0, "inference", "frequency threshold must be in range (0, nyquist freq).");
    /* Part 1 - Mel-filter bank */
    bool filter_alloc_flag = (std::abs(f_l - this->f_l) > 1e-5) ||
                             (std::abs(f_u - this->f_u) > 1e-5) ||
                             (N != this->N) ||
                             (K != this->K) ||
                             this->filter_first_alloc;
    if (filter_alloc_flag)
    {
        /* Split the frequency band use freqThreshold */
        double f_l_m = vuprs::f2mel(f_l);
        double f_u_m = vuprs::f2mel(f_u);
        double interval_m = (f_u_m - f_l_m) / ((double)K + 1.0);
        /* Alloc mel filter parameters */
        this->filter_descriptors.resize(K);
        this->filters.resize(K, N / 2 + 1);
        this->filters.setZero();
        /* Calculate point frequency interval use fs */
        const double freq_per_bin = fs / (double)N;
        const int max_bin = N / 2;
        /* Generate mel filter bank */
        for (int i = 0; i < K; ++i)
        {
            double lower = vuprs::mel2f(interval_m * i + f_l_m);
            double center = vuprs::mel2f(interval_m * (i + 1));
            double upper = vuprs::mel2f(interval_m * (i + 2));
            this->filter_descriptors[i] = {lower, center, upper};
            int idx_l = (int)std::floor(lower / freq_per_bin);
            int idx_c = (int)std::floor(center / freq_per_bin + 0.5);
            int idx_u = (int)std::ceil(upper / freq_per_bin);
            idx_l = std::max(0, std::min(idx_l, max_bin));
            idx_c = std::max(0, std::min(idx_c, max_bin));
            idx_u = std::max(0, std::min(idx_u, max_bin));
            if (idx_c - idx_l < 1 || idx_u - idx_c < 1)
                continue;
            for (int j = idx_l; j <= idx_c; ++j)
            {
                this->filters(i, j) = (double)(j - idx_l) / (double)(idx_c - idx_l);
            }
            for (int j = idx_c + 1; j <= idx_u; ++j)
            {
                this->filters(i, j) = (double)(idx_u - j) / (double)(idx_u - idx_c);
            }
            double area = this->filters.row(i).sum();
            if (area > 0.0)
                this->filters.row(i) /= area;
        }
        this->filter_first_alloc = false;
    }
    /* Part 2 - DCT matrix */
    bool dct_matrix_alloc_flag = (L != this->L) ||
                                 (K != this->K) ||
                                 this->dct_matrix_first_alloc;
    if (dct_matrix_alloc_flag)
    {
        this->dct_matrix.resize(L + 1, K);
        for (int i = 0; i < L + 1; ++i)
        {
            for (int j = 0; j < K; ++j)
            {
                this->dct_matrix(i, j) = std::cos(PI * (double)i * ((double)j + 0.5) / K);
            }
        }
        this->dct_matrix_first_alloc = false;
    }
    /* Realloc fftw plan */
    this->fft_manager.SetParameters(N, true);
    /* Update parameters */
    this->f_l = f_l;
    this->f_u = f_u;
    this->fs = fs;
    this->N = N;
    this->K = K;
    this->L = L;
}

Eigen::Matrix<double, -1, 1> vuprs::MelFilterBank::ComputeBandEnergy(const Eigen::Matrix<Eigen::dcomplex, -1, 1> &signal, bool log) const
{
    PARAM_CHECK(!this->filter_first_alloc, "inference", "Cannot compute band energy from empty filters.");
    PARAM_CHECK(signal.rows() == this->filters.cols(), "inference", "Matrix shape mismatch");
    Eigen::Matrix<double, -1, 1> power = signal.array().abs2();
    Eigen::Matrix<double, -1, 1> energy = this->filters * power;
    if (log)
    {
        return (energy.array() + 1e-12).log10();
    }
    return energy;
}

Eigen::Matrix<double, -1, 1> vuprs::MelFilterBank::ComputeMFCC(const Eigen::Matrix<Eigen::dcomplex, -1, 1> &signal,
                                                               bool include0,
                                                               bool freq_domain,
                                                               vuprs::WindowType w_type)
{
    Eigen::Matrix<double, -1, 1> energy;
    Eigen::Matrix<double, -1, 1> mfcc;
    if (freq_domain)
    {
        PARAM_CHECK(signal.rows() == this->filters.cols(), "inference", "Matrix shape mismatch");
        energy = this->ComputeBandEnergy(signal, true); /* K x 1 */
    }
    else
    {
        PARAM_CHECK(signal.rows() == this->N, "inference", "Signal shape mismatch");
        /* Apply window to signal */
        Eigen::dcomplex mean = signal.mean();
        /* - average */
        Eigen::Matrix<Eigen::dcomplex, -1, 1> windowed_signal = (signal.array() - mean).matrix();
        windowed_signal = vuprs::AddWindow(windowed_signal, w_type);
        Eigen::Matrix<Eigen::dcomplex, -1, 1> freq_signal(this->N);
        /* FFT */
        this->fft_manager.DoDFT(windowed_signal.data(), freq_signal.data());
        /* Cut first half (frequency in range 0 - fs/2) */
        vuprs::CutTheFirstHalf(&freq_signal);
        energy = this->ComputeBandEnergy(freq_signal, true);
    }
    mfcc = this->dct_matrix * energy;
    if (include0)
        return mfcc.head(this->L);
    return mfcc.tail(this->L);
}

void vuprs::SignalExtractor::SetParameters(uint32_t dims, uint32_t frames, double frame_time_ms, double f_l, double f_u)
{
    bool realloc_flag = (this->frames != frames) ||
                        (this->dims != dims) ||
                        (std::abs(this->frame_time_ms - frame_time_ms) > 1e-5) ||
                        (std::abs(this->f_l - f_l) > 1e-5) ||
                        (std::abs(this->f_u - f_u) > 1e-5) ||
                        this->first_alloc;
    if (realloc_flag)
    {
        this->pool_size = frames + 2;
        this->extract_tensor_pool.resize(dims, this->pool_size);
        this->circular_ptr = 0;
        this->total_frames_processed = 0;
        this->flushed = false;
        this->frames = frames;
        this->dims = dims;
        this->frame_time_ms = frame_time_ms;
        this->f_l = f_l;
        this->f_u = f_u;
    }
}

void vuprs::SignalExtractor::InputFrameSignal(const Eigen::Matrix<double, -1, 1> &frame_signal, double fs)
{
    uint32_t N = frame_signal.rows();
    Eigen::Matrix<Eigen::dcomplex, -1, 1> s_complex = (frame_signal.array() - this->signal_average).cast<std::complex<double>>();
    this->mel.SetFilterParameters(this->f_l,
                                  this->f_u,
                                  fs,
                                  N,
                                  this->dims,
                                  this->dims);
    Eigen::Matrix<double, -1, 1> mfcc = this->mel.ComputeMFCC(s_complex, /* complex signal (set imag part to 0) */
                                                              false,     /* do not include MFCC[0] */
                                                              false,     /* time domain */
                                                              vuprs::WindowType::SIG_WINDOW__HANN);
    this->extract_tensor_pool.col(this->circular_ptr) = mfcc;
    this->circular_ptr++;
    this->circular_ptr %= this->pool_size;
    this->total_frames_processed++;
}

void vuprs::SignalExtractor::InputSignal(const Eigen::Matrix<double, -1, 1> &signal, double fs)
{
    PARAM_CHECK(fs > 0.0, "inference", "fs must greater than 0.");
    uint32_t N_total = signal.rows();
    uint32_t N_half_frame = static_cast<uint32_t>(std::floor(0.5 * this->frame_time_ms * fs / 1000.0));
    uint32_t N_frame = 2 * N_half_frame;
    RUNTIME_CHECK(N_total >= N_frame, "inference", "No enough points for extract.");
    /* Slice the signal into frames (overlap) */
    if (!this->signal_average_set)
    {
        this->signal_average = signal.mean();
        this->signal_average_set = true;
    }
    else
    {
        this->signal_average = 0.6 * this->signal_average + 0.4 * signal.mean();
    }
    uint32_t frame_number = N_total / N_half_frame;
    for (uint32_t i = 0; i < frame_number; ++i)
    {
        Eigen::Matrix<double, -1, 1> segment_signal = signal.segment(i * N_half_frame, N_frame);
        this->InputFrameSignal(segment_signal, fs);
    }
    /* Mark as flushed once we have enough frames for a full tensor extraction */
    if (this->total_frames_processed >= this->frames)
    {
        this->flushed = true;
    }
}

void vuprs::SignalExtractor::GetExtractTensor(Eigen::Matrix<uint8_t, -1, -1> *tensor) const
{
    RUNTIME_CHECK(this->Flushed(), "inference", "Cannot read tensor from extractor (not flushed).");
    tensor->resize(this->dims, this->frames);
    Eigen::Matrix<double, -1, -1> tensor_d;
    if (this->circular_ptr >= this->frames)
    {
        tensor_d = this->extract_tensor_pool.middleCols(this->circular_ptr - this->frames,
                                                        this->frames);
    }
    else
    {
        uint32_t r_start = this->pool_size + this->circular_ptr - this->frames;
        auto tensor_d_l = this->extract_tensor_pool.middleCols(0, this->circular_ptr);
        auto tensor_d_r = this->extract_tensor_pool.middleCols(r_start, this->frames - this->circular_ptr);
        tensor_d << tensor_d_l, tensor_d_r;
    }
    /* Quantization */
    double max_d = tensor_d.maxCoeff();
    double min_d = tensor_d.minCoeff();
    double interval_d = std::abs(max_d - min_d);
    if (interval_d < 1e-10)
    {
        tensor->setZero();
        return;
    }
    tensor_d = (tensor_d.array() - min_d + 1e-10) * ((255.0 - 1e-10) / interval_d);
    *tensor = tensor_d.cast<uint8_t>();
}
