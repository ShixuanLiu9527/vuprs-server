#ifndef BEAM_FORMER_TEMPLATE_H
#define BEAM_FORMER_TEMPLATE_H

#include "beam_forming_basic.h"
#include "beam_forming_algorithm.h"

#define DEFAULT_COVARIANCE_SNAP_WINDOW_SIZE  100
#define DEFAULT_ADJACENT_FREQ_AVERAGE_INDEX  0.8

namespace vuprs
{
    class WidebandBeamformerTemplate
    {
        private:

            std::unique_ptr<ThreadPool> threadPool;

            int COVARIANCE_SNAP_WINDOW_SIZE;
            double ADJACENT_FREQ_AVERAGE_INDEX;

            double EXP_WEIGHTED_MOVING_AVERAGE_INDEX;
            double EXP_WEIGHTED_MOVING_AVERAGE_INDEX_1;

            double ADJACENT_FREQ_AVERAGE_INDEX_1;

            void UpdateParameters();

        protected:

            bool firstSnapshot;

            bool is_arrayConfigDown, is_beamfomerConfigDown;
            bool is_signalEmpty, is_covMatrixEmpty;

            vuprs::BeamFormingArray array;

            Eigen::Matrix<Eigen::dcomplex, -1, -1> snap_signalMatrix_freqDomain;  /* Size: (M) x (N / 2 + 1) */
            Eigen::Matrix<Eigen::dcomplex, -1, -1> steeringVectors;  /* Size: (M) x (N / 2 + 1) */
            Eigen::Matrix<Eigen::dcomplex, -1, -1> resultWeightVectors;  /* Size: (M) x (N / 2 + 1) */

            double fs;  /* Sampling frequency */

            std::vector<int> elementPredelayCount;  /* Predelay count */
            std::vector<double> elementPredelay;  /* Predelay time = count * Ts */
            std::vector<std::string> elementChannelName;  /* element channel name list */

            vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>> mean_covMatrix;  /* Size: N / 2 + 1, covMatrix[i] is the mean cov matrix in band [i] */
            vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>> estimate_covMatrix;  /* Size: N / 2 + 1, covMatrix[i] is the mean cov matrix in band [i] */

            /**
             * @brief Calculate beamforming for one frequency.
             * 
             * @note for frequency index [i]: corresponding steering vector = steeringVectors[i];
             * @note                          corresponding covariance matrix = estimate_covMatrix[i];
             * @note                          result weight vector = resultWeightVectors.col(i);
             * 
             * @param freqIndex frequency index
             */
            virtual void CalculateBeamformingForOneFreq(int freqIndex) = 0;

        public:

            WidebandBeamformerTemplate();

            virtual ~WidebandBeamformerTemplate() = default;

            /* STEP 1: CONFIG */

            /**
             * @brief Config beam forming array from JSON file.
             * 
             * @note Check ConfigDown() in advance.
             * 
             * @param arrayConfigJsonFilename JSON file name.
             */
            bool ConfigArrayFromJson(const std::string &arrayConfigJsonFilename);

            /* STEP 2: INPUT SIGNAL */

            /**
             * @brief Input signal.
             * 
             * @note Will not update covariance matrix.
             */
            void InputSignal(const vuprs::SignalData &signal);

            /* STEP 3: UPDATE COVARIANCE MATRIX */

            /**
             * @brief Update covariance matrix.
             */
            void UpdateCovarianceMatrix();

            /* STEP 4: SET TARGET DIRECTION */

            /**
             * @brief Set target direction.
             * 
             * @note Step 1: Update time delay.
             * @note Step 2: Update steering vector.
             */
            void SetTargetDirection(double alt, double az, double waveVelocity);

            /* STEP 5: DO BEAM FORMING CALCULATION */

            /**
             * @brief Do beam forming calculation.
             * 
             * @note Check CalculateEnable() in advance.
             */
            void CalculateBeamforming();

            /* STEP 6: GET RESULT */

            /**
             * @brief Get weight vector list for all frequency.
             * 
             * @param dst output weight vector list.
             */
            void GetWeightVectorValues(Eigen::Matrix<Eigen::dcomplex, -1, -1> *dst) const;

            /**
             * @brief Set covariance matrix fitting parameters.
             */
            void SetCovarianceMatrixFittingParam(int snapsWindowSize = 100, double adjacentFreqAverageIndex = 0.8);

            /**
             * @brief Reset covariance matrices.
             */
            void ResetCovarianceMatrices();

            /**
             * @brief Get element predelay parameters.
             * 
             * @note Check PredelayEnable() in advance.
             * 
             * @param firLength FIR filter bank length.
             * @param includeFIRGroupDelay true: include FIR group delay, false: exclude FIR group delay.
             * @param elementPredelayCount integer delay. (count = -round[delay{m}/Ts + (L-1)/2])
             * @param elementPredelay integer delay time. (Tm = -round[delay{m}/Ts + (L-1)/2] * Ts)
             * @param channelName corresponding channel name.
             */
            void GetElementPredelay(
                double firLength, bool includeFIRGroupDelay, 
                std::vector<int> *elementPredelayCount,
                std::vector<double> *elementPredelay,
                std::vector<std::string> *channelName);

            bool ConfigDown() const;
            bool CalculateEnable() const;
            bool PredelayEnable() const;
    };
}

#endif
