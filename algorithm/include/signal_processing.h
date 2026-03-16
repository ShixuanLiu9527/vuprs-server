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
     * @brief Convert Eigen column vector to std::vector.
     */
    template <typename T>
    void eigenVector2stdVector(const Eigen::Matrix<T, -1, 1> &eigenvector, std::vector<T> *stdvector)
    {
        if (eigenvector.rows() == 0) 
        {
            stdvector->clear();
            return;
        }
        if (eigenvector.cols() != 1)
        {
            throw std::runtime_error("Invalid shape of eigen (expected: Nx1)");
        }
        stdvector->assign(eigenvector.data(), eigenvector.data() + eigenvector.size());
    }

    /**
     * @brief Convert Eigen row vector to std::vector.
     */
    template <typename T>
    void eigenRow2stdVector(const Eigen::Matrix<T, 1, -1> &eigenrow, std::vector<T> *stdvector)
    {
        if (eigenrow.cols() == 0) 
        {
            stdvector->clear();
            return;
        }
        if (eigenrow.rows() != 1)
        {
            throw std::runtime_error("Invalid shape of eigen (expected: 1xN)");
        }
        
        stdvector->assign(eigenrow.data(), eigenrow.data() + eigenrow.size());
    }

    /**
     * @brief Convert std::vector to Eigen column vector.
     */
    template <typename T>
    void stdVector2eigenVector(const std::vector<T> &stdvector, Eigen::Matrix<T, -1, 1> *eigenvector)
    {
        if (stdvector.empty()) 
        {
            eigenvector->resize(0);
            return;
        }
        *eigenvector = Eigen::Map<const Eigen::Matrix<T, -1, 1>>(stdvector.data(), stdvector.size());
    }

    /**
     * @brief Convert std::vector to Eigen row vector.
     */
    template <typename T>
    void stdVector2eigenRow(const std::vector<T> &stdvector, Eigen::Matrix<T, 1, -1> *eigenrow)
    {
        if (stdvector.empty()) 
        {
            eigenrow->resize(1, 0);
            return;
        }
        *eigenrow = Eigen::Map<const Eigen::Matrix<T, 1, -1>>(stdvector.data(), stdvector.size());
    }

    /**
     * @brief Do FFT.
     * 
     * @note Use FFTW3.
     * 
     * @param inputData the input data.
     * @param outputData the output data.
     * @param inverse true: IDFT, false: DFT.
     * 
     * @throw std::runtime_error when input data is empty.
     */
    void FFT(const std::vector<std::complex<double>> &inputData, std::vector<std::complex<double>> *outputData, bool inverse = false);

    /**
     * @brief Do FFT.
     * 
     * @note Use FFTW3.
     * 
     * @param inputData the input data.
     * @param outputData the output data.
     * @param inverse true: IDFT, false: DFT.
     * 
     * @throw std::runtime_error when input data is empty.
     */
    void FFT(const Eigen::Matrix<Eigen::dcomplex, -1, 1> &inputData, Eigen::Matrix<Eigen::dcomplex, -1, 1> *outputData, bool inverse = false);

    /**
     * @brief Cut half size (return size = N / 2 + 1).
     * 
     * @note input: [1, 2, 3, 4, 5, 6]
     * @note output: [1, 2, 3, 4]
     */
    void CutTheFirstHalf(std::vector<std::complex<double>> *inputData);

    /**
     * @brief In-place conjugate symmetric completion (for IDFT).
     * 
     * @note output.size = (input.size - 1) * 2
     * @note input: [1j, 2j, 3j, 4j]
     * @note output: [1j, 2j, 3j, 4j, -3j, -2j]
     * 
     * @param inputData input data.
     */
    void CompleteConjugateSymmetric(std::vector<std::complex<double>> *inputData);

    /**
     * @brief In-place conjugate symmetric completion (for IDFT).
     * 
     * @note output.size = (input.size - 1) * 2
     * @note input: [1j, 2j, 3j, 4j]
     * @note output: [1j, 2j, 3j, 4j, -3j, -2j]
     * 
     * @param inputData input data.
     */
    void CompleteConjugateSymmetric(Eigen::Matrix<Eigen::dcomplex, -1, 1> *inputData);

    /**
     * @brief In-place conjugate symmetric completion.
     * 
     * @note output.size = (input.size - 1) * 2
     * @note input: [1, 2, 3, 4]
     * @note output: [1, 2, 3, 4, 3, 2]
     * 
     * @param inputData input data.
     */
    void CompleteSymmetric(Eigen::Matrix<double, -1, 1> *inputData);

    /**
     * @brief Frequency vector.
     * 
     * @note [f_1 * j, f_2 * j, ..., f_F * j], F = dataNumber / 2 + 1
     * 
     * @param dataNumber total data number (input to FFT).
     * @param samplingFrequency sampling frequency, unit: Hz.
     */
    Eigen::Matrix<Eigen::dcomplex, -1, 1> GenerateComplexFrequencyList(int dataNumber, double samplingFrequency);

    /**
     * @brief Frequency vector.
     * 
     * @note [f_1, f_2, ..., f_F], F = dataNumber / 2 + 1
     * 
     * @param dataNumber total data number (input to FFT).
     * @param samplingFrequency sampling frequency, unit: Hz.
     */
    Eigen::Matrix<double, -1, 1> GenerateRealFrequencyList(int dataNumber, double samplingFrequency);

    enum class WindowType 
    {
        SIG_WINDOW__HAMMING,
        SIG_WINDOW__HANN,
        SIG_WINDOW__BLACKMAN
    };

    Eigen::Matrix<double, -1, 1> GetWindow(vuprs::WindowType type, int signalLength);

    /**
     * @brief Add window for signal.
     * 
     * @param signal the given signal.
     */
    template <typename T>
    void AddWindow(Eigen::Matrix<T, -1, 1> *signal, vuprs::WindowType type = vuprs::WindowType::SIG_WINDOW__HAMMING)
    {
        if (signal == nullptr || signal->size() == 0)
        {
            return;
        }
        *signal = signal->cwiseProduct(vuprs::GetWindow(type, signal->rows()));
    }

    /**
     * @brief Add window for signal.
     * 
     * @note T: double, std::complex<double>
     * 
     * @param signal the given signal.
     */
    template <typename T>
    Eigen::Matrix<T, -1, 1> AddWindow(const Eigen::Matrix<T, -1, 1> &signal, vuprs::WindowType type = vuprs::WindowType::SIG_WINDOW__HAMMING)
    {
        if (signal.size() == 0)
        {
            throw std::runtime_error("Empty signal for window.");
        }
        return signal.cwiseProduct(vuprs::GetWindow(type, signal.rows()));
    }
}

#endif
