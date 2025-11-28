#include "beam_forming_basic.h"

/* --------------------------------------------------------------------------------------------------------------- */
/* ------------------------------------------- Beam Forming Element ---------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

void vuprs::BeamFormingElement::UpdataTimeDelay(double targetAlt, double targetAz, double waveVelocity)
{
    this->timeDelay = -vuprs::AltAz2PointingVector(targetAlt, targetAz).dot(this->positionVector) / waveVelocity;
}

void vuprs::BeamFormingElement::DoFFT()
{
    if (this->adcChannel.empty())
    {
        throw std::runtime_error("Cannot do FFT in an empty element.");
    }
    
    vuprs::FFT(&this->elementSignalTimeDomain, &this->elementSignalFrequencyDomain_std);
    vuprs::CutTheFirstHalf(&this->elementSignalFrequencyDomain_std);
    vuprs::stdVector2eigenVector<std::complex<double>>(&this->elementSignalFrequencyDomain_std, &this->elementSignalFrequencyDomain_eigen);
}

bool vuprs::BeamFormingElement::empty() const
{
    return this->adcChannel.empty();
}

/* --------------------------------------------------------------------------------------------------------------- */
/* -------------------------------------------- Beam Forming Array ----------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

vuprs::BeamFormingArray::BeamFormingArray()
{

}

vuprs::BeamFormingArray::BeamFormingArray(const std::string &filename)
{
    if (!this->LoadArrayFromJson(filename))
    {
        throw std::runtime_error("Failed to load array from: " + filename);
    }
}

vuprs::BeamFormingArray::~BeamFormingArray()
{
    vuprs::AlignedEigenVector<vuprs::BeamFormingElement>().swap(this->elementArray);
}

bool vuprs::BeamFormingArray::LoadArrayFromJson(const std::string &filename)
{
    std::ifstream arrayConfigJsonFile;

    arrayConfigJsonFile.open(filename);
    if (!arrayConfigJsonFile.is_open())
    {
        throw std::runtime_error("Cannot open file: " + filename);
    }

    nlohmann::json configJsonData;

    try
    {
        arrayConfigJsonFile >> configJsonData;
    }
    catch(const std::exception& e)
    {
        throw std::runtime_error("Failed to load array data from: " + filename);
    }

    if (configJsonData.contains("beam_forming_array"))
    {
        auto beamFormingArray = configJsonData["beam_forming_array"];
        if (beamFormingArray.contains("array"))
        {
            auto arrayData = beamFormingArray["array"];
            if (beamFormingArray.is_array())
            {
                int arraySize = arrayData.size();
                this->elementArray.resize(arraySize);
                for (int i = 0; i < arraySize; i++)
                {
                    if (arrayData[i].contains("index") && arrayData[i].contains("position_x") && arrayData[i].contains("position_y") &&
                        arrayData[i].contains("position_z") && arrayData[i].contains("adc_channel"))
                    {
                        double x = 0, y = 0, z = 0;
                        int successCount = 0;
                        bool status = false;
                        std::string adcChannel;

                        x = vuprs::ParseDoubleFromString(arrayData[i]["position_x"].get<std::string>(), &status);
                        if (status) {this->elementArray[i].positionVector(0, 0) = x; successCount++;}

                        y = vuprs::ParseDoubleFromString(arrayData[i]["position_y"].get<std::string>(), &status);
                        if (status) {this->elementArray[i].positionVector(1, 0) = y; successCount++;}

                        z = vuprs::ParseDoubleFromString(arrayData[i]["position_z"].get<std::string>(), &status);
                        if (status) {this->elementArray[i].positionVector(2, 0) = z; successCount++;}

                        adcChannel = arrayData[i]["adc_channel"].get<std::string>();
                        if (!adcChannel.empty()) {this->elementArray[i].adcChannel = adcChannel;}
                        
                        if (successCount < 4)
                        {
                            throw std::runtime_error("Parse array data error.");
                        }
                    }
                    else
                    {
                        throw std::runtime_error("Missing element at index: " + std::to_string(i));
                    }
                }
            }
            else
            {
                throw std::runtime_error("Cannot find array in file: " + filename);
            }
        }
        else
        {
            throw std::runtime_error("Missing element [array]");
        }
    }
    else
    {
        throw std::runtime_error("Missing element [beam_forming_array]");
    }

    return true;
}

void vuprs::BeamFormingArray::UpdateTimeDelay(double targetAlt, double targetAz, double waveVelocity)
{
    int arraySize = this->elementArray.size();
    this->timeDelayVector.resize(arraySize, 1);
    for (int i = 0; i < arraySize; i++)
    {
        this->elementArray[i].UpdataTimeDelay(targetAlt, targetAz, waveVelocity);
        this->timeDelayVector(i, 0).real(this->elementArray[i].timeDelay);
    }
}

void vuprs::BeamFormingArray::InputElementSignal(const vuprs::SignalData &adcData)
{
    if (this->elementArray.size() <= 0)
    {
        throw std::runtime_error("Cannot input signal to an empty array.");
    }

    int arraySize = this->elementArray.size();
    
    this->samplingFrequency = adcData.samplingFrequency;
    this->samplingTime = adcData.samplingTime;
    this->signalPointCounts = adcData.signalPoints;
    
    for (int i = 0; i < arraySize; i++)
    {
        if (adcData.contains(this->elementArray[i].adcChannel))
        {
            adcData.GetChannelData(this->elementArray[i].adcChannel, &this->elementArray[i].elementSignalTimeDomain);
            this->elementArray[i].samplingFrequency = adcData.samplingFrequency;
            this->elementArray[i].samplingTime = adcData.samplingTime;
        }
    }
}

bool vuprs::BeamFormingArray::empty() const
{
    return this->elementArray.size() <= 0;
}

Eigen::Matrix<Eigen::dcomplex, -1, 1> vuprs::BeamFormingArray::ArrayResponseVector(double frequency) const
{
    return (-this->timeDelayVector * std::complex<double>(0, 1) * 2.0 * PI * frequency).array().exp().matrix();
}

Eigen::Matrix<Eigen::dcomplex, -1, -1> vuprs::BeamFormingArray::ArraySignalMatrix(bool frequencyDomain, double *samplingFrequency)
{
    uint64_t dataSize = this->elementArray[0].elementSignalTimeDomain.size();
    uint64_t elementSize = this->elementArray.size();

    if (dataSize <= 0)
    {
        throw std::runtime_error("Cannot get array signal matrix (no data input in advance).");
    }
    if (elementSize <= 0)
    {
        throw std::runtime_error("Cannot get array signal matrix from an empty array.");
    }

    Eigen::Matrix<Eigen::dcomplex, -1, -1> arraySignalMatrix;

    if (frequencyDomain) arraySignalMatrix.resize(this->elementArray.size(), dataSize / 2 + 1);
    else arraySignalMatrix.resize(this->elementArray.size(), dataSize);

    for (int i = 0; i < elementSize; i++)
    {
        if (this->elementArray.empty())
        {
            throw std::runtime_error("Cannot get array signal matrix from an empty array.");
        }
        if (frequencyDomain)
        {
            this->elementArray[i].DoFFT();
            arraySignalMatrix.row(i) = this->elementArray[i].elementSignalFrequencyDomain_eigen.transpose();
        }
        else
        {
            arraySignalMatrix.row(i) = Eigen::Map<Eigen::Matrix<Eigen::dcomplex, 1, -1>>(this->elementArray[i].elementSignalTimeDomain.data(), this->elementArray[i].elementSignalTimeDomain.size());
        }
    }

    if (samplingFrequency != nullptr) *samplingFrequency = this->samplingFrequency;
    return arraySignalMatrix;
}

double vuprs::BeamFormingArray::MaxAbsoluteTimeDelay() const
{
    return this->timeDelayVector.array().abs().matrix().maxCoeff();
}
