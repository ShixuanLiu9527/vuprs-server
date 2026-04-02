/**
 * @brief   Beamforming algorithm.
 * @version 1.0
 * @author  Shixuan Liu, Tongji University
 * @date    2025-9
 */

#ifndef BEAM_FORMING_BASIC_H
#define BEAM_FORMING_BASIC_H

#include <string>
#include <Eigen/Dense>
#include <complex>
#include <fstream>
#include <stdio.h>
#include <math.h>
#include <mutex>
#include <atomic>

#include "nlohmann/json.hpp"

#include "string_parse.h"
#include "aligned_eigen_vector.h"
#include "fpga_data_conversion.h"
#include "signal_processing.h"
#include "beam_forming_algorithm.h"

#define VUPRS_EPS_1 1e-5

#define DEFAULT_SOUND_VELOCITY_MPS 346.0  /* (unit: m/s) NOTE: Default value, can only used for estimating steering vector error. */

namespace vuprs
{
    /**
     * @brief One beam forming element.
     * 
     * @note aligned.
     */
    class BeamFormingElement
    {
        private:

            vuprs::FFTWManagerComplex fftManager;

            void AddWindowForSignal();

        public:
            
            Eigen::Matrix<double, 3, 1> positionVector;  /* [x; y; z], relative to the reference point, unit: m */
            double timeDelay = 0.0;  /* time delay of signal, relative to the reference point, unit: sec */
            std::string adcChannel = "";  /* "" = empty */

            double samplingFrequency = 0.0;  /* sampling frequency for this signal, unit: Hz */
            double samplingTime = 0.0; /* sampling time for this signal, unit: sec */

            std::vector<std::complex<double>> elementSignalTimeDomain;  /* raw data */
            Eigen::Matrix<Eigen::dcomplex, -1, 1> windowedSignal_eigen;  /* windowed raw data */
            Eigen::Matrix<Eigen::dcomplex, -1, 1> elementSignalFrequencyDomain_eigen;  /* First half in frequency domain */
            
            BeamFormingElement();

            /**
             * @brief Calculate time delay for this element.
             * 
             * @note Initialize positionVector in advance.
             * 
             * @param targetAlt alt of the target position (relative to array), unit: deg.
             * @param targetAz az of the target position (relative to array), unit: deg.
             * @param waveVelocity velocity of wave, unit: m/sec.
             */
            void UpdataTimeDelay(double targetAlt, double targetAz, double waveVelocity); 

            /**
             * @brief FFT for the signal: this->elementSignalTimeDomain
             * 
             * @note this->elementSignalTimeDomain (Size: N) ---> DFT ---> this->elementSignalFrequencyDomain_eigen (size: (N/2+1, 1)).
             */
            bool RunDFT();

            bool empty() const;

            EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    };

    /**
     * @brief Beam forming array.
     * 
     * @note aligned.
     */
    class BeamFormingArray
    {
        private:

            std::unique_ptr<vuprs::ThreadPool> threadPool;
            double maxElementPositionError;

        public:

            double fs = 0.0;  /* sampling frequency for this signal, unit: Hz */
            double samplingTime = 0.0;  /* sampling time for this signal, unit: sec */
            int signalPointCounts = 0;  /* sampling points for this signal */
            Eigen::Matrix<Eigen::dcomplex, -1, 1> timeDelayVector;
            vuprs::AlignedEigenVector<vuprs::BeamFormingElement> elementArray;

            BeamFormingArray();

            ~BeamFormingArray();

            /**
             * @brief Load beam forming array from json file.
             * 
             * @throw std::runtime_error when error occurs.
             */
            bool LoadArrayFromJson(const std::string &filename);

            /**
             * @brief Calculate time delay for this array.
             * 
             * @note Initialize positionVector in advance.
             * 
             * @param targetAlt alt of the target position (relative to array), unit: deg.
             * @param targetAz az of the target position (relative to array), unit: deg.
             * @param waveVelocity velocity of wave, unit: m/sec.
             */
            void UpdateTimeDelay(double targetAlt, double targetAz, double waveVelocity); 

            /**
             * @brief Input all signal to the beam forming array and bind the signal to certain element.
             * 
             * @note index = 0: latest data;
             * @note index = data points: newest data.
             * 
             * @param adcData adc data
             */
            void InputElementSignal(const vuprs::SignalData &adcData);

            /**
             * @brief Calculate steering vector.
             * 
             * @note output = [exp(-jwT1), exp(-jwT2), ..., exp(-jwTM)].T
             * @note where: w = 2 * pi * f, f = frequency (unit: Hz).
             * @note        M = element counts.
             * @note Corresponding element of each rows: [element[0], element[1], ..., element[M]]
             * 
             * @param frequency signal frequency (unit: Hz), omega = 2 * pi * f.
             * 
             * @retval Steering vector.
             */
            Eigen::Matrix<Eigen::dcomplex, -1, 1> GetSteeringVector(double frequency) const;

            /**
             * @brief Calculate steering vector for total frequency domain.
             * 
             * @note output->col{i} = [exp(-jw{i}T{1}), exp(-jw{i}T{2}), ..., exp(-jw{i}T{M})].T
             * @note where: w = 2 * pi * f, f = frequency (unit: Hz).
             * @note        M = element counts.
             * @note        T{i} = time delay.
             * @note Corresponding element of each rows: [element[0], element[1], ..., element[M]]
             * 
             * @param matrix output matrix.
             * 
             * @retval matrix: [steering vector for f1, steering vector for f2, ...].
             */
            void GetSteeringVectorMatrix(Eigen::Matrix<Eigen::dcomplex, -1, -1> *matrix) const;

            /**
             * @brief Get array signal matrix (M x N), each column is a snapshot (M is element counts).
             * 
             * @note If FFT is used, the resulting data will be halved M x (N / 2 + 1).
             * @note If FFT not used, the resulting data will be M x N.
             * @note Corresponding element of each rows: [element[0], element[1], ..., element[M]].
             * 
             * @param signalMatrix [output] signal matrix.
             * @param samplingFrequency [output] sampling frequency.
             * @param frequencyDomain true: do FFT for all data, false: do not FFT.
             * 
             * @retval frequencyDomain = true: Array signal matrix (frequency domain, size = (M) x (N / 2 + 1)).
             * @retval frequencyDomain = false: Array signal matrix (time domain, size = (M)x(N))
             */
            void GetArraySignalMatrix(Eigen::Matrix<Eigen::dcomplex, -1, -1> *signalMatrix, double *samplingFrequency = nullptr, bool frequencyDomain = true);

            double GetMaxAbsoluteTimeDelay() const;

            /**
             * @brief Calculate steering vector error radius.
             * 
             * @param signalFrequency signal frequency.
             */
            double CalculateSteeringVectorErrorRadius(double signalFrequency) const;

            bool empty() const;

            EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    };
}

#endif
