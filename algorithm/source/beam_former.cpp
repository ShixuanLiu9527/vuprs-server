#include "beam_former.h"

void vuprs::BeamFormerCBF::OutputSignal(std::vector<std::complex<double>> *outputSignal)
{
    if (this->array.empty())
    {
        throw std::runtime_error("Cannot beam forming use empty array.");
    }

    int elementCount = this->array.elementArray.size();
    Eigen::Matrix<Eigen::dcomplex, -1, 1> elementSignalFrequencyDomain, totalSignalFrequencyDomain;
    Eigen::Matrix<Eigen::dcomplex, -1, 1> beamFormingFrequencyVector = vuprs::GenerateBeamFormingFrequencyList(this->dataNumber, this->samplingFrequency);
    Eigen::Matrix<Eigen::dcomplex, -1, 1> weightVector;
    Eigen::Matrix<Eigen::dcomplex, -1, 1> fullSpectrum(this->dataNumber);

    totalSignalFrequencyDomain.resize(this->dataNumber, 1);
    totalSignalFrequencyDomain.setZero();

    for (int i = 0; i < elementCount; i++)
    {
        /* FFT & Cut */

        vuprs::FFT(&this->array.elementArray[i].elementSignalTimeDomain, &this->array.elementArray[i].elementSignalFrequencyDomain, false);
        vuprs::CutTheFirstHalf(&this->array.elementArray[i].elementSignalFrequencyDomain);

        auto& freqData = this->array.elementArray[i].elementSignalFrequencyDomain;
        elementSignalFrequencyDomain = Eigen::Map<Eigen::Matrix<Eigen::dcomplex, -1, 1>>(freqData.data(), freqData.size());  /* cut */

        /* Weighted */

        weightVector = (beamFormingFrequencyVector * this->array.timeDelayVector(i, 0)).array().exp().matrix();
        this->array.elementArray[i].phasedElementSignalFrequencyDomain = (elementSignalFrequencyDomain.array() * weightVector.array()).matrix();

        /* Montage */

        int halfSize = this->array.elementArray[i].phasedElementSignalFrequencyDomain.size();
        auto& phasedSignal = this->array.elementArray[i].phasedElementSignalFrequencyDomain;

        fullSpectrum << phasedSignal, phasedSignal.bottomRows(halfSize - 1).colwise().reverse().conjugate();

        /* Add */

        totalSignalFrequencyDomain += fullSpectrum;
    }

    totalSignalFrequencyDomain /= std::pow(double(elementCount), 2.0);

    /* IFFT */

    std::vector<std::complex<double>> ifftInput(totalSignalFrequencyDomain.size());
    Eigen::Map<Eigen::Matrix<Eigen::dcomplex, -1, 1>>(ifftInput.data(), ifftInput.size()) = totalSignalFrequencyDomain;

    vuprs::FFT(&ifftInput, outputSignal, true);
}
