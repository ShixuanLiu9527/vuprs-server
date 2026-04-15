#include "beam_former_template.h"

vuprs::WidebandBeamformerTemplate::WidebandBeamformerTemplate()
{
    this->threadPool = std::make_unique<vuprs::ThreadPool>(std::thread::hardware_concurrency());
    this->ResetAll();
}

vuprs::WidebandBeamformerTemplate::~WidebandBeamformerTemplate()
{
    vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>>().swap(this->mean_covMatrix);
    vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>>().swap(this->estimate_covMatrix);
}

void vuprs::WidebandBeamformerTemplate::ResetAll()
{
    this->fs = 0.0;
    this->signalPoints = 0;

    /* flags */

    this->is_arrayConfigDone = false;
    this->is_signalEmpty = true;
    this->is_covMatrixEmpty = true;

    this->firstSnapshot = true;
    this->COVARIANCE_SNAP_WINDOW_SIZE = DEFAULT_COVARIANCE_SNAP_WINDOW_SIZE;
    this->ADJACENT_FREQ_AVERAGE_INDEX = DEFAULT_ADJACENT_FREQ_AVERAGE_INDEX;

    this->ResetCovarianceMatrices();
    this->UpdateParameters();
}

bool vuprs::WidebandBeamformerTemplate::ConfigArrayFromJson(const std::string &arrayConfigJsonFilename)
{
    if (this->array.LoadArrayFromJson(arrayConfigJsonFilename))
    {
        int arraySize = this->array.elementArray.size();
        this->elementChannelName.resize(arraySize);
        this->elementPredelayTime.resize(arraySize);
        this->elementPredelayCount.resize(arraySize);
        for (int i = 0; i < arraySize; i++)
        {
            this->elementChannelName[i] = this->array.elementArray[i].adcChannel;
        }
        this->is_arrayConfigDone = true;
        this->scan_array.LoadArrayFromJson(arrayConfigJsonFilename);  /* Load scan array, which has same element position as array but different time delay */
        return true;
    }
    return false;
}

bool vuprs::WidebandBeamformerTemplate::ConfigDone() const
{
    return this->is_arrayConfigDone;
}

bool vuprs::WidebandBeamformerTemplate::CalculateEnable() const
{
    return this->ConfigDone() && !this->is_signalEmpty && !this->is_covMatrixEmpty;
}

int vuprs::WidebandBeamformerTemplate::ElementCount() const
{
    if (!this->ConfigDone())
    {
        throw std::runtime_error("in [WidebandBeamformerTemplate::ElementCount] Config not complete.");
    }
    return this->array.elementArray.size();
}

void vuprs::WidebandBeamformerTemplate::InputSignal(const vuprs::SignalData &signal)
{
    if (!this->ConfigDone())
    {
        throw std::runtime_error("in [WidebandBeamformerTemplate::InputSignal] Config not complete.");
    }

    this->array.InputElementSignal(signal);
    this->is_signalEmpty = false;

    if (signal.signalPoints != this->signalPoints && abs(signal.samplingFrequency - this->fs) > 1e-3)
    {
        this->fs = signal.samplingFrequency;
        this->signalPoints = signal.signalPoints;

        this->signalFrequencyList = vuprs::GenerateRealFrequencyList(this->signalPoints, this->fs);
        this->signalFrequencyList_complex = vuprs::GenerateComplexFrequencyList(this->signalPoints, this->fs);

        this->array.GetSteeringVectorMatrix(&this->steeringVectors);  /* Get steering vectors */
    }
}

void vuprs::WidebandBeamformerTemplate::SetTargetDirection(double alt, double az, double waveVelocity)
{
    if (!this->ConfigDone())
    {
        throw std::runtime_error("in [WidebandBeamformerTemplate::SetTargetDirection] Config not complete.");
    }
    this->array.UpdateTimeDelay(alt, az, waveVelocity);  /* Update time delay */
    if (this->fs > 0.0 && this->signalPoints > 0)
    {
        this->array.GetSteeringVectorMatrix(&this->steeringVectors);  /* Get steering vectors */
    }
}

void vuprs::WidebandBeamformerTemplate::UpdateCovarianceMatrix()
{
    if (!this->ConfigDone())
    {
        throw std::runtime_error("in [WidebandBeamformerTemplate::UpdateCovarianceMatrix] Config not complete.");
    }
    if (this->is_signalEmpty)
    {
        throw std::runtime_error("in [WidebandBeamformerTemplate::UpdateCovarianceMatrix] Signal is empty.");
    }

    this->array.GetArraySignalMatrix(&this->snap_signalMatrix_freqDomain, nullptr, true);  /* Get array signal */

    int dataPoints = this->snap_signalMatrix_freqDomain.cols();

    if (this->estimate_covMatrix.size() <= 0)
    {
        this->estimate_covMatrix.resize(dataPoints);
    }
    else if (this->estimate_covMatrix.size() != dataPoints)
    {
        throw std::runtime_error("in [WidebandBeamformerTemplate::UpdateCovarianceMatrix] Data points in snapshot != latest");
    }

    if (this->mean_covMatrix.size() <= 0)
    {
        this->mean_covMatrix.resize(dataPoints);
    }
    else if (this->mean_covMatrix.size() != dataPoints)
    {
        throw std::runtime_error("in [WidebandBeamformerTemplate::UpdateCovarianceMatrix] Data points in snapshot != latest");
    }

    Eigen::Matrix<Eigen::dcomplex, -1, 1> snapshotFreqSignal;
    Eigen::Matrix<Eigen::dcomplex, -1, -1> snapshotCovMatrix;

    /* Update mean covariance matrix */

    for (int i = 0; i < dataPoints; i++)
    {
        snapshotFreqSignal = this->snap_signalMatrix_freqDomain.col(i);
        snapshotCovMatrix = snapshotFreqSignal * snapshotFreqSignal.adjoint();
        if (this->firstSnapshot)
        {
            this->mean_covMatrix[i] = snapshotCovMatrix;
        }
        else
        {
            this->mean_covMatrix[i] = this->EXP_WEIGHTED_MOVING_AVERAGE_INDEX * snapshotCovMatrix + \
                                      this->EXP_WEIGHTED_MOVING_AVERAGE_INDEX_1 * this->mean_covMatrix[i];
        }
    }

    /* Update estimate covariance matrix */

    this->estimate_covMatrix[0] = this->mean_covMatrix[0];

    for (int i = 1; i < dataPoints - 1; i++)
    {
        this->estimate_covMatrix[i] = this->ADJACENT_FREQ_AVERAGE_INDEX_1 * this->mean_covMatrix[i - 1] + \
                                      this->ADJACENT_FREQ_AVERAGE_INDEX * this->mean_covMatrix[i] + \
                                      this->ADJACENT_FREQ_AVERAGE_INDEX_1 * this->mean_covMatrix[i + 1];
    }

    this->estimate_covMatrix[dataPoints - 1] = this->mean_covMatrix[dataPoints - 1];

    this->is_covMatrixEmpty = false;
    this->firstSnapshot = false;
}

void vuprs::WidebandBeamformerTemplate::SetCovarianceMatrixFittingParam(int snapsWindowSize, double adjacentFreqAverageIndex)
{
    this->COVARIANCE_SNAP_WINDOW_SIZE = snapsWindowSize;
    this->ADJACENT_FREQ_AVERAGE_INDEX = adjacentFreqAverageIndex;
    this->UpdateParameters();
}

void vuprs::WidebandBeamformerTemplate::UpdateParameters()
{
    this->EXP_WEIGHTED_MOVING_AVERAGE_INDEX = 2.0 / double(this->COVARIANCE_SNAP_WINDOW_SIZE + 1);  /* N = 2/a - 1 */
    this->EXP_WEIGHTED_MOVING_AVERAGE_INDEX_1 = 1.0 - this->EXP_WEIGHTED_MOVING_AVERAGE_INDEX;  /* 1 - a */

    this->ADJACENT_FREQ_AVERAGE_INDEX_1 = 0.5 * (1.0 - this->ADJACENT_FREQ_AVERAGE_INDEX);
}

void vuprs::WidebandBeamformerTemplate::ResetCovarianceMatrices()
{
    this->firstSnapshot = true;
    this->is_signalEmpty = true;
    this->is_covMatrixEmpty = true;

    this->mean_covMatrix.clear();
    this->mean_covMatrix.shrink_to_fit();

    this->estimate_covMatrix.clear();
    this->estimate_covMatrix.shrink_to_fit();
}

void vuprs::WidebandBeamformerTemplate::GetWeightVectorValues(Eigen::Matrix<Eigen::dcomplex, -1, -1> *dst) const
{
    *dst = this->resultWeightVectors;
}

void vuprs::WidebandBeamformerTemplate::GetFIRExpectedFrequencyResponse(Eigen::Matrix<Eigen::dcomplex, -1, -1> *dst, std::vector<std::string> *channelName, bool considerPredelay) const
{
    if (dst == nullptr || channelName == nullptr)
    {
        throw std::runtime_error("in [WidebandBeamformerTemplate::GetFIRExpectedFrequencyResponse] Destination cannot be NULL.");
    }
    if (this->is_signalEmpty)
    {
        throw std::runtime_error("in [WidebandBeamformerTemplate::GetFIRExpectedFrequencyResponse] No signal input.");
    }
    
    if (!considerPredelay)
    {
        *dst = this->resultWeightVectors;
        return;
    }

    uint64_t arraySize = this->array.elementArray.size();
    Eigen::Matrix<Eigen::dcomplex, 1, -1> j_2_pi_fk = this->signalFrequencyList_complex.transpose() * 2.0 * PI;
    Eigen::Matrix<Eigen::dcomplex, 1, -1> exp_j_2_pi_fk_Tm;  /* exp(j * 2 * pi * fk * Tm) */
    double Tm;

    dst->resize(this->resultWeightVectors.rows(), this->resultWeightVectors.cols());
    *channelName = this->elementChannelName;
    
    for (uint64_t i = 0; i < arraySize; i++)
    {
        Tm = this->elementPredelayTime[i];

        /* exp(j2*pi*fk*Tm) */

        exp_j_2_pi_fk_Tm = j_2_pi_fk * Tm;
        exp_j_2_pi_fk_Tm = exp_j_2_pi_fk_Tm.array().exp().matrix();
        
        /* Hd(fk) = conj(w(fk)) * exp(j*2*pi*fk*Tm), k = 0, 1, ..., N/2 + 1 */

        dst->row(i) = this->resultWeightVectors.row(i).array().conjugate() * exp_j_2_pi_fk_Tm.array();
    }
}

void vuprs::WidebandBeamformerTemplate::UpdateElementPredelay_externalFS(
    double firLength, double fs, bool includeFIRGroupDelay, 
    std::vector<int> *elementPredelayCount,
    std::vector<double> *elementPredelayTime,
    std::vector<std::string> *channelName)
{
    if (!this->ConfigDone())
    {
        throw std::runtime_error("in [WidebandBeamformerTemplate::UpdateElementPredelay_externalFS] No signal input.");
    }
    if (elementPredelayTime == nullptr || elementPredelayCount == nullptr || channelName == nullptr)
    {
        throw std::runtime_error("in [WidebandBeamformerTemplate::UpdateElementPredelay_externalFS] Predelay pointer is NULL.");
    }

    /* Calculate predelay */

    uint64_t arraySize = this->array.elementArray.size();
    double Ts = 1.0 / fs;

    for (uint64_t i = 0; i < arraySize; i++)
    {
        if (includeFIRGroupDelay)
        {
            this->elementPredelayCount[i] = -std::round(
                this->array.elementArray[i].timeDelay / Ts + (firLength - 1.0) / 2.0
            );
        }
        else
        {
            this->elementPredelayCount[i] = -std::round(this->array.elementArray[i].timeDelay / Ts);
        }
        this->elementPredelayTime[i] = this->elementPredelayCount[i] * Ts;
    }

    *elementPredelayTime = this->elementPredelayTime;
    *elementPredelayCount = this->elementPredelayCount;
    *channelName = this->elementChannelName;

    int minPredelay = (*elementPredelayCount)[0];

    /* find min predelay count */

    for (uint64_t i = 0; i < arraySize; i++)
    {
        if ((*elementPredelayCount)[i] < minPredelay)
        {
            minPredelay = (*elementPredelayCount)[i];
        }
    }
    for (uint64_t i = 0; i < arraySize; i++)
    {
        (*elementPredelayCount)[i] -= minPredelay;
        (*elementPredelayTime)[i] = (*elementPredelayCount)[i] * Ts;
    }
}

void vuprs::WidebandBeamformerTemplate::UpdateAndGetElementPredelay(
    double firLength, bool includeFIRGroupDelay, 
    std::vector<int> *elementPredelayCount, 
    std::vector<double> *elementPredelayTime, 
    std::vector<std::string> *channelName)
{
    if (!this->ConfigDone())
    {
        throw std::runtime_error("in [vuprs::WidebandBeamformerTemplate::UpdateAndGetElementPredelay] Signal is empty.");
    }
    this->UpdateElementPredelay_externalFS(firLength, this->fs, includeFIRGroupDelay, 
        elementPredelayCount, elementPredelayTime, channelName);
}

void vuprs::WidebandBeamformerTemplate::UpdateAndGetElementPredelay(
    double firLength, double fs, bool includeFIRGroupDelay, 
    std::vector<int> *elementPredelayCount, 
    std::vector<double> *elementPredelayTime, 
    std::vector<std::string> *channelName)
{
    this->UpdateElementPredelay_externalFS(firLength, fs, includeFIRGroupDelay, 
        elementPredelayCount, elementPredelayTime, channelName);
}

bool vuprs::WidebandBeamformerTemplate::ScanForPositionPower(
    std::vector<double> *res, 
    const std::vector<double> &alt, const std::vector<double> &az, 
    double frequency, double waveVelocity)
{
    if (res == nullptr)
    {
        throw std::runtime_error("in [ScanForPositionPower] Result pointer cannot be NULL.");
    }
    if (!this->ConfigDone())
    {
        throw std::runtime_error("in [ScanForPositionPower] Config not complete.");
    }
    if (alt.size() != az.size())
    {
        throw std::runtime_error("in [ScanForPositionPower] Altitude and azimuth size mismatch.");
    }
    if (this->is_covMatrixEmpty)
    {
        return false;
    }

    /* STEP 1: Get covariance matrix for the specified frequency */

    int freqIndex = static_cast<int>(std::round(frequency / this->fs * this->signalPoints));

    if (freqIndex < 0 || freqIndex >= this->estimate_covMatrix.size())
    {
        freqIndex = this->estimate_covMatrix.size() / 2 - 1;  /* Set to the closest frequency index (N/2 - 1) if out of range */
    }

    double realFrequency = this->signalFrequencyList(freqIndex);
    Eigen::Matrix<Eigen::dcomplex, -1, -1> R = this->estimate_covMatrix[freqIndex];

    double M = R.cols();  /* Element counts */

    /* STEP 2: Get steering vectors for all alt & az */

    Eigen::Matrix<Eigen::dcomplex, -1, -1> weights;  /* size = M x numScans */
    this->scan_array.GetSteeringVectorMatrix(&weights, alt, az, realFrequency, waveVelocity);

    weights /= M;  /* for CBF: w = ps/M */

    /* STEP 3: Eigenvalue decomposition */

    Eigen::Matrix<double, -1, 1> gamma;
    Eigen::Matrix<Eigen::dcomplex, -1, -1> U;  /* R = U * gamma * U.H */
    vuprs::EigenvalueDecomposition(R, &gamma, &U);

    Eigen::Matrix<Eigen::dcomplex, -1, -1> B = U * gamma.cwiseSqrt().asDiagonal();  /* R = U * gamma * U.H = B * B.H */

    /* STEP 4: Calculate power for each alt & az */

    Eigen::Matrix<Eigen::dcomplex, -1, -1> Y = B.adjoint() * weights;  /* Y = B.H * w */
    Eigen::Matrix<double, -1, 1> power = Y.colwise().norm().transpose();  /* power(i) = norm(Y.col(i)) */

    /* STEP 5: Convert to result */

    vuprs::eigenVector2stdVector(power, res);
    return true;
}

void vuprs::WidebandBeamformerTemplate::CalculateBeamforming()
{
    if (!this->CalculateEnable())
    {
        throw std::runtime_error("in [WidebandBeamformerTemplate::CalculateBeamforming] Cannot calculate beam forming at that time.");
    }

    int numFreqs = this->estimate_covMatrix.size();
    if (numFreqs == 0) return;
    
    int numElements = this->array.elementArray.size();
    this->resultWeightVectors.resize(numElements, numFreqs);
    
    std::vector<std::future<void>> futures;
    futures.reserve(numFreqs);
    
    for (int i = 0; i < numFreqs; i++)
    {
        if (i == 0 || i == numFreqs - 1)  /* X(0) & X(N/2) */
        {
            this->resultWeightVectors.col(i) *= 0;
            continue;
        }
        futures.emplace_back(this->threadPool->enqueue(
            [this, i]() {this->CalculateBeamformingForOneFreq(i);}
        ));
    }
    for (auto &f : futures) f.get();
}
