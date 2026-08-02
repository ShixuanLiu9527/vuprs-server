#ifndef FIR_H
#define FIR_H

#include "algorithm/bf/beam_forming_algorithm.h"
#include "algorithm/bf/beam_forming_basic.h"

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
        std::unique_ptr<vuprs::ThreadPool> thread_pool;
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
         * @brief Set interest region for frequency.
         *
         * @note 0 < lower < upper < 0.5 * fs.
         *
         * @param lower lower boundary.
         * @param upper upper boundary.
         */
        void SetFrequencyRange(double lower, double upper);

        /**
         * @brief Solve FIR coefficient use expected frequency response.
         *
         * @note Target response size = M x (N/2+1), M = element size & FIR banks, N = signal points.
         *
         * @param response target response (N/2+1).
         * @param channel_name corresponding channel name for [response].
         * @param fs sampling frequency.
         *
         * @throw std::runtime_error
         *
         * @retval true: success.
         * @retval false: failed.
         */
        bool SolveCoeffUseExpectedFrequencyResponse(const Eigen::Matrix<Eigen::dcomplex, -1, -1> &response,
                                                    const std::vector<std::string> &channel_name,
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
