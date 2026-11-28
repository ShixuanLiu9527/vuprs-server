#ifndef BEAM_FORMING_BASIC_H
#define BEAM_FORMING_BASIC_H

#include <memory>
#include <string>
#include <vector>
#include <Eigen/Dense>
#include <complex>
#include "algo/dsp/dsp.h"
#include "misc/mt.h"
#include "misc/aligned_vec.h"

#define VUPRS_EPS_1 1e-5

namespace vuprs
{
    class SignalData;

    /**
     * @brief One beam forming element.
     *
     * @note aligned.
     */
    class BeamFormingElement
    {
    private:
        vuprs::FFTWManagerComplex fft_manager;

        void AddWindowForSignal();

    public:
        Eigen::Matrix<double, 3, 1> position_vector; /* [x; y; z], relative to the reference point, unit: m */
        double time_delay = 0.0;                     /* time delay of signal, relative to the reference point, unit: sec */
        std::string adc_channel = "";                /* "" = empty */

        double fs = 0.0;            /* sampling frequency for this signal, unit: Hz */
        double sampling_time = 0.0; /* sampling time for this signal, unit: sec */

        std::vector<std::complex<double>> element_signal_time_domain;                /* raw data */
        Eigen::Matrix<Eigen::dcomplex, -1, 1> windowed_signal_eigen;                 /* windowed raw data */
        Eigen::Matrix<Eigen::dcomplex, -1, 1> element_signal_frequency_domain_eigen; /* First half in frequency domain */

        BeamFormingElement();

        /**
         * @brief Calculate time delay for this element.
         *
         * @note Initialize position_vector in advance.
         *
         * @param target_alt alt of the target position (relative to array), unit: deg.
         * @param target_az az of the target position (relative to array), unit: deg.
         * @param wave_velocity velocity of wave, unit: m/sec.
         */
        void UpdataTimeDelay(double target_alt, double target_az, double wave_velocity);

        /**
         * @brief FFT for the signal: this->element_signal_time_domain
         *
         * @note this->element_signal_time_domain (Size: N) ---> DFT ---> this->element_signal_frequency_domain_eigen (size: (N/2+1, 1)).
         */
        bool RunDFT();

        bool empty() const;

        EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    };

    /**
     * @brief Beam forming array base.
     *
     * @note Owns the element list and loads it from a JSON array config.
     * @note LoadArrayFromJson() sorts elements by ADC channel (A1..A8, B1..B8),
     * @note so element_array[i] always corresponds to ADC channel i.
     *
     * @note aligned.
     */
    class BeamFormingArrayBase
    {
    protected:
        vuprs::AlignedEigenVector<vuprs::BeamFormingElement> element_array;
        double max_element_position_error = 0.0; /* all elements share the same position error radius, unit: m */

    public:
        BeamFormingArrayBase() = default;

        virtual ~BeamFormingArrayBase() = 0; /* abstract base: instantiate a derived array instead */

        /**
         * @brief Load beam forming array from json file.
         *
         * @note Elements are sorted by ADC channel (A1..A8, B1..B8) after loading,
         * @note and the file must contain every ADC channel exactly once.
         *
         * @throw std::runtime_error when error occurs.
         */
        bool LoadArrayFromJson(const std::string &filename);

        bool empty() const;

        BeamFormingElement &operator[](size_t idx) { return this->element_array[idx]; }
        const BeamFormingElement &operator[](size_t idx) const { return this->element_array[idx]; }
        size_t size() const { return this->element_array.size(); }

        /**
         * @brief Calculate the real time delay matrix for arbitrary target directions.
         *
         * @note Used by scan: output->col{i} = [T{1}, T{2}, ..., T{M}].T,
         * @note where T{m} = pos{m} . pointing_vector / wave_velocity.
         *
         * @param alt altitude list (degree).
         * @param az azimuth list (degree).
         * @param wave_velocity velocity of wave, unit: m/sec.
         */
        Eigen::Matrix<double, -1, -1> GetTimeDelay(const std::vector<double> &alt,
                                                   const std::vector<double> &az,
                                                   double wave_velocity) const;

        EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    };

    /**
     * @brief Beam forming array.
     *
     * @note aligned.
     */
    class BeamFormingArray : public BeamFormingArrayBase
    {
    private:
        std::shared_ptr<vuprs::ThreadPool> thread_pool; /* Owned fallback; replaced by SetThreadPool() */
        Eigen::Matrix<Eigen::dcomplex, -1, 1> time_delay_vector;
        double fs = 0.0;             /* sampling frequency for this signal, unit: Hz */
        double sampling_time = 0.0;  /* sampling time for this signal, unit: sec */
        int signal_point_counts = 0; /* sampling points for this signal */
        double wave_velocity;        /* wave velocity of the last UpdateTimeDelay(), unit: m/s */

    public:
        BeamFormingArray();

        /**
         * @brief Replace the internal thread pool with a shared one.
         *
         * @note Used for the per-element FFT work. Must not be called while FFT tasks are running.
         */
        void SetThreadPool(std::shared_ptr<vuprs::ThreadPool> pool);

        /**
         * @brief Calculate time delay for this array.
         *
         * @note Initialize position_vector in advance.
         *
         * @param target_alt alt of the target position (relative to array), unit: deg.
         * @param target_az az of the target position (relative to array), unit: deg.
         * @param wave_velocity velocity of wave, unit: m/sec.
         */
        void UpdateTimeDelay(double target_alt, double target_az, double wave_velocity);

        /**
         * @brief Input all signal to the beam forming array and bind the signal to certain element.
         *
         * @note index = 0: latest data;
         * @note index = data points: newest data.
         *
         * @param adc_data adc data (channel buffers are moved out)
         */
        void InputElementSignal(vuprs::SignalData &&adc_data);

        /**
         * @brief Calculate steering vector.
         *
         * @note output = [exp(-jwT1), exp(-jwT2), ..., exp(-jwTM)].T
         * @note where: w = 2 * pi * f, f = frequency (unit: Hz).
         * @note        M = element counts.
         * @note Corresponding element of each rows: [element[0], element[1], ..., element[M]]
         *
         * @param frequency signal frequency (unit: Hz), omega = 2 * pi * f.
         *
         * @retval Steering vector.
         */
        Eigen::Matrix<Eigen::dcomplex, -1, 1> GetSteeringVector(double frequency) const;

        /**
         * @brief Calculate steering vector for total frequency domain.
         *
         * @note output->col{i} = [exp(-jw{i}T{1}), exp(-jw{i}T{2}), ..., exp(-jw{i}T{M})].T
         * @note where: w = 2 * pi * f, f = frequency (unit: Hz).
         * @note        M = element counts.
         * @note        T{i} = time delay.
         * @note Corresponding element of each rows: [element[0], element[1], ..., element[M]]
         *
         * @param matrix output matrix.
         *
         * @retval matrix: [steering vector for f1, steering vector for f2, ...].
         */
        void GetSteeringVectorMatrix(Eigen::Matrix<Eigen::dcomplex, -1, -1> *matrix) const;

        /**
         * @brief Get array signal matrix (M x N), each column is a snapshot (M is element counts).
         *
         * @note If FFT is used, the resulting data will be halved M x (N / 2 + 1).
         * @note If FFT not used, the resulting data will be M x N.
         * @note Corresponding element of each rows: [element[0], element[1], ..., element[M]].
         *
         * @param signal_matrix [output] signal matrix.
         * @param fs [output] sampling frequency.
         * @param frequency_domain true: do FFT for all data, false: do not FFT.
         *
         * @retval frequency_domain = true: Array signal matrix (frequency domain, size = (M) x (N / 2 + 1)).
         * @retval frequency_domain = false: Array signal matrix (time domain, size = (M)x(N))
         */
        void GetArraySignalMatrix(Eigen::Matrix<Eigen::dcomplex, -1, -1> *signal_matrix,
                                  double *fs = nullptr,
                                  bool frequency_domain = true);

        double GetMaxAbsoluteTimeDelay() const;

        /**
         * @brief Calculate steering vector error radius.
         *
         * @param signal_frequency signal frequency.
         */
        double CalculateSteeringVectorErrorRadius(double signal_frequency) const;

        EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    };

    bool SaveToCSV(const Eigen::Matrix<double, -1, 1> &data, const std::string &filename);
    bool SaveToCSV_complex(const Eigen::Matrix<Eigen::dcomplex, -1, 1> &data, const std::string &filename);
}

#endif
