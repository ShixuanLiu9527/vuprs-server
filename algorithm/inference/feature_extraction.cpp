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
    bool filterAllocFlag = (std::abs(f_l - this->f_l) > 1e-5) ||
                           (std::abs(f_u - this->f_u) > 1e-5) ||
                           (N != this->N) ||
                           (K != this->K) ||
                           this->filterFirstAlloc;
    if (filterAllocFlag)
    {
        /* Split the frequency band use freqThreshold */
        double f_l_m = vuprs::f2mel(f_l);
        double f_u_m = vuprs::f2mel(f_u);
        double interval_m = (f_u_m - f_l_m) / ((double)K + 1.0);
        /* Alloc mel filter parameters */
        this->filterDescriptors.resize(K);
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
            this->filterDescriptors[i] = {lower, center, upper};
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
        this->filterFirstAlloc = false;
    }
    /* Part 2 - DCT matrix */
    bool dctMatrixAllocFlag = (L != this->L) ||
                              (K != this->K) ||
                              this->dctMatrixFirstAlloc;
    if (dctMatrixAllocFlag)
    {
        this->dctMatrix.resize(L + 1, K);
        for (int i = 0; i < L + 1; ++i)
        {
            for (int j = 0; j < K; ++j)
            {
                this->dctMatrix(i, j) = std::cos(PI * (double)i * ((double)j + 0.5) / K);
            }
        }
        this->dctMatrixFirstAlloc = false;
    }
    /* Realloc fftw plan */
    this->fftManager.SetParameters(N, true);
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
    PARAM_CHECK(!this->filterFirstAlloc, "inference", "Cannot compute band energy from empty filters.");
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
                                                               bool freqDomain,
                                                               vuprs::WindowType wType)
{
    Eigen::Matrix<double, -1, 1> energy;
    Eigen::Matrix<double, -1, 1> mfcc;
    if (freqDomain)
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
        Eigen::Matrix<Eigen::dcomplex, -1, 1> windowedSignal = (signal.array() - mean).matrix();
        windowedSignal = vuprs::AddWindow(windowedSignal, wType);
        Eigen::Matrix<Eigen::dcomplex, -1, 1> freqSignal(this->N);
        /* FFT */
        this->fftManager.DoDFT(windowedSignal.data(), freqSignal.data());
        /* Cut first half (frequency in range 0 - fs/2) */
        vuprs::CutTheFirstHalf(&freqSignal);
        energy = this->ComputeBandEnergy(freqSignal, true);
    }
    mfcc = this->dctMatrix * energy;
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
                        this->firstAlloc;
    if (realloc_flag)
    {
        this->pool_size = frames + 2;
        this->extractTensorPool.resize(dims, this->pool_size);
        this->circularPtr = 0;
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
    this->extractTensorPool.col(this->circularPtr) = mfcc;
    this->circularPtr++;
    this->circularPtr %= this->pool_size;
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
        Eigen::Matrix<double, -1, 1> segmentSignal = signal.segment(i * N_half_frame, N_frame);
        this->InputFrameSignal(segmentSignal, fs);
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
    if (this->circularPtr >= this->frames)
    {
        tensor_d = this->extractTensorPool.middleCols(this->circularPtr - this->frames,
                                                      this->frames);
    }
    else
    {
        uint32_t r_start = this->pool_size + this->circularPtr - this->frames;
        auto tensor_d_l = this->extractTensorPool.middleCols(0, this->circularPtr);
        auto tensor_d_r = this->extractTensorPool.middleCols(r_start, this->frames - this->circularPtr);
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
