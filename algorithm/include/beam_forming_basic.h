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

#define PI 3.14159265358979323846

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
            std::string adcChannel = "";

            std::vector<std::complex<double>> elementSignalTimeDomain, elementSignalFrequencyDomain;
            Eigen::Matrix<Eigen::dcomplex, -1, 1> phasedElementSignalFrequencyDomain;

            /**
             * @brief Calculate time delay for this element.
             * 
             * @note Initialize positionVector in advance.
             * 
             * @param targetAlt alt of the target position (relative to array), unit: deg.
             * @param targetAz az of the target position (relative to array), unit: deg.
             * @param waveVelocity velocity of wave, unit: m/sec.
             */
            void UpdataTimeDelay(const double &targetAlt, const double &targetAz, const double &waveVelocity); 

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

            Eigen::Matrix<double, -1, -1> timeDelayVector;
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
            void UpdateTimeDelay(const double &targetAlt, const double &targetAz, const double &waveVelocity); 

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
            Eigen::Matrix<Eigen::dcomplex, -1, -1> ArrayResponseVector(const double &frequency) const;

            double MaxAbsoluteTimeDelay() const;

            bool empty() const;

            EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    };

    inline double rad2deg(const double &rad) {return rad / PI * 180.0;}
    inline double deg2rad(const double &deg) {return deg / 180.0 * PI;}

    inline Eigen::Matrix<double, 3, 1> AltAz2PointingVector(const double &alt, const double &az)
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