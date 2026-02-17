#ifndef FPGA_DATA_CONVERT_H
#define FPGA_DATA_CONVERT_H

#include <stdint.h>
#include <cmath>
#include <vector>
#include <stdexcept>

/**
 * @note ADC data format: Q15
 * @note FIR Coefficients: Q31
 * @note Beam forming result: Q16.16
 */

namespace vuprs
{
    constexpr double Q15_FIXED_SCALE_16BIT = 32768.0;  /* 2^15 */
    constexpr double Q31_FIXED_SCALE_32BIT = 2147483648.0;  /* 2^31 */
    constexpr double Q16_FIXED_SCALE_32BIT = 65536.0;  /* 2^16 */
    
    constexpr double FPGA_Q15_MAX = 1.0 - 1.0 / vuprs::Q15_FIXED_SCALE_16BIT;  // 0.999969482421875
    constexpr double FPGA_Q15_MIN = -1.0;

    constexpr double FPGA_Q31_MAX = 1.0 - 1.0 / vuprs::Q31_FIXED_SCALE_32BIT;  // 0.9999999995343387
    constexpr double FPGA_Q31_MIN = -1.0;

    constexpr double FPGA_Q16_MAX = 32768.0 - 1.0 / vuprs::Q16_FIXED_SCALE_32BIT;  // 32767.9999847412109375
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
        if (VAL >= FPGA_Q31_MAX) return 0x7FFFFFFF;
        if (VAL <= FPGA_Q31_MIN) return 0x80000000;

        return static_cast<uint32_t>(static_cast<int32_t>(std::round(VAL * vuprs::Q31_FIXED_SCALE_32BIT)));
    }

    /**
     * @brief Convert uint32_t to double (Q31)
     */
    double inline Q31__UINT32_TO_DOUBLE(uint32_t CODE)
    {
        return static_cast<double>(static_cast<int32_t>(CODE)) / vuprs::Q31_FIXED_SCALE_32BIT;
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
        if (VAL >= FPGA_Q16_MAX) return 0x7FFFFFFF;
        if (VAL <= FPGA_Q16_MIN) return 0x80000000;

        return static_cast<uint32_t>(static_cast<int32_t>(std::round(VAL * vuprs::Q16_FIXED_SCALE_32BIT)));
    }

    /**
     * @brief Convert uint32_t to double (Q16.16)
     */
    double inline Q16__UINT32_TO_DOUBLE(uint32_t CODE)
    {
        return static_cast<double>(static_cast<int32_t>(CODE)) / vuprs::Q16_FIXED_SCALE_32BIT;
    }

    /**
     * @brief Convert FIR coefficient from double to Q31.
     * 
     * @note The input must be scaled in advance: coef <- coef / max(coef).
     * @note Make sure coefficients in range [FPGA_Q31_MIN, FPGA_Q31_MAX].
     * 
     * @param input_scaled scaled FIR coefficients.
     * @param output output Q31 coefficients (send to FPGA).
     * 
     * @throw std::runtime_error
     */
    void inline ScaledFIRCoefficient_DOUBLE_TO_Q31_UINT32(const std::vector<double> &input_scaled, std::vector<uint32_t> *output)
    {
        uint64_t coefficientSize = input_scaled.size();
        
        if (coefficientSize == 0)
        {
            throw std::runtime_error("Input FIR coefficient is empty.");
        }

        output->resize(coefficientSize);
        for (uint64_t i = 0; i < coefficientSize; i++)
        {
            (*output)[i] = Q31__DOUBLE_TO_UINT32(input_scaled[i]);
        }
    }

    /**
     * @brief Convert FIR coefficient from double to Q31.
     * 
     * @param input_scaled scaled FIR coefficients.
     * @param output output Q31 coefficients (send to FPGA).
     * @param maxAbsCoef max absolute coefficient (= max(abs(coef{i}))).
     * 
     * @throw std::runtime_error
     */
    void inline FIRCoefficient_DOUBLE_TO_Q31_UINT32(const std::vector<double> &input, std::vector<uint32_t> *output, double maxAbsCoef)
    {
        uint64_t coefficientSize = input.size();

        if (coefficientSize == 0)
        {
            throw std::runtime_error("Input FIR coefficient is empty.");
        }
        if (maxAbsCoef <= 0)
        {
            throw std::runtime_error("Max absolute coefficient <= 0.");
        }

        output->resize(coefficientSize);
        for (uint64_t i = 0; i < coefficientSize; i++)
        {
            (*output)[i] = Q31__DOUBLE_TO_UINT32(input[i] / maxAbsCoef);
        }
    }
}

#endif
