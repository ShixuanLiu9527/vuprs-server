#include "beam_forming_basic.h"

/* --------------------------------------------------------------------------------------------------------------- */
/* ------------------------------------------- Beam Forming Element ---------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

void vuprs::BeamFormingElement::UpdataTimeDelay(const double &targetAlt, const double &targetAz, const double &waveVelocity)
{
    this->timeDelay = -vuprs::AltAz2PointingVector(targetAlt, targetAz).dot(this->positionVector) / waveVelocity;
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
    vuprs::AlignedEigenVector<vuprs::BeamFormingElement>().swap(this->array);
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
                this->array.resize(arraySize);
                for (int i = 0; i < arraySize; i++)
                {
                    if (arrayData[i].contains("index") && arrayData[i].contains("position_x") && arrayData[i].contains("position_y") &&
                        arrayData[i].contains("position_z") && arrayData[i].contains("adc_channel"))
                    {
                        double x = 0, y = 0, z = 0;
                        int adcChannel = 0, successCount = 0;
                        bool status = false;

                        x = vuprs::ParseDoubleFromString(arrayData[i]["position_x"].get<std::string>(), &status);
                        if (status) {this->array[i].positionVector(0, 0) = x; successCount++;}

                        y = vuprs::ParseDoubleFromString(arrayData[i]["position_y"].get<std::string>(), &status);
                        if (status) {this->array[i].positionVector(1, 0) = y; successCount++;}

                        z = vuprs::ParseDoubleFromString(arrayData[i]["position_z"].get<std::string>(), &status);
                        if (status) {this->array[i].positionVector(2, 0) = z; successCount++;}

                        adcChannel = vuprs::ParseNumberFromString(arrayData[i]["adc_channel"].get<std::string>(), &status);
                        if (status) {this->array[i].adcChannel = adcChannel; successCount++;}

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

void vuprs::BeamFormingArray::UpdataTimeDelay(const double &targetAlt, const double &targetAz, const double &waveVelocity)
{
    int arraySize = this->array.size();
    this->timeDelayVector.resize(arraySize, 1);
    for (int i = 0; i < arraySize; i++)
    {
        this->array[i].UpdataTimeDelay(targetAlt, targetAz, waveVelocity);
        this->timeDelayVector(i, 0) = this->array[i].timeDelay;
    }
}

Eigen::Matrix<Eigen::dcomplex, -1, -1> vuprs::BeamFormingArray::ArrayResponseVector(const double &frequency) const
{
    return (this->timeDelayVector.cast<Eigen::dcomplex>() * std::complex<double>(0, 1) * 2.0 * PI * frequency).array().exp().matrix();
}

double vuprs::BeamFormingArray::MaxAbsoluteTimeDelay() const
{
    return this->timeDelayVector.maxCoeff();
}
