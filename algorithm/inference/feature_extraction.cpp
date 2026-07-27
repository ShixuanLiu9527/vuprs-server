#include <algorithm>
#include "algorithm/inference/feature_extraction.h"
#include "logger/log_manager.h"

vuprs::MelFilterBank::MelFilterBank()
{
    this->N = 0;
    this->K = 0;
    this->fs = 1;
    this->filterFirstAlloc = true;
    this->dctMatrixFirstAlloc = true;
}

vuprs::MelFilterBank::~MelFilterBank()
{
}

void vuprs::MelFilterBank::SetFilterParameters(double fs,
                                               uint32_t N,
                                               uint32_t K,
                                               uint32_t L)
{
    PARAM_CHECK(N > 0 && N % 2 == 0, "inference", "N must greater than zero and must be even.");
    PARAM_CHECK(K > 0, "inference", "K must greater than zero.");
    PARAM_CHECK(L > 0 && L <= K, "inference", "L must be in range (0, K].");
    PARAM_CHECK(fs > 0.0, "inference", "fs must greater than zero.");
    /* Part 1 - Mel-filter bank */
    bool filterAllocFlag = (std::abs(fs - this->fs) > 1e-5) ||
                           (N != this->N) ||
                           (K != this->K) ||
                           this->filterFirstAlloc;
    if (filterAllocFlag)
    {
        double maxFreq = fs / 2.0;
        double maxFreq_m = vuprs::f2mel(maxFreq);
        double interval_m = maxFreq_m / ((double)K + 1.0);
        /* Alloc mel filter parameters */
        this->filterDescriptors.resize(K);
        this->filters.resize(K, N / 2 + 1);
        this->filters.setZero();
        const double freq_per_bin = fs / (double)N;
        const int max_bin = N / 2;
        /* Generate mel filter bank */
        for (int i = 0; i < K; ++i)
        {
            double lower = vuprs::mel2f(interval_m * i);
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
        this->dctMatrix.resize(L, K);
        for (int i = 0; i < L; ++i)
        {
            for (int j = 0; j < K; ++j)
            {
                this->dctMatrix(i, j) = std::cos(PI * (double)i * ((double)j + 0.5) / K);
            }
        }
        this->dctMatrixFirstAlloc = false;
    }
    /* Update parameters */
    this->fs = fs;
    this->N = N;
    this->K = K;
    this->L = L;
}

Eigen::Matrix<double, -1, 1> vuprs::MelFilterBank::ComputeBandEnergy(const Eigen::Matrix<Eigen::dcomplex, -1, 1> &signal, bool log) const
{
    PARAM_CHECK(signal.rows() == this->filters.cols(), "inference", "Matrix shape mismatch");
    Eigen::Matrix<double, -1, 1> power = signal.array().abs2();
    Eigen::Matrix<double, -1, 1> energy = this->filters * power;
    if (log)
    {
        return (energy.array() + 1e-12).log10();
    }
    return energy;
}

Eigen::Matrix<double, -1, 1> vuprs::MelFilterBank::ComputeMFCC(const Eigen::Matrix<Eigen::dcomplex, -1, 1> &signal, bool include0) const
{
    PARAM_CHECK(signal.rows() == this->filters.cols(), "inference", "Matrix shape mismatch");
    Eigen::Matrix<double, -1, 1> energy = this->ComputeBandEnergy(signal, true); /* K x 1 */
    Eigen::Matrix<double, -1, 1> mfcc = this->dctMatrix * energy;
    if (include0)
        return mfcc;
    return mfcc.tail(this->L - 1);
}
