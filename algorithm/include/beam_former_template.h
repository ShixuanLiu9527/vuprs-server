#ifndef BEAM_FORMER_TEMPLATE_H
#define BEAM_FORMER_TEMPLATE_H

#include "beam_forming_basic.h"
#include "signal_processing.h"

namespace vuprs
{
    /**
     * @brief Beam former template.
     * @note aligned.
     */
    class BeamFormerTemplate
    {
        protected:

            vuprs::BeamFormingArray array;

        public:

            BeamFormerTemplate();

            BeamFormerTemplate(const std::string &arrayConfigFile);

            virtual ~BeamFormerTemplate() = default;

            /**
             * @brief Config the beam forming array [STEP 1].
             * 
             * @param arrayConfigFile config json file.
             */
            void ConfigBeamFormingArray(const std::string &arrayConfigFile);

            /**
             * @brief Input signal of array [STEP 2].
             * 
             * @param signalData signal data.
             */
            void InputElementSignal(const vuprs::SignalData &signalData);

            /**
             * @brief Define the target direction [STEP 3].
             * 
             * @param alt altitude of target, relative to base array (unit: degrees).
             * @param az azimuth of target, relative to base array (unit: degrees).
             * @param waveVelocity velocity of wave (unit: m/sec).
             */
            void SetTargetDirection(double alt, double az, double waveVelocity);

            virtual void GetOutputSignal(std::vector<std::complex<double>> *outputSignal) = 0;

            EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    };

    /**
     * @brief Frequency vector.
     * 
     * @note [2pi * f_1 * j, 2pi * f_2 * j, ..., 2pi * f_F * j], F = dataNumber / 2 + 1
     * 
     * @param dataNumber total data number (input to FFT).
     * @param samplingFrequency sampling frequency, unit: Hz.
     */
    Eigen::Matrix<Eigen::dcomplex, -1, 1> GenerateBeamFormingFrequencyList(int dataNumber, double samplingFrequency);
}

#endif
