#include "signal_processing.h"

void vuprs::FFT(std::vector<std::complex<double>> *inputRealData, std::vector<std::complex<double>> *outputData, bool inverse)
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
}

void vuprs::CutTheFirstHalf(std::vector<std::complex<double>> *inputRealData)
{
    int size = inputRealData->size();
    inputRealData->resize(size / 2);
}
