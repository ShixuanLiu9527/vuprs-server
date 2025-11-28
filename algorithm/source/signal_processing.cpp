#include "signal_processing.h"

void vuprs::FFT(const std::vector<std::complex<double>> *inputRealData, std::vector<std::complex<double>> *outputData, bool inverse)
{
    uint64_t dataSize = inputRealData->size();

    fftw_complex *input, *output;
    fftw_plan plan;

    input = reinterpret_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex) * dataSize));
    output = reinterpret_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex) * dataSize));

    if (!inverse)
    {
        plan = fftw_plan_dft_1d(dataSize, input, output, FFTW_FORWARD, FFTW_ESTIMATE);
    }
    else
    {
        plan = fftw_plan_dft_1d(dataSize, input, output, FFTW_BACKWARD, FFTW_ESTIMATE);
    }

    /* Input data to *input */

    if (!inputRealData->empty())
    {
        std::memcpy(input, inputRealData->data(), dataSize * sizeof(std::complex<double>));
    }
    else
    {
        throw std::runtime_error("The input data is empty.");
    }

    /* Do fft */

    fftw_execute(plan);

    outputData->resize(dataSize);
    std::memcpy(outputData->data(), output, dataSize * sizeof(std::complex<double>));

    fftw_destroy_plan(plan);
    fftw_free(input);
    fftw_free(output);

    if (inverse)
    {
        for (int i = 0; i < dataSize; i++)
        {
            (*outputData)[i] = (*outputData)[i] / (double)dataSize;
        }
    }
}

void vuprs::CutTheFirstHalf(std::vector<std::complex<double>> *inputData)
{
    int size = inputData->size();
    inputData->resize(size / 2 + 1);
}

void vuprs::SignalMontage(std::vector<std::complex<double>> *inputData)
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
}

void vuprs::SignalMontage(Eigen::Matrix<Eigen::dcomplex, -1, 1> *inputData)
{
    int originSize = inputData->size();

    if (originSize <= 2)
    {
        return;
    }

    int backHalfSize = originSize - 2;
    Eigen::VectorXcd backHalf = inputData->segment(1, backHalfSize).reverse();
    
    backHalf = backHalf.conjugate();
    
    inputData->conservativeResize(originSize + backHalfSize);
    inputData->tail(backHalfSize) = backHalf;
}

Eigen::Matrix<Eigen::dcomplex, -1, 1> vuprs::GenerateFrequencyList(int dataNumber, double samplingFrequency)
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
