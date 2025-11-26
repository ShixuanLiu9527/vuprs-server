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
    void FFT(std::vector<std::complex<double>> *inputRealData, std::vector<std::complex<double>> *outputData, bool inverse = false);

    void CutTheFirstHalf(std::vector<std::complex<double>> *inputRealData);
}

#endif
