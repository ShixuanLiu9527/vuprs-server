#include "beam_former_template.h"

vuprs::BeamFormerTemplate::BeamFormerTemplate()
{

}

vuprs::BeamFormerTemplate::BeamFormerTemplate(const std::string &arrayConfigFile)
{
    this->ConfigBeamFormingArray(arrayConfigFile);
}

void vuprs::BeamFormerTemplate::ConfigBeamFormingArray(const std::string &arrayConfigFile)
{
    this->array.LoadArrayFromJson(arrayConfigFile);
}

void vuprs::BeamFormerTemplate::InputElementSignal(const vuprs::SignalData &signalData)
{
    if (this->array.empty())
    {
        throw std::runtime_error("Cannot input element signal to an empty array.");
    }

    this->array.InputElementSignal(signalData);

    this->samplingFrequency = signalData.samplingFrequency;
    this->samplingTime = signalData.samplingTime;
    this->dataNumber = signalData._channelData[0].size();
}

void vuprs::BeamFormerTemplate::SetTargetDirection(const double &alt, const double az, const double waveVelocity)
{
    if (this->array.empty())
    {
        throw std::runtime_error("Cannot set target direction for an empty array.");
    }

    this->array.UpdateTimeDelay(alt, az, waveVelocity);
}

Eigen::Matrix<Eigen::dcomplex, -1, 1> vuprs::GenerateBeamFormingFrequencyList(const int &dataNumber, const double &samplingFrequency)
{
    Eigen::Matrix<Eigen::dcomplex, -1, 1> retVector;
    int frequencyNumber = dataNumber / 2;
    retVector.resize(frequencyNumber, 1);
    for (int i = 0; i < frequencyNumber; i++)
    {
        retVector(i, 0).real(0.0);
        retVector(i, 0).imag(2.0 * PI * (double(i) * samplingFrequency / double(frequencyNumber)));  /* -2 * pi * f * j */
    }
    return retVector;
}
