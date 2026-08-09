#ifndef FPGA_DATA_TYPE_TYPE_H
#define FPGA_DATA_TYPE_TYPE_H

#include <stdint.h>
#include <cmath>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <Eigen/Dense>
#include "logger/check.h"

/**
 * @note ADC data format: Q15
 * @note FIR Coefficients: Q31
 * @note Beam forming result: Q16.16
 * @note FIR scale: Q16.16
 */

namespace vuprs
{
    constexpr double FPGA_TYPE_EPS = 1e-18;
    constexpr double Q15_FIXED_SCALE_16BIT = 32768.0;      /* 2^15 */
    constexpr double Q31_FIXED_SCALE_32BIT = 2147483648.0; /* 2^31 */
    constexpr double Q16_FIXED_SCALE_32BIT = 65536.0;      /* 2^16 */

    constexpr double FPGA_Q15_MAX = 1.0 - 1.0 / vuprs::Q15_FIXED_SCALE_16BIT; // 0.999969482421875
    constexpr double FPGA_Q15_MIN = -1.0;

    constexpr double FPGA_Q31_MAX = 1.0 - 1.0 / vuprs::Q31_FIXED_SCALE_32BIT; // 0.9999999995343387
    constexpr double FPGA_Q31_MIN = -1.0;

    constexpr double FPGA_Q16_MAX = 32768.0 - 1.0 / vuprs::Q16_FIXED_SCALE_32BIT; // 32767.9999847412109375
    constexpr double FPGA_Q16_MIN = -32768.0;

    /* ------------------------------------ Q15 ----------------------------------- */

    /**
     * @brief Convert uint16_t to double (Q15)
     *
     * @note Q15 format: 1 sign + 15 fractional bits
     * @note Range: [Q15_MIN, Q15_MAX] = [-1.0, 0.999969482421875]
     * @note Scale: 2^15 = 32768.0
     */
    double inline Q15__UINT16_TO_DOUBLE(uint16_t CODE)
    {
        return static_cast<double>(static_cast<int16_t>(CODE)) / vuprs::Q15_FIXED_SCALE_16BIT;
    }

    /**
     * @brief Convert uint16_t to ADC double (Q15)
     *
     * @param CODE uint16_t ADC code
     * @param V_SCALE Reference voltage (e.g., 5.0 or 10.0 V for AD7606)
     */
    double inline Q15__ADC_UINT16_TO_DOUBLE(uint16_t CODE, double V_SCALE)
    {
        return Q15__UINT16_TO_DOUBLE(CODE) * V_SCALE;
    }

    /* ------------------------------------ Q31 ----------------------------------- */

    /**
     * @brief Convert double to uint32_t (Q31) with rounding
     *
     * @note Q31 format: 1 sign + 31 fractional bits
     * @note Range: [Q31_MIN, Q31_MAX] = [-1.0, 0.9999999995343387]
     * @note Scale: 2^31 = 2147483648.0
     */
    uint32_t inline Q31__DOUBLE_TO_UINT32(double VAL)
    {
        PARAM_CHECK(std::isfinite(VAL), "fpga", " Q31 conversion: input is NaN or Inf");
        if ((VAL + FPGA_TYPE_EPS) >= FPGA_Q31_MAX)
            return 0x7FFFFFFF;
        if ((VAL - FPGA_TYPE_EPS) <= FPGA_Q31_MIN)
            return 0x80000000;
        double _val = std::round(VAL * vuprs::Q31_FIXED_SCALE_32BIT);
        if ((_val + FPGA_TYPE_EPS) >= 2147483647.0)
            return 0x7FFFFFFF;
        if ((_val - FPGA_TYPE_EPS) <= -2147483648.0)
            return 0x80000000;
        return static_cast<uint32_t>(static_cast<int32_t>(_val));
    }

    /**
     * @brief Convert uint32_t to double (Q31)
     */
    double inline Q31__UINT32_TO_DOUBLE(uint32_t CODE)
    {
        return static_cast<double>(static_cast<int32_t>(CODE)) / vuprs::Q31_FIXED_SCALE_32BIT;
    }

    /**
     * @brief Convert double to uint32_t (Q31).
     *
     * @note The input must be scaled in advance: coef <- coef / max(coef).
     * @note Make sure data in range [FPGA_Q31_MIN, FPGA_Q31_MAX].
     *
     * @param input_scaled input data (double in range [FPGA_Q31_MIN, FPGA_Q31_MAX]).
     * @param output output data (Q31 format).
     *
     * @throw std::runtime_error
     */
    void inline Q31__DOUBLE_TO_UINT32(const std::vector<double> &input_scaled, std::vector<uint32_t> *output)
    {
        uint64_t coefficient_size = input_scaled.size();
        PARAM_CHECK(coefficient_size > 0, "fpga", " in " + std::string(__func__) + " Input FIR coefficient is empty.");
        output->resize(coefficient_size);
        for (uint64_t i = 0; i < coefficient_size; i++)
        {
            (*output)[i] = Q31__DOUBLE_TO_UINT32(input_scaled[i]);
        }
    }

    /**
     * @brief Convert double to uint32_t (Q31).
     *
     * @param input input data (double).
     * @param output output data (Q31).
     * @param max_abs max absolute value (= max(abs(input{i}))).
     *
     * @throw std::runtime_error
     */
    template <typename T_INPUT>
    void inline Q31__DOUBLE_TO_UINT32(const T_INPUT &input,
                                      std::vector<uint32_t> *output,
                                      double max_abs)
    {
        static_assert(std::is_same_v(T_INPUT, Eigen::Matrix<double, -1, 1>) ||
                          std::is_same_v(T_INPUT, Eigen::Matrix<float, -1, 1>) ||
                          std::is_same_v(T_INPUT, std::vector<double>) ||
                          std::is_same_v(T_INPUT, std::vector<float>),
                      "invalid type.");
        uint64_t data_number = input.size();
        PARAM_CHECK(data_number > 0, "fpga", " in [Q31__DOUBLE_TO_UINT32] Input vector is empty.");
        PARAM_CHECK(max_abs >= 0.0, "fpga", "max absolute value must >= 0.0");
        output->resize(data_number);
        if (std::abs(max_abs - 0.0) < FPGA_TYPE_EPS)
        {
            memset(output->data(), 0, data_number * sizeof(uint32_t));
            return;
        }
        if constexpr (std::is_same_v(T_INPUT, Eigen::Matrix<double, -1, 1>) ||
                      std::is_same_v(T_INPUT, Eigen::Matrix<float, -1, 1>))
        {
            for (uint64_t i = 0; i < data_number; i++)
                (*output)[i] = Q31__DOUBLE_TO_UINT32((double)input(i) / max_abs);
        }
        else if constexpr (std::is_same_v(T_INPUT, std::vector<double>) ||
                           std::is_same_v(T_INPUT, std::vector<float>))
        {
            for (uint64_t i = 0; i < data_number; i++)
                (*output)[i] = Q31__DOUBLE_TO_UINT32((double)input[i] / max_abs);
        }
    }

    /* ------------------------------------ Q16 ----------------------------------- */

    /**
     * @brief Convert double to uint32_t (Q16.16) with rounding
     *
     * @note Q16.16 format: 16 integer + 16 fractional bits
     * @note Range: [Q16_MIN, Q16_MAX] = [-32768.0, 32767.9999847412109375]
     * @note Scale: 2^16 = 65536.0
     */
    uint32_t inline Q16__DOUBLE_TO_UINT32(double VAL)
    {
        PARAM_CHECK(std::isfinite(VAL), "fpga", " Q16 conversion: input is NaN or Inf");
        if ((VAL + FPGA_TYPE_EPS) >= FPGA_Q16_MAX)
            return 0x7FFFFFFF;
        if ((VAL - FPGA_TYPE_EPS) <= FPGA_Q16_MIN)
            return 0x80000000;
        double _val = std::round(VAL * vuprs::Q16_FIXED_SCALE_32BIT);
        if ((_val + FPGA_TYPE_EPS) >= 2147483647.0)
            return 0x7FFFFFFF;
        if ((_val - FPGA_TYPE_EPS) <= -2147483648.0)
            return 0x80000000;
        return static_cast<uint32_t>(static_cast<int32_t>(_val));
    }

    /**
     * @brief Convert uint32_t to double (Q16.16)
     */
    double inline Q16__UINT32_TO_DOUBLE(uint32_t CODE)
    {
        return static_cast<double>(static_cast<int32_t>(CODE)) / vuprs::Q16_FIXED_SCALE_32BIT;
    }

    void inline Q16__UINT32_TO_DOUBLE(const std::vector<uint32_t> &input, std::vector<double> *output)
    {
        uint64_t _size = input.size();
        PARAM_CHECK(_size > 0, "fpga", " in [Q16__UINT32_TO_DOUBLE] Input FIR result is empty.");
        output->resize(_size);
        for (uint64_t i = 0; i < _size; i++)
        {
            (*output)[i] = Q16__UINT32_TO_DOUBLE(input[i]);
        }
    }
}

#endif
