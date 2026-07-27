#ifndef FEATURE_EXTRACTION_H
#define FEATURE_EXTRACTION_H

#include <math.h>
#include <Eigen/Dense>
#include <vector>
#include "algorithm/signal_processing/signal_processing.h"
#include "logger/log_manager.h"

namespace vuprs
{
    double f2mel(double f) { return 2595.0 * log10(1.0 + f / 700.0); }

    double mel2f(double mel) { return 700.0 * (pow(10.0, mel / 2595.0) - 1.0); }

    Eigen::Matrix<double, -1, 1> f2mel(const Eigen::Matrix<double, -1, 1> &f)
    {
        return 2595.0 * (f.array() / 700.0 + 1.0).log10();
    }

    Eigen::Matrix<double, -1, 1> mel2f(const Eigen::Matrix<double, -1, 1> &mel)
    {
        return (700.0 * ((mel.array() / 2595.0 * std::log(10.0)).exp() - 1.0)).matrix();
    }

    struct MelFilterDescriptor
    {
        double centerFreq;
        double lowerFreq;
        double upperFreq;
    };

    class MelFilterBank
    {
    private:
        bool filterFirstAlloc, dctMatrixFirstAlloc;
        double fs;                                          /* sampling frequency: Hz */
        uint32_t N;                                         /* sampling points: N */
        uint32_t K;                                         /* mel filter count */
        uint32_t L;                                         /* output MFCC dimension */
        Eigen::Matrix<double, -1, -1> filters;              /* K x (N / 2 + 1) matrix, each row of the matrix is a mel filter */
        Eigen::Matrix<double, -1, -1> dctMatrix;            /* L x K, reserve for dct */
        std::vector<MelFilterDescriptor> filterDescriptors; /* K x 1, corrsponding to the filters */

    public:
        MelFilterBank();
        ~MelFilterBank();

        /**
         * @brief Set and update parameters of mel filter bank.
         *
         * @note This operation will trigger memory alloc automatically.
         *
         * @param fs sampling frequency in Hz.
         * @param N sampling points.
         * @param K filter bank count.
         * @param L MFCC output dimension.
         */
        void SetFilterParameters(double fs,
                                 uint32_t N,
                                 uint32_t K,
                                 uint32_t L);

        Eigen::Matrix<double, -1, -1> &filter() { return this->filters; }
        Eigen::Matrix<double, 1, -1> operator[](size_t idx) { return this->filters.row(idx); }

        /**
         * @brief Compute mel band energy.
         *
         * @note This is the intermediate step in calculating MFCC.
         * @note if \p log == false, output = filters (size M x (N / 2 + 1)) * signal (size = N / 2 + 1);
         * @note if \p log == true, output = log10(1.0 + filters * signal).
         *
         * @param signal Input signal in frequency domain, size = N / 2 + 1 (frequency in range 0 - fs / 2.0).
         * @param log log flag.
         *
         * @retval mel filter band energy of the signal.
         */
        Eigen::Matrix<double, -1, 1> ComputeBandEnergy(const Eigen::Matrix<Eigen::dcomplex, -1, 1> &signal, bool log = false) const;

        /**
         * @brief Compute MFCC.
         *
         * @note The 0th element of the output is the total energy (= sum(mel output)), witch can
         * @note be excluded through option \p include0.
         *
         * @param signal Input signal in frequency domain, size = N / 2 + 1 (frequency in range 0 - fs / 2.0).
         * @param include0 Keep the 0th element.
         *
         * @retval if \p include0 == true, the output is completed MFCC (size = L x 1).
         * @retval if \p include0 == false, the output part MFCC (element 0 is deleted, size = (L-1) x 1).
         */
        Eigen::Matrix<double, -1, 1> ComputeMFCC(const Eigen::Matrix<Eigen::dcomplex, -1, 1> &signal, bool include0 = false) const;
    };
}

#endif
