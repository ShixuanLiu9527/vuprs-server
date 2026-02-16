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
    if (this->elementSignalTimeDomain.size() <= 0)
    {
        throw std::runtime_error("Signal is empty.");
    }
    
    vuprs::FFT(this->elementSignalTimeDomain, &this->elementSignalFrequencyDomain_std);
    vuprs::CutTheFirstHalf(&this->elementSignalFrequencyDomain_std);
    vuprs::stdVector2eigenVector<std::complex<double>>(this->elementSignalFrequencyDomain_std, &this->elementSignalFrequencyDomain_eigen);
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
    this->samplingFrequency = 0.0;
    this->samplingTime = 0.0;
    this->signalPointCounts = 0;

    this->threadPool = std::make_unique<vuprs::ThreadPool>(std::thread::hardware_concurrency());
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
                this->elementArray.clear();
                this->elementArray.reserve(arraySize);

                for (int i = 0; i < arraySize; i++)
                {
                    if (arrayData[i].contains("position_x") && 
                        arrayData[i].contains("position_y") && 
                        arrayData[i].contains("position_z") && 
                        arrayData[i].contains("adc_channel"))
                    {
                        double x = 0, y = 0, z = 0;
                        int successCount = 0;
                        bool status = false;
                        std::string adcChannel;

                        vuprs::BeamFormingElement oneBeamFormingElement;

                        x = vuprs::ParseDoubleFromString(arrayData[i]["position_x"].get<std::string>(), &status);
                        if (status)
                        {
                            oneBeamFormingElement.positionVector(0, 0) = x; 
                            successCount++;
                        }

                        y = vuprs::ParseDoubleFromString(arrayData[i]["position_y"].get<std::string>(), &status);
                        if (status) 
                        {
                            oneBeamFormingElement.positionVector(1, 0) = y; 
                            successCount++;
                        }

                        z = vuprs::ParseDoubleFromString(arrayData[i]["position_z"].get<std::string>(), &status);
                        if (status) 
                        {
                            oneBeamFormingElement.positionVector(2, 0) = z; 
                            successCount++;
                        }

                        adcChannel = arrayData[i]["adc_channel"].get<std::string>();
                        if (!adcChannel.empty()) 
                        {
                            oneBeamFormingElement.adcChannel = adcChannel;
                            successCount++;
                        }

                        if (successCount < 4)
                        {
                            throw std::runtime_error("Parse array data error.");
                        }

                        if (IS_ADC_CHANNEL_NAME(adcChannel))
                        {
                            this->elementArray.push_back(oneBeamFormingElement);
                        }
                        else
                        {
                            throw std::runtime_error("Invalid ADC channel name in file: " + filename);
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

Eigen::Matrix<Eigen::dcomplex, -1, 1> vuprs::BeamFormingArray::GetSteeringVector(double frequency) const
{
    return (-this->timeDelayVector * std::complex<double>(0, 1) * 2.0 * PI * frequency).array().exp().matrix();
}

void vuprs::BeamFormingArray::GetSteeringVectorMatrix(Eigen::Matrix<Eigen::dcomplex, -1, -1> *matrix) const
{
    Eigen::Matrix<Eigen::dcomplex, -1, 1> _j_omega = -2.0 * PI * vuprs::GenerateComplexFrequencyList(this->signalPointCounts, this->samplingFrequency);  /* -j * omega */
    *matrix = (this->timeDelayVector * _j_omega.transpose()).array().exp().matrix();  /* exp(-jwT{i}) */
}

void vuprs::BeamFormingArray::GetArraySignalMatrix(Eigen::Matrix<Eigen::dcomplex, -1, -1> *signalMatrix, double *samplingFrequency, bool frequencyDomain)
{
    uint64_t dataSize = this->signalPointCounts;
    uint64_t elementSize = this->elementArray.size();

    if (dataSize <= 0)
    {
        throw std::runtime_error("Cannot get array signal matrix (no data input in advance).");
    }
    if (elementSize <= 0)
    {
        throw std::runtime_error("Cannot get array signal matrix from an empty array.");
    }

    if (frequencyDomain) signalMatrix->resize(this->elementArray.size(), dataSize / 2 + 1);  /* M x (N/2 + 1) */
    else signalMatrix->resize(elementSize, dataSize);  /* M x N */

    std::vector<std::future<void>> futures;
    futures.reserve(elementSize);

    if (frequencyDomain)
    {
        for (uint64_t i = 0; i < elementSize; i++)
        {
            futures.emplace_back(
                [this, i](){this->elementArray[i].DoFFT();}
            );
        }
        for (auto &f : futures)  /* wait complete */
        {
            f.get();
        }
        for (uint64_t i = 0; i < elementSize; i++)
        {
            signalMatrix->row(i) = this->elementArray[i].elementSignalFrequencyDomain_eigen.transpose();
        }
    }
    else
    {
        for (uint64_t i = 0; i < elementSize; i++)
        {
            signalMatrix->row(i) = Eigen::Map<Eigen::Matrix<Eigen::dcomplex, 1, -1>>(
                this->elementArray[i].elementSignalTimeDomain.data(), 
                this->elementArray[i].elementSignalTimeDomain.size()
            );
        }
    }
    if (samplingFrequency != nullptr) *samplingFrequency = this->samplingFrequency;
}

double vuprs::BeamFormingArray::GetMaxAbsoluteTimeDelay() const
{
    return this->timeDelayVector.array().abs().matrix().maxCoeff();
}
