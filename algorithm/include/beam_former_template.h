#ifndef BEAM_FORMER_TEMPLATE_H
#define BEAM_FORMER_TEMPLATE_H

#include "beam_forming_basic.h"
#include "signal_processing.h"

namespace vuprs
{
    class BeamFormerTemplate
    {
        protected:

            vuprs::BeamFormingArray array;
            double samplingTime = 0.0, samplingFrequency = 0.0;
            int dataNumber = 0;

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
            void SetTargetDirection(const double &alt, const double az, const double waveVelocity);

            virtual void OutputSignal(std::vector<std::complex<double>> *outputSignal) = 0;
    };

    /**
     * @brief 2 * pi * f * j
     */
    Eigen::Matrix<Eigen::dcomplex, -1, 1> GenerateBeamFormingFrequencyList(const int &dataNumber, const double &samplingFrequency);
}

#endif
