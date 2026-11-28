#ifndef FIR_H
#define FIR_H

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "algo/dsp/dsp.h"
#include "misc/mt.h"

namespace vuprs
{
    class FIRCalculator
    {
    private:
        bool config_done;
        std::mutex mtx;
        uint32_t fir_length;
        uint32_t last_signal_points; /* last N */
        double freq_range_l, freq_range_u;
        std::vector<vuprs::FFTWManagerComplex> fft_managers; /* FFT manager */
        std::shared_ptr<vuprs::ThreadPool> thread_pool; /* Owned fallback; replaced by SetThreadPool() */
        std::vector<std::vector<double>> fir_coefficient;
        double max_abs_coefficient = 0.0;

    public:
        FIRCalculator();

        ~FIRCalculator();

        /**
         * @brief Configure FIR filter bank from JSON file.
         */
        bool ConfigFIRFromJsonFile(const std::string &json_filename);

        /**
         * @brief Replace the internal thread pool with a shared one.
         *
         * @note Used for the per-channel IFFT work. Must not be called while IFFT tasks are running.
         */
        void SetThreadPool(std::shared_ptr<vuprs::ThreadPool> pool);

        /**
         * @brief Set interest region for frequency.
         *
         * @note 0 < lo < hi < 0.5 * fs.
         *
         * @param lo lower boundary.
         * @param hi upper boundary.
         */
        void SetFrequencyRange(double lo, double hi);

        /**
         * @brief Solve FIR coefficient use expected frequency response.
         *
         * @note Target response size = M x (N/2+1), M = element size & FIR banks, N = signal points.
         * @note Response rows must be in ADC channel order (A1..A8, B1..B8); FIR bank i is
         * @note generated from response row i.
         *
         * @param response target response (N/2+1).
         * @param fs sampling frequency.
         *
         * @throw std::runtime_error
         *
         * @retval true: success.
         * @retval false: failed.
         */
        bool SolveCoeffUseExpectedFrequencyResponse(const Eigen::Matrix<Eigen::dcomplex, -1, -1> &response,
                                                    double fs);

        /**
         * @brief Get FIR Filter Bank coefficients.
         */
        void GetFIRBankCoefficient(std::vector<std::vector<double>> *dst) const;

        /**
         * @brief Get FIR filter bank coefficients that are all zero.
         */
        void GetZeroFIRBankCoefficient(std::vector<std::vector<double>> *dst, uint32_t channel_number) const;

        /**
         * @brief Get FIR filter length.
         */
        uint32_t FIRLength() const;

        /**
         * @brief Get maximum absolute coefficient of FIR filter bank.
         */
        double MaxAbsoluteFIRCoefficient() const;

        EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    };
}

#endif
