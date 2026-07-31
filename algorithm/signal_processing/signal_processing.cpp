#include "algorithm/signal_processing/signal_processing.h"
#include "logger/log_manager.h"

std::mutex vuprs::FFTWManagerComplex::fftw_mtx;

vuprs::FFTWManagerComplex::FFTWManagerComplex()
{
    this->fft_points = 0;
    this->fft_forward = true; /* true: DFT backward, false: DFT forward */
    this->fft_input = nullptr;
    this->fft_output = nullptr; /* input & output memory */
    this->fft_plan = nullptr;   /* FFT plan */
}

vuprs::FFTWManagerComplex::~FFTWManagerComplex()
{
    std::unique_lock<std::mutex> lock(this->fftw_mtx); /* LOCK */
    if (this->fft_plan != nullptr)
    {
        fftw_destroy_plan(this->fft_plan);
        this->fft_plan = nullptr;
    }
    if (this->fft_input != nullptr)
    {
        fftw_free(this->fft_input);
        this->fft_input = nullptr;
    }
    if (this->fft_output != nullptr)
    {
        fftw_free(this->fft_output);
        this->fft_output = nullptr;
    }
}

vuprs::FFTWManagerComplex::FFTWManagerComplex(vuprs::FFTWManagerComplex &&other) noexcept
{
    this->fft_points = other.fft_points.load();
    this->fft_forward = other.fft_forward.load();
    this->fft_input = other.fft_input;
    this->fft_output = other.fft_output; /* input & output memory */
    this->fft_plan = other.fft_plan;     /* FFT plan */
    other.fft_points = 0;
    other.fft_forward = true;
    other.fft_input = nullptr;
    other.fft_output = nullptr;
    other.fft_plan = nullptr;
}

vuprs::FFTWManagerComplex &vuprs::FFTWManagerComplex::operator=(vuprs::FFTWManagerComplex &&other) noexcept
{
    if (this != &other)
    {
        std::unique_lock<std::mutex> lock(this->fftw_mtx); /* LOCK */

        if (this->fft_plan != nullptr)
        {
            fftw_destroy_plan(this->fft_plan);
        }
        if (this->fft_input != nullptr)
        {
            fftw_free(this->fft_input);
        }
        if (this->fft_output != nullptr)
        {
            fftw_free(this->fft_output);
        }

        this->fft_points = other.fft_points.load();
        this->fft_forward = other.fft_forward.load();
        this->fft_input = other.fft_input;
        this->fft_output = other.fft_output;
        this->fft_plan = other.fft_plan;

        other.fft_points = 0;
        other.fft_forward = true;
        other.fft_input = nullptr;
        other.fft_output = nullptr;
        other.fft_plan = nullptr;
    }
    return *this;
}

void vuprs::FFTWManagerComplex::SetDFTDirection(bool forward)
{
    if (this->fft_forward != forward)
    {
        std::unique_lock<std::mutex> lock(this->fftw_mtx); /* LOCK */
        PARAM_CHECK(this->fft_input != nullptr && this->fft_output != nullptr,
                    "signal_processing",
                    " in [FFTWManagerComplex::SetDFTDirection] fftw_input or fftw_output is NULL.");
        /* destroy */
        if (this->fft_plan != nullptr)
        {
            fftw_destroy_plan(this->fft_plan);
            this->fft_plan = nullptr;
        }
        /* generate plan */
        this->fft_plan = fftw_plan_dft_1d(this->fft_points,
                                          this->fft_input,
                                          this->fft_output,
                                          forward ? FFTW_FORWARD : FFTW_BACKWARD,
                                          FFTW_ESTIMATE);
        RUNTIME_CHECK(this->fft_plan != nullptr,
                      "signal_processing",
                      " in [FFTWManagerComplex::SetDFTDirection] fft_plan is NULL.");
        this->fft_forward = forward;
    }
}

void vuprs::FFTWManagerComplex::SetDFTPoints(uint64_t N)
{
    if (this->fft_points != N && N > 0)
    {
        std::unique_lock<std::mutex> lock(this->fftw_mtx); /* LOCK */

        /* free plan */
        if (this->fft_plan != nullptr)
        {
            fftw_destroy_plan(this->fft_plan);
            this->fft_plan = nullptr;
        }
        /* Free buffer */
        if (this->fft_input != nullptr)
        {
            fftw_free(this->fft_input);
            this->fft_input = nullptr;
        }
        if (this->fft_output != nullptr)
        {
            fftw_free(this->fft_output);
            this->fft_output = nullptr;
        }
        /* malloc */
        this->fft_input = reinterpret_cast<fftw_complex *>(fftw_malloc(sizeof(fftw_complex) * N));
        this->fft_output = reinterpret_cast<fftw_complex *>(fftw_malloc(sizeof(fftw_complex) * N));
        RUNTIME_CHECK(this->fft_input != nullptr && this->fft_output != nullptr, "signal_processing", " in [FFTWManagerComplex::SetDFTPoints] Failed to allocate FFTW memory.");
        /* plan */
        this->fft_plan = fftw_plan_dft_1d(N,
                                          this->fft_input,
                                          this->fft_output,
                                          this->fft_forward ? FFTW_FORWARD : FFTW_BACKWARD,
                                          FFTW_ESTIMATE);
        RUNTIME_CHECK(this->fft_plan != nullptr, "signal_processing", " in [FFTWManagerComplex::SetDFTPoints] Failed to allocate FFTW plan.");
        this->fft_points = N;
    }
}

void vuprs::FFTWManagerComplex::SetParameters(uint64_t points, bool forward)
{
    this->SetDFTPoints(points);
    this->SetDFTDirection(forward);
}

void vuprs::FFTWManagerComplex::DoDFT(const void *input, void *output)
{
    PARAM_CHECK(this->fft_plan != nullptr && this->fft_input != nullptr && this->fft_output != nullptr, "signal_processing", " in [FFTWManagerComplex::DoDFT] fft_plan or fft_input or fft_output is NULL.");
    std::memcpy(this->fft_input,
                input,
                this->fft_points * sizeof(std::complex<double>));
    fftw_execute(this->fft_plan);
    std::memcpy(output,
                this->fft_output,
                this->fft_points * sizeof(std::complex<double>));
}

void vuprs::FFT(const Eigen::Matrix<Eigen::dcomplex, -1, 1> &input_data,
                Eigen::Matrix<Eigen::dcomplex, -1, 1> *output_data,
                bool inverse)
{
    uint64_t data_size = input_data.rows();
    PARAM_CHECK(data_size > 0, "signal_processing", " in [vuprs::FFT] The input data is empty.");
    fftw_complex *input, *output;
    fftw_plan plan;
    input = reinterpret_cast<fftw_complex *>(fftw_malloc(sizeof(fftw_complex) * data_size));
    output = reinterpret_cast<fftw_complex *>(fftw_malloc(sizeof(fftw_complex) * data_size));
    if (input == nullptr || output == nullptr)
    {
        fftw_free(input);
        fftw_free(output);
        RUNTIME_CHECK(false,
                      "signal_processing",
                      " in [vuprs::FFT] Failed to allocate FFTW memory.");
    }
    plan = fftw_plan_dft_1d(data_size,
                            input,
                            output,
                            inverse ? FFTW_BACKWARD : FFTW_FORWARD,
                            FFTW_ESTIMATE);
    if (plan == nullptr)
    {
        fftw_free(input);
        fftw_free(output);
        RUNTIME_CHECK(false, "signal_processing", " in [vuprs::FFT] Failed to allocate FFTW plan.");
    }
    try
    {
        /* Input data to *input */
        std::memcpy(input,
                    input_data.data(),
                    data_size * sizeof(std::complex<double>));
        /* Do fft */
        fftw_execute(plan);
        output_data->resize(data_size, 1);
        std::memcpy(output_data->data(),
                    output,
                    data_size * sizeof(std::complex<double>));
        if (inverse)
        {
            (*output_data) = (*output_data) / (double)data_size;
        }
    }
    catch (const std::exception &e)
    {
        fftw_destroy_plan(plan);
        fftw_free(input);
        fftw_free(output);
        RUNTIME_CHECK(false, "signal_processing", " in [vuprs::FFT] Error occurred in FFT. (" + std::string(e.what()) + ")");
    }
    fftw_destroy_plan(plan);
    fftw_free(input);
    fftw_free(output);
}

void vuprs::FFT(const std::vector<std::complex<double>> &input_data, std::vector<std::complex<double>> *output_data, bool inverse)
{
    uint64_t data_size = input_data.size();
    PARAM_CHECK(data_size > 0, "signal_processing", " in [vuprs::FFT] The input data is empty.");
    fftw_complex *input, *output;
    fftw_plan plan;
    input = reinterpret_cast<fftw_complex *>(fftw_malloc(sizeof(fftw_complex) * data_size));
    output = reinterpret_cast<fftw_complex *>(fftw_malloc(sizeof(fftw_complex) * data_size));
    if (input == nullptr || output == nullptr)
    {
        fftw_free(input);
        fftw_free(output);
        RUNTIME_CHECK(false, "signal_processing", " in [vuprs::FFT] Failed to allocate FFTW memory.");
    }
    plan = fftw_plan_dft_1d(data_size,
                            input,
                            output,
                            inverse ? FFTW_BACKWARD : FFTW_FORWARD,
                            FFTW_ESTIMATE);
    if (plan == nullptr)
    {
        fftw_free(input);
        fftw_free(output);
        RUNTIME_CHECK(false, "signal_processing", " in [vuprs::FFT] Failed to allocate FFTW plan.");
    }
    try
    {
        /* Input data to *input */
        std::memcpy(input,
                    input_data.data(),
                    data_size * sizeof(std::complex<double>));
        /* Do fft */
        fftw_execute(plan);
        output_data->resize(data_size);
        std::memcpy(output_data->data(),
                    output,
                    data_size * sizeof(std::complex<double>));
        if (inverse)
        {
            for (uint64_t i = 0; i < data_size; i++)
            {
                (*output_data)[i] = (*output_data)[i] / (double)data_size;
            }
        }
    }
    catch (const std::exception &e)
    {
        fftw_destroy_plan(plan);
        fftw_free(input);
        fftw_free(output);
        RUNTIME_CHECK(false, "signal_processing", " in [vuprs::FFT] Error occurred in FFT. (" + std::string(e.what()) + ")");
    }
    fftw_destroy_plan(plan);
    fftw_free(input);
    fftw_free(output);
}

void vuprs::CutTheFirstHalf(std::vector<std::complex<double>> *input_data)
{
    int size = input_data->size();
    input_data->resize(size / 2 + 1);
}

void vuprs::CutTheFirstHalf(Eigen::Matrix<Eigen::dcomplex, -1, 1> *input_data)
{
    PARAM_CHECK(input_data != nullptr, "signal_processing", " in [vuprs::CutTheFirstHalf] Input data is null.");
    int size = input_data->rows();
    if (size <= 0)
        return;
    input_data->resize(size / 2 + 1);
}

void vuprs::CompleteConjugateSymmetric(std::vector<std::complex<double>> *input_data)
{
    uint64_t originSize = input_data->size();
    if (originSize <= 2)
    {
        return;
    }
    std::vector<std::complex<double>> backHalf = *input_data;
    /* reverse */
    std::reverse(backHalf.begin(), backHalf.end());
    /* erase N/2 and 0 */
    if (!backHalf.empty())
        backHalf.erase(backHalf.begin());
    if (!backHalf.empty())
        backHalf.pop_back();
    /* conjugate */
    for (auto &element : backHalf)
    {
        element = std::conj(element);
    }
    /* insert */
    input_data->insert(input_data->end(),
                       backHalf.begin(),
                       backHalf.end());
    (*input_data)[originSize - 1].imag(0.0);
    (*input_data)[0].imag(0.0);
}

void vuprs::CompleteConjugateSymmetric(Eigen::Matrix<Eigen::dcomplex, -1, 1> *input_data)
{
    int halfSize = input_data->rows(); /* N / 2 + 1 */
    if (halfSize <= 2)
        return;
    int fullSize = 2 * (halfSize - 1);
    input_data->conservativeResize(fullSize);
    for (int i = 1; i < halfSize - 1; i++)
    {
        (*input_data)(fullSize - i) = std::conj((*input_data)(i));
    }
    (*input_data)(halfSize - 1).imag(0.0); /* x(N/2).imag = 0 */
    (*input_data)(0).imag(0.0);            /* x(0).imag = 0 */
}

void vuprs::CompleteSymmetric(Eigen::Matrix<double, -1, 1> *input_data)
{
    int halfSize = input_data->rows(); /* N / 2 + 1 */
    if (halfSize <= 2)
        return;
    int fullSize = 2 * (halfSize - 1);
    Eigen::Matrix<double, -1, 1> original = *input_data;
    input_data->conservativeResize(fullSize);
    for (int i = 1; i < halfSize - 1; i++)
    {
        (*input_data)(fullSize - i) = original(i);
    }
}

Eigen::Matrix<Eigen::dcomplex, -1, 1> vuprs::GenerateComplexFrequencyList(int data_number, double fs)
{
    Eigen::Matrix<Eigen::dcomplex, -1, 1> retVector;
    int frequencyNumber = data_number / 2 + 1;
    retVector.resize(frequencyNumber, 1);
    retVector.setZero();
    for (int i = 0; i < frequencyNumber; i++)
    {
        retVector(i, 0).imag(double(i) / double(data_number));
    }
    retVector *= fs; /* f * j */
    return retVector;
}

Eigen::Matrix<double, -1, 1> vuprs::GenerateRealFrequencyList(int data_number, double fs)
{
    Eigen::Matrix<double, -1, 1> retVector;
    int frequencyNumber = data_number / 2 + 1;
    retVector.resize(frequencyNumber, 1);
    retVector.setZero();
    for (int i = 0; i < frequencyNumber; i++)
    {
        retVector(i, 0) = double(i) / double(data_number);
    }
    retVector *= fs; /* f * j */
    return retVector;
}

Eigen::Matrix<double, -1, 1> vuprs::GetWindow(vuprs::WindowType type, int signal_length)
{
    Eigen::Matrix<double, -1, 1> w(signal_length);

    switch (type)
    {
    case WindowType::SIG_WINDOW__HAMMING:
    {
        double alpha0 = 25.0 / 46.0;
        double alpha1 = 1.0 - alpha0;
        for (int i = 0; i < signal_length; i++)
        {
            w(i) = alpha0 - alpha1 * cos(2 * M_PI * i / (signal_length - 1));
        }
        break;
    }
    case WindowType::SIG_WINDOW__HANN:
    {
        for (int i = 0; i < signal_length; i++)
        {
            w(i) = 0.5 * (1 - cos(2 * M_PI * i / (signal_length - 1)));
        }
        break;
    }
    case WindowType::SIG_WINDOW__BLACKMAN:
    {
        double a0 = 0.42;
        double a1 = 0.5;
        double a2 = 0.08;
        for (int i = 0; i < signal_length; i++)
        {
            w(i) = a0 - a1 * cos(2 * M_PI * i / (signal_length - 1)) + a2 * cos(4 * M_PI * i / (signal_length - 1));
        }
        break;
    }
    default:
    {
        PARAM_CHECK(false, "signal_processing", " in [vuprs::GetWindow] Invalid widnow type.");
    }
    }
    return w;
}

void vuprs::ApplyBandpassWindow(Eigen::Matrix<Eigen::dcomplex, -1, 1> *Hd,
                                double f_low,
                                double f_high,
                                double fs,
                                int N,
                                double trans_width)
{
    int K = Hd->rows();
    if (K <= 1)
        return;
    double df = fs / (double)N;
    if (trans_width < 0)
        trans_width = 10.0 * df;
    if (f_low - trans_width < 0)
        trans_width = f_low;
    if (f_high + trans_width > fs / 2.0)
        trans_width = fs / 2.0 - f_high;
    if (trans_width < 1e-12)
        trans_width = 1e-12;
    for (int k = 1; k < K; ++k)
    {
        double f = k * df;
        double gain = 0.0;
        if (f < (f_low - trans_width))
        {
            gain = 0.0;
        }
        else if (f < f_low)
        {
            double t = (f - (f_low - trans_width)) / trans_width;
            gain = 0.5 * (1.0 - std::cos(PI * t));
        }
        else if (f <= f_high)
        {
            gain = 1.0;
        }
        else if (f <= (f_high + trans_width))
        {
            double t = (f - f_high) / trans_width;
            gain = 0.5 * (1.0 + std::cos(PI * t));
        }
        else
        {
            gain = 0.0;
        }
        (*Hd)(k) *= gain;
    }
    if (K > 0)
        (*Hd)(K - 1).imag(0.0);
}
