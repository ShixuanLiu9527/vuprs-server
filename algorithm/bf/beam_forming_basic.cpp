#include "algorithm/bf/beam_forming_basic.h"
#include "logger/log_manager.h"

/* --------------------------------------------------------------------------------------------------------------- */
/* ------------------------------------------- Beam Forming Element ---------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

vuprs::BeamFormingElement::BeamFormingElement()
{
}

void vuprs::BeamFormingElement::UpdataTimeDelay(double targetAlt, double targetAz, double waveVelocity)
{
    this->timeDelay = -vuprs::AltAz2PointingVector(targetAlt, targetAz).dot(this->positionVector) / waveVelocity;
}

void vuprs::BeamFormingElement::AddWindowForSignal()
{
    RUNTIME_CHECK(!this->adcChannel.empty(), "bf", "Cannot do FFT in an empty element");
    RUNTIME_CHECK(this->elementSignalTimeDomain.size() > 0, "bf", "Signal is empty");
    /* Add window */
    vuprs::stdVector2eigenVector<Eigen::dcomplex>(this->elementSignalTimeDomain, &this->windowedSignal_eigen);
    Eigen::dcomplex mean = this->windowedSignal_eigen.mean(); /* - average */
    this->windowedSignal_eigen.array() -= mean;
    vuprs::AddWindow<Eigen::dcomplex>(&this->windowedSignal_eigen, vuprs::WindowType::SIG_WINDOW__HAMMING);
}

bool vuprs::BeamFormingElement::RunDFT()
{
    this->AddWindowForSignal();
    int N = this->windowedSignal_eigen.rows();
    RUNTIME_CHECK(N > 0, "bf", "Failed to allocate FFTW memory (data size = 0)");
    this->fftManager.SetParameters(N, true);
    if (this->elementSignalFrequencyDomain_eigen.rows() != N)
    {
        this->elementSignalFrequencyDomain_eigen.resize(N, 1);
    }
    this->fftManager.DoDFT(this->windowedSignal_eigen.data(), this->elementSignalFrequencyDomain_eigen.data());
    vuprs::CutTheFirstHalf(&this->elementSignalFrequencyDomain_eigen);
    return true;
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
    this->fs = 0.0;
    this->samplingTime = 0.0;
    this->signalPointCounts = 0;
    this->maxElementPositionError = 0.0;
    this->threadPool = std::make_unique<vuprs::ThreadPool>(std::thread::hardware_concurrency());
}

vuprs::BeamFormingArray::~BeamFormingArray()
{
    vuprs::AlignedEigenVector<vuprs::BeamFormingElement>().swap(this->elementArray);
}

double vuprs::BeamFormingArray::CalculateSteeringVectorErrorRadius(double signalFrequency) const
{
    RUNTIME_CHECK(!this->empty(), "bf", "Array is empty");

    /* Calculate time delay error */
    double timedelay_err = this->maxElementPositionError / DEFAULT_SOUND_VELOCITY_MPS;
    /* Get error */
    Eigen::Matrix<double, -1, 1> vec;
    vec.resize(this->elementArray.size(), 1);
    vec.setOnes();
    vec *= PI * signalFrequency * timedelay_err;
    vec = vec.array().sin().pow(2.0).matrix();
    return 4.0 * vec.sum();
}

bool vuprs::BeamFormingArray::LoadArrayFromJson(const std::string &filename)
{
    std::ifstream arrayConfigJsonFile;
    arrayConfigJsonFile.open(filename);
    RUNTIME_CHECK(arrayConfigJsonFile.is_open(), "bf", "Cannot open file: " + filename);

    nlohmann::json configJsonData;
    try
    {
        arrayConfigJsonFile >> configJsonData;
    }
    catch (const std::exception &e)
    {
        RUNTIME_CHECK(false, "bf", " in [BeamFormingArray::LoadArrayFromJson] Failed to load array data from: " + filename);
    }

    if (configJsonData.contains("beam_forming_array"))
    {
        auto beamFormingArray = configJsonData["beam_forming_array"];
        if (beamFormingArray.contains("array"))
        {
            auto arrayData = beamFormingArray["array"];
            if (arrayData.is_array())
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
                        std::string adcChannel;
                        vuprs::BeamFormingElement oneBeamFormingElement;
                        vuprs::__JsonStringParseFLOAT<double>(&x, arrayData[i], "position_x", true);
                        vuprs::__JsonStringParseFLOAT<double>(&y, arrayData[i], "position_y", true);
                        vuprs::__JsonStringParseFLOAT<double>(&z, arrayData[i], "position_z", true);
                        oneBeamFormingElement.positionVector(0, 0) = x;
                        oneBeamFormingElement.positionVector(1, 0) = y;
                        oneBeamFormingElement.positionVector(2, 0) = z;
                        vuprs::__JsonParseString(&adcChannel, arrayData[i], "adc_channel", true);
                        oneBeamFormingElement.adcChannel = adcChannel;
                        RUNTIME_CHECK(IS_ADC_CHANNEL_NAME(adcChannel), "bf", " in [BeamFormingArray::LoadArrayFromJson] Invalid ADC channel name in file: " + filename);
                        this->elementArray.push_back(std::move(oneBeamFormingElement));
                    }
                    else
                    {
                        RUNTIME_CHECK(false, "bf", " in [BeamFormingArray::LoadArrayFromJson] Missing element at index: " + std::to_string(i));
                    }
                }
            }
            else
            {
                RUNTIME_CHECK(false, "bf", " in [BeamFormingArray::LoadArrayFromJson] Cannot find array in file: " + filename);
            }
        }
        else
        {
            RUNTIME_CHECK(false, "bf", " in [BeamFormingArray::LoadArrayFromJson] Missing element [array]");
        }
        if (beamFormingArray.contains("info"))
        {
            auto info = beamFormingArray["info"];
            vuprs::__JsonStringParseFLOAT<double>(&this->maxElementPositionError, info, "max-element-position-error-radius", true);
        }
        else
        {
            RUNTIME_CHECK(false, "bf", " in [BeamFormingArray::LoadArrayFromJson] Missing element [info]");
        }
    }
    else
    {
        RUNTIME_CHECK(false, "bf", " in [BeamFormingArray::LoadArrayFromJson] Missing element [beam_forming_array]");
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
        this->timeDelayVector(i, 0).imag(0.0);
    }
}

void vuprs::BeamFormingArray::InputElementSignal(const vuprs::SignalData &adcData)
{
    RUNTIME_CHECK(this->elementArray.size() > 0, "bf", "Cannot input signal to an empty array.");

    int arraySize = this->elementArray.size();
    this->fs = adcData.samplingFrequency;
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
    Eigen::Matrix<Eigen::dcomplex, -1, 1> _j_omega = -2.0 * PI * vuprs::GenerateComplexFrequencyList(this->signalPointCounts, this->fs); /* -j * omega */
    *matrix = (this->timeDelayVector * _j_omega.transpose()).array().exp().matrix();                                                     /* exp(-jwT{i}) */
}

void vuprs::BeamFormingArray::GetArraySignalMatrix(Eigen::Matrix<Eigen::dcomplex, -1, -1> *signalMatrix, double *samplingFrequency, bool frequencyDomain)
{
    uint64_t dataSize = this->signalPointCounts;
    uint64_t elementSize = this->elementArray.size();
    RUNTIME_CHECK(dataSize > 0, "bf", "Cannot get array signal matrix (no data input in advance)");
    RUNTIME_CHECK(elementSize > 0, "bf", "Cannot get array signal matrix from an empty array");
    if (frequencyDomain)
        signalMatrix->resize(this->elementArray.size(), dataSize / 2 + 1); /* M x (N/2 + 1) */
    else
        signalMatrix->resize(elementSize, dataSize); /* M x N */
    if (frequencyDomain)
    {
        std::vector<std::future<void>> futures;
        futures.reserve(elementSize);
        for (int i = 0; i < elementSize; i++)
        {
            futures.emplace_back(this->threadPool->enqueue(
                [this, i, &signalMatrix]()
                {
                    this->elementArray[i].RunDFT();
                    signalMatrix->row(i) = this->elementArray[i].elementSignalFrequencyDomain_eigen.transpose();
                }));
        }
        for (auto &f : futures)
            f.get();
    }
    else
    {
        for (uint64_t i = 0; i < elementSize; i++)
        {
            signalMatrix->row(i) = Eigen::Map<Eigen::Matrix<Eigen::dcomplex, 1, -1>>(
                this->elementArray[i].elementSignalTimeDomain.data(),
                this->elementArray[i].elementSignalTimeDomain.size());
        }
    }
    if (samplingFrequency != nullptr)
        *samplingFrequency = this->fs;
}

double vuprs::BeamFormingArray::GetMaxAbsoluteTimeDelay() const
{
    return this->timeDelayVector.array().abs().matrix().maxCoeff();
}

/* --------------------------------------------------------------------------------------------------------------- */
/* -------------------------------------------- Beam Forming Scan Array ------------------------------------------ */
/* --------------------------------------------------------------------------------------------------------------- */

vuprs::BeamFormingScanArray::BeamFormingScanArray()
{
}

vuprs::BeamFormingScanArray::~BeamFormingScanArray()
{
}

bool vuprs::BeamFormingScanArray::LoadArrayFromJson(const std::string &filename)
{
    std::ifstream arrayConfigJsonFile;
    arrayConfigJsonFile.open(filename);
    RUNTIME_CHECK(arrayConfigJsonFile.is_open(), "bf", "Cannot open file: " + filename);
    nlohmann::json configJsonData;
    try
    {
        arrayConfigJsonFile >> configJsonData;
    }
    catch (const std::exception &e)
    {
        RUNTIME_CHECK(false, "bf", " in [BeamFormingArray::LoadArrayFromJson] Failed to load array data from: " + filename);
    }

    if (configJsonData.contains("beam_forming_array"))
    {
        auto beamFormingArray = configJsonData["beam_forming_array"];
        if (beamFormingArray.contains("array"))
        {
            auto arrayData = beamFormingArray["array"];
            if (arrayData.is_array())
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
                        std::string adcChannel;
                        vuprs::BeamFormingElement oneBeamFormingElement;
                        vuprs::__JsonStringParseFLOAT<double>(&x, arrayData[i], "position_x", true);
                        vuprs::__JsonStringParseFLOAT<double>(&y, arrayData[i], "position_y", true);
                        vuprs::__JsonStringParseFLOAT<double>(&z, arrayData[i], "position_z", true);
                        oneBeamFormingElement.positionVector(0, 0) = x;
                        oneBeamFormingElement.positionVector(1, 0) = y;
                        oneBeamFormingElement.positionVector(2, 0) = z;
                        vuprs::__JsonParseString(&adcChannel, arrayData[i], "adc_channel", true);
                        oneBeamFormingElement.adcChannel = adcChannel;
                        RUNTIME_CHECK(IS_ADC_CHANNEL_NAME(adcChannel), "bf", " in [BeamFormingArray::LoadArrayFromJson] Invalid ADC channel name in file: " + filename);
                        this->elementArray.push_back(std::move(oneBeamFormingElement));
                    }
                    else
                    {
                        RUNTIME_CHECK(false, "bf", " in [BeamFormingArray::LoadArrayFromJson] Missing element at index: " + std::to_string(i));
                    }
                }
            }
            else
            {
                RUNTIME_CHECK(false, "bf", " in [BeamFormingArray::LoadArrayFromJson] Cannot find array in file: " + filename);
            }
        }
        else
        {
            RUNTIME_CHECK(false, "bf", " in [BeamFormingArray::LoadArrayFromJson] Missing element [array]");
        }
    }
    else
    {
        RUNTIME_CHECK(false, "bf", " in [BeamFormingArray::LoadArrayFromJson] Missing element [beam_forming_array]");
    }

    return true;
}

Eigen::Matrix<Eigen::dcomplex, -1, -1> vuprs::BeamFormingScanArray::GetImagTimedelay(const std::vector<double> &alt, const std::vector<double> &az, double waveVelocity) const
{
    PARAM_CHECK(!this->empty(), "bf", " in [BeamFormingScanArray::GetSteeringVectorMatrix] Array is empty.");
    PARAM_CHECK(alt.size() == az.size(), "bf", " in [BeamFormingScanArray::GetSteeringVectorMatrix] Size of alt and az should be the same.");

    int k = alt.size();
    int M = this->elementArray.size();
    Eigen::Matrix<Eigen::dcomplex, -1, -1> res(M, k);
    std::complex<double> j(0, 1);
    Eigen::Matrix<double, 3, 1> pointingVector;
    Eigen::Matrix<double, -1, -1> elementPositionMatrix(M, 3);
    for (int i = 0; i < M; i++)
    {
        elementPositionMatrix.row(i) = this->elementArray[i].positionVector.transpose();
    }
    for (int i = 0; i < k; i++)
    {
        pointingVector = vuprs::AltAz2PointingVector(alt[i], az[i]);
        res.col(i) = elementPositionMatrix * pointingVector * j / waveVelocity;
    }
    return res;
}

void vuprs::BeamFormingScanArray::GetSteeringVectorMatrix(Eigen::Matrix<Eigen::dcomplex, -1, -1> *matrix, const std::vector<double> &alt, const std::vector<double> &az, double frequency, double waveVelocity) const
{
    PARAM_CHECK(!this->empty(), "bf", " in [BeamFormingScanArray::GetSteeringVectorMatrix] Array is empty.");
    PARAM_CHECK(alt.size() == az.size(), "bf", " in [BeamFormingScanArray::GetSteeringVectorMatrix] Size of alt and az should be the same.");
    Eigen::Matrix<Eigen::dcomplex, -1, -1> jT = this->GetImagTimedelay(alt,
                                                                       az,
                                                                       waveVelocity);
    (*matrix) = (-2.0 * PI * frequency) * jT;
    matrix->array() = matrix->array().exp();
}

bool vuprs::BeamFormingScanArray::empty() const
{
    return this->elementArray.size() <= 0;
}

/* --------------------------------------------------------------------------------------------------------------- */
/* ------------------------------------------------ Tool functions ----------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

bool vuprs::SaveToCSV(const std::vector<double> &data, const std::string &filename)
{
    if (data.empty())
    {
        return true;
    }
    std::string dir;
    vuprs::SplitFile(filename, &dir, nullptr, nullptr);
    if (!dir.empty() && !vuprs::PathExist(dir))
    {
        vuprs::MakeDir(dir);
    }
    std::ofstream file(filename);
    if (!file.is_open())
    {
        return false;
    }
    file << std::fixed << std::setprecision(20);
    for (const auto &value : data)
    {
        file << value << "\n";
    }
    file.close();
    return true;
}

bool vuprs::SaveToCSV(const std::vector<std::vector<double>> &data, const std::string &filename)
{
    if (data.empty())
        return true;
    size_t rowCount = data[0].size();
    for (size_t i = 1; i < data.size(); ++i)
    {
        if (data[i].size() != rowCount)
            return false;
    }
    std::string dir;
    vuprs::SplitFile(filename, &dir, nullptr, nullptr);
    if (!dir.empty() && !vuprs::PathExist(dir))
    {
        vuprs::MakeDir(dir);
    }
    std::ofstream file(filename);
    if (!file.is_open())
        return false;
    file << std::fixed << std::setprecision(20);
    for (size_t r = 0; r < rowCount; ++r)
    {
        for (size_t c = 0; c < data.size(); ++c)
        {
            file << data[c][r];
            if (c + 1 < data.size())
                file << ",";
        }
        file << "\n";
    }
    file.close();
    return true;
}

bool vuprs::SaveToCSV_complex(const std::vector<std::complex<double>> &data, const std::string &filename)
{
    if (data.empty())
    {
        return true;
    }
    std::string dir;
    vuprs::SplitFile(filename, &dir, nullptr, nullptr);
    if (!dir.empty() && !vuprs::PathExist(dir))
    {
        vuprs::MakeDir(dir);
    }
    std::ofstream file(filename);
    if (!file.is_open())
    {
        return false;
    }
    file << "real,imag\n";
    file << std::fixed << std::setprecision(20);
    for (const auto &value : data)
    {
        file << value.real() << "," << value.imag() << "\n";
    }
    file.close();
    return true;
}

bool vuprs::SaveToCSV(const Eigen::Matrix<double, -1, 1> &data, const std::string &filename)
{
    std::vector<double> _data;
    vuprs::eigenVector2stdVector<double>(data, &_data);
    vuprs::SaveToCSV(_data, filename);
}

bool vuprs::SaveToCSV_complex(const Eigen::Matrix<Eigen::dcomplex, -1, 1> &data, const std::string &filename)
{
    std::vector<std::complex<double>> _data;
    vuprs::eigenVector2stdVector<std::complex<double>>(data, &_data);
    vuprs::SaveToCSV_complex(_data, filename);
}
