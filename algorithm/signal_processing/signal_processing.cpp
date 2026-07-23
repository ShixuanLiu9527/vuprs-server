#include "signal_processing.h"

std::mutex vuprs::FFTWManagerComplex::fftw_mtx;

vuprs::FFTWManagerComplex::FFTWManagerComplex()
{
    this->fft_points = 0;
    this->fft_forward = true;  /* true: DFT backward, false: DFT forward */
    this->fft_input = nullptr;
    this->fft_output = nullptr;  /* input & output memory */
    this->fft_plan = nullptr;  /* FFT plan */
}

vuprs::FFTWManagerComplex::~FFTWManagerComplex()
{
    std::unique_lock<std::mutex> lock(this->fftw_mtx);  /* LOCK */
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

vuprs::FFTWManagerComplex::FFTWManagerComplex(vuprs::FFTWManagerComplex&& other) noexcept
{
    this->fft_points = other.fft_points.load();
    this->fft_forward = other.fft_forward.load();
    this->fft_input = other.fft_input;
    this->fft_output = other.fft_output;  /* input & output memory */
    this->fft_plan = other.fft_plan;  /* FFT plan */

    other.fft_points = 0;
    other.fft_forward = true;
    other.fft_input = nullptr;
    other.fft_output = nullptr;
    other.fft_plan = nullptr;
}

vuprs::FFTWManagerComplex& vuprs::FFTWManagerComplex::operator=(vuprs::FFTWManagerComplex&& other) noexcept
{
    if (this != &other)
    {
        std::unique_lock<std::mutex> lock(this->fftw_mtx);  /* LOCK */
        
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
        std::unique_lock<std::mutex> lock(this->fftw_mtx);  /* LOCK */

        if (this->fft_input == nullptr || this->fft_output == nullptr)
        {
            throw std::runtime_error("in [FFTWManagerComplex::SetDFTDirection] fftw_input or fftw_output is NULL.");
        }

        /* destroy */

        if (this->fft_plan != nullptr)
        {
            fftw_destroy_plan(this->fft_plan);
            this->fft_plan = nullptr;
        }

        /* generate plan */

        if (forward)
        {
            this->fft_plan = fftw_plan_dft_1d(this->fft_points, this->fft_input, this->fft_output, FFTW_FORWARD, FFTW_ESTIMATE);
        }
        else
        {
            this->fft_plan = fftw_plan_dft_1d(this->fft_points, this->fft_input, this->fft_output, FFTW_BACKWARD, FFTW_ESTIMATE);
        }

        if (this->fft_plan == nullptr)
        {
            throw std::runtime_error("in [FFTWManagerComplex::SetDFTDirection] fft_plan is NULL.");
        }

        this->fft_forward = forward;
    }
}

void vuprs::FFTWManagerComplex::SetDFTPoints(uint64_t N)
{
    if (this->fft_points != N && N > 0)
    {
        std::unique_lock<std::mutex> lock(this->fftw_mtx);  /* LOCK */

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

        this->fft_input = reinterpret_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex) * N));
        this->fft_output = reinterpret_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex) * N));

        if (this->fft_input == nullptr || this->fft_output == nullptr)
        {
            throw std::runtime_error("in [FFTWManagerComplex::SetDFTPoints] Failed to allocate FFTW memory.");
        }

        /* plan */

        if (this->fft_forward) 
        {
            this->fft_plan = fftw_plan_dft_1d(N, this->fft_input, this->fft_output, FFTW_FORWARD, FFTW_ESTIMATE);
        }
        else 
        {
            this->fft_plan = fftw_plan_dft_1d(N, this->fft_input, this->fft_output, FFTW_BACKWARD, FFTW_ESTIMATE);
        }

        if (this->fft_plan == nullptr)
        {
            throw std::runtime_error("in [FFTWManagerComplex::SetDFTPoints] Failed to allocate FFTW plan.");
        }

        this->fft_points = N;
    }
}

void vuprs::FFTWManagerComplex::SetParameters(uint64_t points, bool forward)
{
    this->SetDFTPoints(points);
    this->SetDFTDirection(forward);
}

void vuprs::FFTWManagerComplex::DoDFT(const void* input, void *output)
{
    if (this->fft_plan == nullptr || this->fft_input == nullptr || this->fft_output == nullptr)
    {
        throw std::runtime_error("in [FFTWManagerComplex::DoDFT] fft_plan or fft_input or fft_output is NULL.");
    }
    std::memcpy(this->fft_input, input, this->fft_points * sizeof(std::complex<double>));
    fftw_execute(this->fft_plan);
    std::memcpy(output, this->fft_output, this->fft_points * sizeof(std::complex<double>));
}

void vuprs::FFT(const Eigen::Matrix<Eigen::dcomplex, -1, 1> &inputData, Eigen::Matrix<Eigen::dcomplex, -1, 1> *outputData, bool inverse)
{
    uint64_t dataSize = inputData.rows();

    if (dataSize <= 0)
    {
        throw std::runtime_error("in [vuprs::FFT] The input data is empty.");
    }
    
    fftw_complex *input, *output;
    fftw_plan plan;

    input = reinterpret_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex) * dataSize));
    output = reinterpret_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex) * dataSize));

    if (input == nullptr || output == nullptr) 
    {
        fftw_free(input);
        fftw_free(output);
        throw std::runtime_error("in [vuprs::FFT] Failed to allocate FFTW memory.");
    }

    if (!inverse)
    {
        plan = fftw_plan_dft_1d(dataSize, input, output, FFTW_FORWARD, FFTW_ESTIMATE);
    }
    else
    {
        plan = fftw_plan_dft_1d(dataSize, input, output, FFTW_BACKWARD, FFTW_ESTIMATE);
    }

    if (plan == nullptr)
    {
        fftw_free(input);
        fftw_free(output);
        throw std::runtime_error("in [vuprs::FFT] Failed to allocate FFTW plan.");
    }

    try
    {
        /* Input data to *input */

        std::memcpy(input, inputData.data(), dataSize * sizeof(std::complex<double>));

        /* Do fft */

        fftw_execute(plan);

        outputData->resize(dataSize, 1);
        std::memcpy(outputData->data(), output, dataSize * sizeof(std::complex<double>));

        if (inverse)
        {
            (*outputData) = (*outputData) / (double)dataSize;
        }
    }
    catch (const std::exception &e)
    {
        fftw_destroy_plan(plan);
        fftw_free(input);
        fftw_free(output);
        throw std::runtime_error("in [vuprs::FFT] Error occurred in FFT. (" + std::string(e.what()) + ")");
    }

    fftw_destroy_plan(plan);
    fftw_free(input);
    fftw_free(output);
}

void vuprs::FFT(const std::vector<std::complex<double>> &inputData, std::vector<std::complex<double>> *outputData, bool inverse)
{
    uint64_t dataSize = inputData.size();

    if (dataSize <= 0)
    {
        throw std::runtime_error("in [vuprs::FFT] The input data is empty.");
    }

    fftw_complex *input, *output;
    fftw_plan plan;

    input = reinterpret_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex) * dataSize));
    output = reinterpret_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex) * dataSize));

    if (input == nullptr || output == nullptr)
    {
        fftw_free(input);
        fftw_free(output);
        throw std::runtime_error("in [vuprs::FFT] Failed to allocate FFTW memory.");
    }

    if (!inverse)
    {
        plan = fftw_plan_dft_1d(dataSize, input, output, FFTW_FORWARD, FFTW_ESTIMATE);
    }
    else
    {
        plan = fftw_plan_dft_1d(dataSize, input, output, FFTW_BACKWARD, FFTW_ESTIMATE);
    }

    if (plan == nullptr)
    {
        fftw_free(input);
        fftw_free(output);
        throw std::runtime_error("in [vuprs::FFT] Failed to allocate FFTW plan.");
    }

    try
    {
        /* Input data to *input */

        std::memcpy(input, inputData.data(), dataSize * sizeof(std::complex<double>));

        /* Do fft */

        fftw_execute(plan);

        outputData->resize(dataSize);
        std::memcpy(outputData->data(), output, dataSize * sizeof(std::complex<double>));

        if (inverse)
        {
            for (uint64_t i = 0; i < dataSize; i++)
            {
                (*outputData)[i] = (*outputData)[i] / (double)dataSize;
            }
        }
    }
    catch (const std::exception &e)
    {
        fftw_destroy_plan(plan);
        fftw_free(input);
        fftw_free(output);
        throw std::runtime_error("in [vuprs::FFT] Error occurred in FFT. (" + std::string(e.what()) + ")");
    }

    fftw_destroy_plan(plan);
    fftw_free(input);
    fftw_free(output);
}

void vuprs::CutTheFirstHalf(std::vector<std::complex<double>> *inputData)
{
    int size = inputData->size();
    inputData->resize(size / 2 + 1);
}

void vuprs::CutTheFirstHalf(Eigen::Matrix<Eigen::dcomplex, -1, 1> *inputData)
{
    if (inputData == nullptr)
    {
        throw std::runtime_error("in [vuprs::CutTheFirstHalf] Input data is null.");
    }
    
    int size = inputData->rows();

    if (size <= 0) return;
    
    inputData->resize(size / 2 + 1);
}

void vuprs::CompleteConjugateSymmetric(std::vector<std::complex<double>> *inputData)
{
    uint64_t originSize = inputData->size();

    if (originSize <= 2)
    {
        return;
    }

    std::vector<std::complex<double>> backHalf = *inputData;

    /* reverse */

    std::reverse(backHalf.begin(), backHalf.end());

    /* erase N/2 and 0 */

    if (!backHalf.empty()) backHalf.erase(backHalf.begin());
    if (!backHalf.empty()) backHalf.pop_back();

    /* conjugate */

    for (auto& element: backHalf) {element = std::conj(element);}

    /* insert */

    inputData->insert(inputData->end(), backHalf.begin(), backHalf.end());

    (*inputData)[originSize - 1].imag(0.0);
    (*inputData)[0].imag(0.0);
}

void vuprs::CompleteConjugateSymmetric(Eigen::Matrix<Eigen::dcomplex, -1, 1> *inputData)
{
    int halfSize = inputData->rows();  /* N / 2 + 1 */ 
    if (halfSize <= 2) return;
    
    int fullSize = 2 * (halfSize - 1);
    
    inputData->conservativeResize(fullSize);
    
    for (int i = 1; i < halfSize - 1; i++)
    {
        (*inputData)(fullSize - i) = std::conj((*inputData)(i));
    }

    (*inputData)(halfSize - 1).imag(0.0);  /* x(N/2).imag = 0 */
    (*inputData)(0).imag(0.0);  /* x(0).imag = 0 */
}

void vuprs::CompleteSymmetric(Eigen::Matrix<double, -1, 1> *inputData)
{
    int halfSize = inputData->rows();  /* N / 2 + 1 */
    if (halfSize <= 2) return;
    
    int fullSize = 2 * (halfSize - 1);
    
    Eigen::Matrix<double, -1, 1> original = *inputData;
    
    inputData->conservativeResize(fullSize);

    for (int i = 1; i < halfSize - 1; i++)
    {
        (*inputData)(fullSize - i) = original(i);
    }
}

Eigen::Matrix<Eigen::dcomplex, -1, 1> vuprs::GenerateComplexFrequencyList(int dataNumber, double samplingFrequency)
{
    Eigen::Matrix<Eigen::dcomplex, -1, 1> retVector;
    int frequencyNumber = dataNumber / 2 + 1;
    retVector.resize(frequencyNumber, 1);
    retVector.setZero();
    for (int i = 0; i < frequencyNumber; i++)
    {
        retVector(i, 0).imag(double(i) / double(dataNumber));
    }
    retVector *= samplingFrequency;  /* f * j */
    return retVector;
}

Eigen::Matrix<double, -1, 1> vuprs::GenerateRealFrequencyList(int dataNumber, double samplingFrequency)
{
    Eigen::Matrix<double, -1, 1> retVector;
    int frequencyNumber = dataNumber / 2 + 1;
    retVector.resize(frequencyNumber, 1);
    retVector.setZero();
    for (int i = 0; i < frequencyNumber; i++)
    {
        retVector(i, 0) = double(i) / double(dataNumber);
    }
    retVector *= samplingFrequency;  /* f * j */
    return retVector;
}

Eigen::Matrix<double, -1, 1> vuprs::GetWindow(vuprs::WindowType type, int signalLength)
{
    Eigen::Matrix<double, -1, 1> w(signalLength);
    
    switch (type)
    {
        case WindowType::SIG_WINDOW__HAMMING: 
        {
            double alpha0 = 25.0 / 46.0;
            double alpha1 = 1.0 - alpha0;
            for (int i = 0; i < signalLength; i++) 
            {
                w(i) = alpha0 - alpha1 * cos(2 * M_PI * i / (signalLength - 1));
            }
            break;
        }
        case WindowType::SIG_WINDOW__HANN: 
        {
            for (int i = 0; i < signalLength; i++) 
            {
                w(i) = 0.5 * (1 - cos(2 * M_PI * i / (signalLength - 1)));
            }
            break;
        }
        case WindowType::SIG_WINDOW__BLACKMAN: 
        {
            double a0 = 0.42;
            double a1 = 0.5;
            double a2 = 0.08;
            for (int i = 0; i < signalLength; i++) 
            {
                w(i) = a0 - a1 * cos(2 * M_PI * i / (signalLength - 1)) + a2 * cos(4 * M_PI * i / (signalLength - 1));
            }
            break;
        }
        default:
        {
            throw std::runtime_error("in [vuprs::GetWindow] Invalid widnow type.");
        }
    }
    return w;
}

void vuprs::ApplyBandpassWindow(Eigen::Matrix<Eigen::dcomplex, -1, 1>* Hd, double f_low, double f_high, 
                                double fs, int N, double trans_width) 
{
    int K = Hd->rows();
    if (K <= 1) return;
    double df = fs / (double)N;
    if (trans_width < 0) trans_width = 10.0 * df;
    if (f_low - trans_width < 0) trans_width = f_low;
    if (f_high + trans_width > fs / 2.0) trans_width = fs / 2.0 - f_high;
    if (trans_width < 1e-12) trans_width = 1e-12;
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
    if (K > 0) (*Hd)(K - 1).imag(0.0);
}
