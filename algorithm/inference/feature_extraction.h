#ifndef FEATURE_EXTRACTION_H
#define FEATURE_EXTRACTION_H

#include <math.h>
#include <deque>
#include <Eigen/Dense>
#include <vector>
#include "algorithm/signal_processing/signal_processing.h"
#include "logger/log_manager.h"

namespace vuprs
{
    double f2mel(double f) { return 2595.0 * log10(1.0 + f / 700.0); }

    double mel2f(double mel) { return 700.0 * (pow(10.0, mel / 2595.0) - 1.0); }

    Eigen::Matrix<double, -1, 1> f2mel(const Eigen::Matrix<double, -1, 1> &f)
    {
        return 2595.0 * (f.array() / 700.0 + 1.0).log10();
    }

    Eigen::Matrix<double, -1, 1> mel2f(const Eigen::Matrix<double, -1, 1> &mel)
    {
        return (700.0 * ((mel.array() / 2595.0 * std::log(10.0)).exp() - 1.0)).matrix();
    }

    struct MelFilterDescriptor
    {
        double center_freq;
        double lower_freq;
        double upper_freq;
    };

    class MelFilterBank
    {
    private:
        bool filter_first_alloc, dct_matrix_first_alloc;
        double f_l;                                          /* lower boundary of frequency interval for mel-filters */
        double f_u;                                          /* upper boundary of frequency interval for mel-filters */
        double fs;                                           /* sampling frequency: Hz */
        uint32_t N;                                          /* sampling points: N */
        uint32_t K;                                          /* mel filter count */
        uint32_t L;                                          /* output MFCC dimension */
        Eigen::Matrix<double, -1, -1> filters;               /* K x (N / 2 + 1) matrix, each row of the matrix is a mel filter */
        Eigen::Matrix<double, -1, -1> dct_matrix;            /* (L + 1) x K, reserve for dct */
        std::vector<MelFilterDescriptor> filter_descriptors; /* K x 1, corrsponding to the filters */
        vuprs::FFTWManagerComplex fft_manager;

    public:
        MelFilterBank() : filter_first_alloc(true),
                          dct_matrix_first_alloc(true),
                          f_l(0.0),
                          f_u(0.0),
                          fs(1.0),
                          N(0),
                          K(0),
                          L(0) {};
        ~MelFilterBank() = default;

        /**
         * @brief Set and update parameters of mel filter bank.
         *
         * @note This operation will trigger memory alloc automatically.
         * @note K >= L.
         *
         * @param f_l lower boundary of frequency interval for mel-filters
         * @param f_u lower boundary of frequency interval for mel-filters
         * @param fs frequency threshold in Hz (<= fs/2).
         * @param N sampling points.
         * @param K filter bank count.
         * @param L MFCC output dimension.
         */
        void SetFilterParameters(double f_l,
                                 double f_u,
                                 double fs,
                                 uint32_t N,
                                 uint32_t K,
                                 uint32_t L);

        Eigen::Matrix<double, -1, -1> &filter() { return this->filters; }
        Eigen::Matrix<double, 1, -1> operator[](size_t idx) { return this->filters.row(idx); }

        /**
         * @brief Compute mel band energy.
         *
         * @note This is the intermediate step in calculating MFCC.
         * @note if \p log == false, output (K x 1) = filters (size K x (N / 2 + 1)) * signal (size = N / 2 + 1);
         * @note if \p log == true, output (K x 1) = log10(1.0 + filters * signal).
         *
         * @param signal Input signal in frequency domain, size = N / 2 + 1 (frequency in range 0 - fs / 2.0).
         * @param log log flag.
         *
         * @retval mel filter band energy of the signal.
         */
        Eigen::Matrix<double, -1, 1> ComputeBandEnergy(const Eigen::Matrix<Eigen::dcomplex, -1, 1> &signal, bool log = false) const;

        /**
         * @brief Compute MFCC.
         *
         * @note The 0th element of the output is the total energy (= sum(mel output)), witch can
         * @note be excluded through option \p include0.
         * @note If \p freq_domain == false, this method will apply hamming window
         * @note and FFT forward transform operation to \p signal.
         *
         * @param signal Input signal.
         *               If \p freq_domain == true: size = N / 2 + 1 (frequency in range 0 - fs / 2.0).
         *               If \p freq_domain == false: size = N (N points).
         * @param include0 Keep the 0th element.
         * @param freq_domain true: \p signal is in frequency domain.
         *                   false: \p signal is in time domain.
         * @param w_type Window type for FFT if \p freq_domain == false.
         *
         * @retval if \p include0 == true, the output is completed MFCC (size = L x 1).
         * @retval if \p include0 == false, the output part MFCC (element 0 is deleted, size = L x 1).
         */
        Eigen::Matrix<double, -1, 1> ComputeMFCC(const Eigen::Matrix<Eigen::dcomplex, -1, 1> &signal,
                                                 bool include0 = false,
                                                 bool freq_domain = true,
                                                 vuprs::WindowType w_type = vuprs::WindowType::SIG_WINDOW__HANN);

        EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    };

    /**
     * @brief Signal Extractor
     *
     * @note [signal] (InputSignal()) -> Extractor -> (GetMFCC()) [MFCC-quantized]
     */
    class SignalExtractor
    {
    private:
        bool first_alloc;
        bool flushed;
        bool signal_average_set;
        double signal_average;
        double frame_time_ms;                              /* time per frame (ms) */
        double f_l;                                        /* frequency range (lower) */
        double f_u;                                        /* frequency range (upper) */
        uint32_t N;                                        /* signal points per frame */
        uint32_t pool_size;                                /* pool size */
        uint32_t frames;                                   /* feature frames */
        uint32_t dims;                                     /* feature dims */
        uint32_t circular_ptr;                             /* alway point to newly data */
        uint32_t total_frames_processed;                   /* total frames written to pool */
        Eigen::Matrix<double, -1, -1> extract_tensor_pool; /* extract tensor pool (frame X dim) */
        MelFilterBank mel;

        void InputFrameSignal(const Eigen::Matrix<double, -1, 1> &frame_signal, double fs);

    public:
        SignalExtractor() : frames(0),
                            dims(0),
                            N(0),
                            circular_ptr(0),
                            total_frames_processed(0),
                            first_alloc(true),
                            flushed(false),
                            signal_average_set(false),
                            f_l(0.0),
                            f_u(0.0),
                            frame_time_ms(0.0),
                            signal_average(0.0) {};
        ~SignalExtractor() = default;

        SignalExtractor(const SignalExtractor &other) = delete;
        SignalExtractor &operator=(const SignalExtractor &other) = delete;
        SignalExtractor(SignalExtractor &&other) = delete;
        SignalExtractor &operator=(SignalExtractor &&other) = delete;

        /**
         * @brief Set parameters.
         *
         * @note The output tensor size is (dims x frames) in 2D:
         *       [mfcc(0,0),      mfcc(0,1),      ..., mfcc(0,frames-1),
         *        mfcc(1,0),      mfcc(1,1),      ..., mfcc(1,frames-1),
         *        ...             ...                  ...
         *        mfcc(dims-1,0), mfcc(dims-1,1), ..., mfcc(dims-1,frames-1)]
         *
         * @param dims Vector dim in output tensor.
         * @param frames Frames of MFCC.
         * @param frame_time_ms Time period of one frame.
         * @param f_l Lower boundary of frequency interval in MFCC.
         * @param f_u Lower boundary of frequency interval in MFCC.
         */
        void SetParameters(uint32_t dims, uint32_t frames, double frame_time_ms, double f_l, double f_u);

        /**
         * @brief Input signal.
         *
         * @param signal Time domain signal.
         * @param fs Sampling frequency for this signal.
         */
        void InputSignal(const Eigen::Matrix<double, -1, 1> &signal, double fs);

        /**
         * @brief Flushed flag.
         *
         * @retval true: flushed.
         * @retval false: Not flushed.
         */
        bool Flushed() const { return this->flushed; }

        /**
         * @brief Get eigenvalue tensor for the given signal.
         *
         * @note Check flushed flag from Flushed() before.
         *
         * @param tensor output tensor, size = (dims x frames)
         */
        void GetExtractTensor(Eigen::Matrix<uint8_t, -1, -1> *tensor) const;

        EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    };
}

#endif
