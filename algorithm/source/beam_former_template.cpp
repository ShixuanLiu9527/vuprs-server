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
}

void vuprs::BeamFormerTemplate::SetTargetDirection(double alt, double az, double waveVelocity)
{
    if (this->array.empty())
    {
        throw std::runtime_error("Cannot set target direction for an empty array.");
    }

    this->array.UpdateTimeDelay(alt, az, waveVelocity);
}

Eigen::Matrix<Eigen::dcomplex, -1, 1> vuprs::GenerateBeamFormingFrequencyList(int dataNumber, double samplingFrequency)
{
    return 2.0 * PI * vuprs::GenerateFrequencyList(dataNumber, samplingFrequency);  /* 2 * pi * f * j */
}
