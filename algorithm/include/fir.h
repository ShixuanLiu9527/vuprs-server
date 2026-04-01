#ifndef FIR_H
#define FIR_H

#include "beam_forming_algorithm.h"
#include "beam_forming_basic.h"

namespace vuprs
{
    class FIRCalculator
    {
        private:

            bool configdone;

            uint32_t firLength;
            uint32_t lastSignalPoints;  /* last N */
            double freqRange_l, freqRange_u;

            std::vector<std::vector<double>> firCoefficient;
            double maxAbsCoefficient;

            Eigen::Matrix<Eigen::dcomplex, -1, -1> matrixE;

        public:

            FIRCalculator();

            ~FIRCalculator();

            /**
             * @brief Configure FIR filter bank from JSON file.
             */
            bool ConfigFIRFromJsonFile(const std::string &jsonFilename);

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
             * @param response target response.
             * @param fs sampling frequency.
             * 
             * @throw std::runtime_error
             * 
             * @retval true: success.
             * @retval false: failed.
             */
            bool SolveCoeffUseExpectedFrequencyResponse(const Eigen::Matrix<Eigen::dcomplex, -1, -1> &response, double fs);

            /**
             * @brief Get FIR Filter Bank coefficients.
             */
            void GetFIRBankCoefficient(std::vector<std::vector<double>> *dst) const;

            /**
             * @brief Get FIR filter bank coefficients that are all zero.
             */
            void GetZeroFIRBankCoefficient(std::vector<std::vector<double>> *dst, uint32_t channelNumber) const;

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
