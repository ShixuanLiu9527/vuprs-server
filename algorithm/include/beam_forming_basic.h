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
            
            Eigen::Matrix<double, 3, 1> positionVector;  /* Relative to the reference point, unit: m */
            double timeDelay = 0.0;  /* unit: sec */
            int adcChannel = 0;

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
        private:

            Eigen::Matrix<double, -1, -1> timeDelayVector;
            vuprs::AlignedEigenVector<vuprs::BeamFormingElement> array;

        public:

            BeamFormingArray() = default;

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
            void UpdataTimeDelay(const double &targetAlt, const double &targetAz, const double &waveVelocity); 

            /**
             * @brief Calculate Array response vector.
             * 
             * @param frequency frequency.
             * 
             * @retval Array response vector.
             */
            Eigen::Matrix<Eigen::dcomplex, -1, -1> ArrayResponseVector(const double &frequency) const;

            double MaxAbsoluteTimeDelay() const;

            EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    };

    inline double rad2deg(const double &rad) {return rad / PI * 180.0;}
    inline double deg2rad(const double &deg) {return deg / 180.0 * PI;}

    inline Eigen::Matrix<double, 3, 1> AltAz2PointingVector(const double &alt, const double &az)
    {
        Eigen::Matrix<double, 3, 1> vec;
        double calt = cos(vuprs::deg2rad(alt)), caz = cos(vuprs::deg2rad(az)), salt = sin(vuprs::deg2rad(alt)), saz = sin(vuprs::deg2rad(az));
        vec << calt * saz, calt * caz, salt;
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