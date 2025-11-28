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

#include "nlohmann/json.hpp"

#include "string_parse.h"
#include "aligned_eigen_vector.h"
#include "fpga_data_parse.h"
#include "signal_data.h"
#include "signal_processing.h"

#define PI 3.14159265358979323846
#define VUPRS_EPS_1 1e-5

namespace vuprs
{
    /**
     * @brief One beam forming element.
     * 
     * @note aligned.
     */
    class BeamFormingElement
    {
        public:
            
            Eigen::Matrix<double, 3, 1> positionVector;  /* [x; y; z], relative to the reference point, unit: m */
            double timeDelay = 0.0;  /* time delay of signal, relative to the reference point, unit: sec */
            std::string adcChannel = "";  /* "" = empty */

            double samplingFrequency = 0.0;  /* sampling frequency for this signal, unit: Hz */
            double samplingTime = 0.0; /* sampling time for this signal, unit: sec */

            std::vector<std::complex<double>> elementSignalTimeDomain;  /* raw data */
            std::vector<std::complex<double>> elementSignalFrequencyDomain_std;  /* fft data */
            Eigen::Matrix<Eigen::dcomplex, -1, 1> elementSignalFrequencyDomain_eigen;  /* First half in frequency domain */
            Eigen::Matrix<Eigen::dcomplex, -1, 1> phasedElementSignalFrequencyDomain_eigen;  /* Total data in frequency domain */

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
             * @note STEP 1: this->elementSignalTimeDomain ---> FFT ---> this->elementSignalFrequencyDomain_std;
             * @note STEP 2: this->elementSignalFrequencyDomain_std ---> Cut First Half ---> this->elementSignalFrequencyDomain_std;
             * @note STEP 3: this->elementSignalFrequencyDomain_eigen ---> Eigen::Map ---> this->elementSignalFrequencyDomain_eigen.
             */
            void DoFFT();

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
        public:

            double samplingFrequency = 0.0;  /* sampling frequency for this signal, unit: Hz */
            double samplingTime = 0.0;  /* sampling time for this signal, unit: sec */
            int signalPointCounts = 0;  /* sampling points for this signal */
            Eigen::Matrix<Eigen::dcomplex, -1, 1> timeDelayVector;
            vuprs::AlignedEigenVector<vuprs::BeamFormingElement> elementArray;

            BeamFormingArray();

            BeamFormingArray(const std::string &filename);

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
             * @param adcData adc data
             */
            void InputElementSignal(const vuprs::SignalData &adcData);

            /**
             * @brief Calculate Array response vector.
             * 
             * @param frequency frequency.
             * 
             * @retval Array response vector.
             */
            Eigen::Matrix<Eigen::dcomplex, -1, 1> ArrayResponseVector(double frequency) const;

            /**
             * @brief Get array signal matrix (M x N), each column is a snapshot.
             * 
             * @note If FFT is used, the resulting data will be halved (N / 2 + 1).
             * 
             * @param frequencyDomain true: do FFT for all data.
             *                        false: do not FFT.
             * @param samplingFrequency sampling frequency.
             * 
             * @retval frequencyDomain = true: Array signal matrix (frequency domain, size = (M) x (N / 2 + 1)).
             * @retval frequencyDomain = false: Array signal matrix (time domain, size = (M)x(N))
             */
            Eigen::Matrix<Eigen::dcomplex, -1, -1> ArraySignalMatrix(bool frequencyDomain = true, double *samplingFrequency = nullptr);

            double MaxAbsoluteTimeDelay() const;

            bool empty() const;

            EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    };

    template <typename T>
    void eigenVector2stdVector(const Eigen::Matrix<T, -1, 1> *eigenvector, std::vector<T> *stdvector)
    {
        if (eigenvector->rows() <= 0) return;
        if (eigenvector->cols() != 1) throw std::runtime_error("Invalid shape of eigen (valid: Nx1)");
        *stdvector = std::vector<T>(eigenvector->data(), eigenvector->data() + eigenvector->size());
    }

    template <typename T>
    void eigenRol2stdVector(const Eigen::Matrix<T, 1, -1> *eigenvector, std::vector<T> *stdvector)
    {
        if (eigenvector->cols() <= 0) return;
        if (eigenvector->rows() != 1) throw std::runtime_error("Invalid shape of eigen (valid: 1xN)");
        *stdvector = std::vector<T>(eigenvector->data(), eigenvector->data() + eigenvector->size());
    }

    template <typename T>
    void stdVector2eigenVector(std::vector<T> *stdvector, Eigen::Matrix<T, -1, 1> *eigenvector)
    {
        if (stdvector->empty()) return;
        *eigenvector = Eigen::Map<Eigen::Matrix<T, -1, 1>>(stdvector->data(), stdvector->size());
    }

    template <typename T>
    void stdVector2eigenRol(std::vector<T> *stdvector, Eigen::Matrix<T, 1, -1> *eigenvector)
    {
        if (stdvector->empty()) return;
        *eigenvector = Eigen::Map<Eigen::Matrix<T, 1, -1>>(stdvector->data(), stdvector->size());
    }

    inline double rad2deg(double rad) {return rad / PI * 180.0;}
    inline double deg2rad(double deg) {return deg / 180.0 * PI;}

    inline Eigen::Matrix<double, 3, 1> AltAz2PointingVector(double alt, double az)
    {
        Eigen::Matrix<double, 3, 1> vec;
        double c_alt = cos(vuprs::deg2rad(alt)), c_az = cos(vuprs::deg2rad(az)), \
               s_alt = sin(vuprs::deg2rad(alt)), s_az = sin(vuprs::deg2rad(az));
        vec << c_alt * s_az, c_alt * c_az, s_alt;
        return vec.normalized();
    }

    inline void PointingVector2AltAz(const Eigen::Matrix<double, 3, 1> &vec, double *alt, double *az)
    {
        double x = vec(0, 0), y = vec(1, 0), z = vec(2, 0);
        if (z > 1.0) z = 1.0;
        else if (z < -1.0) z = -1.0;
        *alt = vuprs::rad2deg(asin(z));
        *az = vuprs::rad2deg(atan2(x, y));
    }
}

#endif