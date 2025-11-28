#ifndef SIGNAL_PROCESSING_H
#define SIGNAL_PROCESSING_H

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <stdint.h>
#include <complex>
#include <cstring>
#include <Eigen/Dense>

#include "fftw3.h"  /* Use FFTW for processing */

namespace vuprs
{
    /**
     * @brief FFT.
     * 
     * @note Use FFTW3.
     * 
     * @param inputRealData the input data.
     * @param outputData the output data.
     * 
     * @throw std::runtime_error when input data is empty.
     */
    void FFT(const std::vector<std::complex<double>> *inputRealData, std::vector<std::complex<double>> *outputData, bool inverse = false);

    void CutTheFirstHalf(std::vector<std::complex<double>> *inputData);

    void SignalMontage(std::vector<std::complex<double>> *inputData);
    void SignalMontage(Eigen::Matrix<Eigen::dcomplex, -1, 1> *inputData);

    /**
     * @brief Frequency vector.
     * 
     * @note [f_1 * j, f_2 * j, ..., f_F * j], F = dataNumber / 2 + 1
     * 
     * @param dataNumber total data number (input to FFT).
     * @param samplingFrequency sampling frequency, unit: Hz.
     */
    Eigen::Matrix<Eigen::dcomplex, -1, 1> GenerateFrequencyList(int dataNumber, double samplingFrequency);
}

#endif
