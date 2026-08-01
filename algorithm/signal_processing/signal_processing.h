#ifndef SIGNAL_PROCESSING_H
#define SIGNAL_PROCESSING_H

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <stdint.h>
#include <complex>
#include <cstring>
#include <Eigen/Dense>
#include <atomic>
#include <mutex>
#include "logger/check.h"
#include "3rdparty/fftw3/api/fftw3.h"

#define PI 3.14159265358979323846

namespace vuprs
{
    class FFTWManagerComplex
    {
    private:
        static std::mutex fftw_mtx; /* global lock */

        std::atomic<bool> fft_forward;        /* true: DFT backward, false: DFT forward */
        std::atomic<uint64_t> fft_points;     /* data size = N */
        fftw_complex *fft_input, *fft_output; /* input & output memory */
        fftw_plan fft_plan;                   /* FFT plan */
        fftw_plan ifft_plan;                  /* IFFT plan */

        void SetDFTDirection(bool forward = true);

        void SetDFTPoints(uint64_t N);

    public:
        FFTWManagerComplex();

        ~FFTWManagerComplex();

        FFTWManagerComplex(const FFTWManagerComplex &other) = delete;
        FFTWManagerComplex &operator=(const FFTWManagerComplex &other) = delete;

        FFTWManagerComplex(FFTWManagerComplex &&other) noexcept;
        FFTWManagerComplex &operator=(FFTWManagerComplex &&other) noexcept;

        /**
         * @brief Set DFT manager parameters.
         *
         * @param points signal points for DFT.
         * @param forward true: time domain --> frequency domain, false: frequency domain --> time domain.
         */
        void SetParameters(uint64_t points, bool forward);

        /**
         * @brief Run DFT.
         *
         * @param input input data buffer.
         * @param output output data buffer.
         */
        void DoDFT(const void *input, void *output);
    };

    /**
     * @brief Convert Eigen column vector to std::vector.
     */
    template <typename T>
    void eigenVector2stdVector(const Eigen::Matrix<T, -1, 1> &eigen_vector, std::vector<T> *std_vector)
    {
        if (eigen_vector.rows() == 0)
        {
            std_vector->clear();
            return;
        }
        PARAM_CHECK(eigen_vector.cols() == 1, "signal_processing", " Invalid shape of eigen (expected: Nx1)");
        std_vector->assign(eigen_vector.data(), eigen_vector.data() + eigen_vector.size());
    }

    /**
     * @brief Convert Eigen row vector to std::vector.
     */
    template <typename T>
    void eigenRow2stdVector(const Eigen::Matrix<T, 1, -1> &eigen_row, std::vector<T> *std_vector)
    {
        if (eigen_row.cols() == 0)
        {
            std_vector->clear();
            return;
        }
        PARAM_CHECK(eigen_row.rows() == 1, "signal_processing", " Invalid shape of eigen (expected: 1xN)");

        std_vector->assign(eigen_row.data(), eigen_row.data() + eigen_row.size());
    }

    /**
     * @brief Convert std::vector to Eigen column vector.
     */
    template <typename T>
    void stdVector2eigenVector(const std::vector<T> &std_vector, Eigen::Matrix<T, -1, 1> *eigen_vector)
    {
        if (std_vector.empty())
        {
            eigen_vector->resize(0);
            return;
        }
        *eigen_vector = Eigen::Map<const Eigen::Matrix<T, -1, 1>>(std_vector.data(), std_vector.size());
    }

    /**
     * @brief Convert std::vector to Eigen row vector.
     */
    template <typename T>
    void stdVector2eigenRow(const std::vector<T> &std_vector, Eigen::Matrix<T, 1, -1> *eigen_row)
    {
        if (std_vector.empty())
        {
            eigen_row->resize(1, 0);
            return;
        }
        *eigen_row = Eigen::Map<const Eigen::Matrix<T, 1, -1>>(std_vector.data(), std_vector.size());
    }

    /**
     * @brief Do FFT.
     *
     * @note Use FFTW3.
     *
     * @param input_data the input data.
     * @param output_data the output data.
     * @param inverse true: IDFT, false: DFT.
     *
     * @throw std::runtime_error when input data is empty.
     */
    void FFT(const std::vector<std::complex<double>> &input_data, std::vector<std::complex<double>> *output_data, bool inverse = false);

    /**
     * @brief Do FFT.
     *
     * @note Use FFTW3.
     *
     * @param input_data the input data.
     * @param output_data the output data.
     * @param inverse true: IDFT, false: DFT.
     *
     * @throw std::runtime_error when input data is empty.
     */
    void FFT(const Eigen::Matrix<Eigen::dcomplex, -1, 1> &input_data, Eigen::Matrix<Eigen::dcomplex, -1, 1> *output_data, bool inverse = false);

    /**
     * @brief Cut half size (return size = N / 2 + 1).
     *
     * @note input: [1, 2, 3, 4, 5, 6]
     * @note output: [1, 2, 3, 4]
     */
    void CutTheFirstHalf(std::vector<std::complex<double>> *input_data);

    /**
     * @brief Cut half size (return size = N / 2 + 1).
     *
     * @note input: [1, 2, 3, 4, 5, 6]
     * @note output: [1, 2, 3, 4]
     */
    void CutTheFirstHalf(Eigen::Matrix<Eigen::dcomplex, -1, 1> *input_data);

    /**
     * @brief In-place conjugate symmetric completion (for IDFT).
     *
     * @note output.size = (input.size - 1) * 2
     * @note input: [1j, 2j, 3j, 4j]
     * @note output: [1j, 2j, 3j, 4j, -3j, -2j]
     *
     * @param input_data input data.
     */
    void CompleteConjugateSymmetric(std::vector<std::complex<double>> *input_data);

    /**
     * @brief In-place conjugate symmetric completion (for IDFT).
     *
     * @note output.size = (input.size - 1) * 2
     * @note input: [1j, 2j, 3j, 4j]
     * @note output: [1j, 2j, 3j, 4j, -3j, -2j]
     *
     * @param input_data input data.
     */
    void CompleteConjugateSymmetric(Eigen::Matrix<Eigen::dcomplex, -1, 1> *input_data);

    /**
     * @brief In-place conjugate symmetric completion.
     *
     * @note output.size = (input.size - 1) * 2
     * @note input: [1, 2, 3, 4]
     * @note output: [1, 2, 3, 4, 3, 2]
     *
     * @param input_data input data.
     */
    void CompleteSymmetric(Eigen::Matrix<double, -1, 1> *input_data);

    /**
     * @brief Frequency vector.
     *
     * @note [f_1 * j, f_2 * j, ..., f_F * j], F = data_number / 2 + 1
     *
     * @param data_number total data number (input to FFT).
     * @param fs sampling frequency, unit: Hz.
     */
    Eigen::Matrix<Eigen::dcomplex, -1, 1> GenerateComplexFrequencyList(int data_number, double fs);

    /**
     * @brief Frequency vector.
     *
     * @note [f_1, f_2, ..., f_F], F = data_number / 2 + 1
     *
     * @param data_number total data number (input to FFT).
     * @param fs sampling frequency, unit: Hz.
     */
    Eigen::Matrix<double, -1, 1> GenerateRealFrequencyList(int data_number, double fs);

    enum class WindowType
    {
        SIG_WINDOW__HAMMING,
        SIG_WINDOW__HANN,
        SIG_WINDOW__BLACKMAN
    };

    Eigen::Matrix<double, -1, 1> GetWindow(vuprs::WindowType type, int signal_length);

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
        PARAM_CHECK(signal.size() > 0, "signal_processing", " Empty signal for window.");
        return signal.cwiseProduct(vuprs::GetWindow(type, signal.rows()));
    }

    template <typename T>
    void ifftshift(std::vector<T> *vec)
    {
        if (vec == nullptr || vec->size() <= 1)
            return;
        size_t N = vec->size();
        size_t shift = N / 2;
        std::rotate(vec->begin(), vec->begin() + shift, vec->end());
    }

    template <typename T>
    void ifftshift(Eigen::Matrix<T, -1, 1> *vec)
    {
        if (vec == nullptr || vec->size() <= 1)
            return;
        Eigen::Index N = vec->size();
        Eigen::Index shift = N / 2;
        Eigen::Matrix<T, -1, 1> temp(N);
        temp.head(N - shift) = vec->tail(N - shift);
        temp.tail(shift) = vec->head(shift);
        *vec = temp;
    }

    void ApplyBandpassWindow(Eigen::Matrix<Eigen::dcomplex, -1, 1> *Hd,
                             double f_low,
                             double f_high,
                             double fs,
                             int N,
                             double trans_width = -1.0);
}

#endif
