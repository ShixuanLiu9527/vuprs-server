#include "config.h"
#include "algorithm/bf/beam_former_template.h"
#include "logger/log_manager.h"

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
        int arraySize = this->array.size();
        this->elementChannelName.resize(arraySize);
        this->elementPredelayTime.resize(arraySize);
        this->elementPredelayCount.resize(arraySize);
        for (int i = 0; i < arraySize; i++)
        {
            this->elementChannelName[i] = this->array[i].adcChannel;
        }
        this->is_arrayConfigDone = true;
        this->scan_array.LoadArrayFromJson(arrayConfigJsonFilename); /* Load scan array, which has same element position as array but different time delay */
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
    RUNTIME_CHECK(this->ConfigDone(), "bf", "Config not complete");
    return this->array.size();
}

void vuprs::WidebandBeamformerTemplate::InputSignal(const vuprs::SignalData &signal)
{
    RUNTIME_CHECK(this->ConfigDone(), "bf", "Config not complete");
    this->array.InputElementSignal(signal);
    this->is_signalEmpty = false;
    if (signal.signalPoints != this->signalPoints && std::abs(signal.samplingFrequency - this->fs) > 1e-3)
    {
        this->fs = signal.samplingFrequency;
        this->signalPoints = signal.signalPoints;
        this->signalFrequencyList = vuprs::GenerateRealFrequencyList(this->signalPoints, this->fs);
        this->signalFrequencyList_complex = vuprs::GenerateComplexFrequencyList(this->signalPoints, this->fs);
        this->array.GetSteeringVectorMatrix(&this->steeringVectors); /* Get steering vectors */
    }
}

void vuprs::WidebandBeamformerTemplate::SetTargetDirection(double alt, double az, double waveVelocity)
{
    RUNTIME_CHECK(this->ConfigDone(), "bf", "Config not complete");
    this->array.UpdateTimeDelay(alt, az, waveVelocity); /* Update time delay */
    if (this->fs > 0.0 && this->signalPoints > 0)
    {
        this->array.GetSteeringVectorMatrix(&this->steeringVectors); /* Get steering vectors */
    }
}

void vuprs::WidebandBeamformerTemplate::UpdateCovarianceMatrix()
{
    RUNTIME_CHECK(this->ConfigDone(), "bf", "Config not complete");
    RUNTIME_CHECK(!this->is_signalEmpty, "bf", "Signal is empty.");
    this->array.GetArraySignalMatrix(&this->snap_signalMatrix_freqDomain, nullptr, true); /* Get array signal */
    int dataPoints = this->snap_signalMatrix_freqDomain.cols();
    if (this->estimate_covMatrix.size() <= 0)
    {
        this->estimate_covMatrix.resize(dataPoints);
    }
    else if (this->estimate_covMatrix.size() != dataPoints)
    {
        RUNTIME_CHECK(false, "bf", "Data points in snapshot != latest");
    }
    if (this->mean_covMatrix.size() <= 0)
    {
        this->mean_covMatrix.resize(dataPoints);
    }
    else if (this->mean_covMatrix.size() != dataPoints)
    {
        RUNTIME_CHECK(false, "bf", "Data points in snapshot != latest");
    }
    /* Update mean covariance matrix */
    for (int i = 0; i < dataPoints; i++)
    {
        Eigen::Matrix<Eigen::dcomplex, -1, 1> snapshotFreqSignal = this->snap_signalMatrix_freqDomain.col(i);         /* each col of Snapshot Signal Matrix */
        Eigen::Matrix<Eigen::dcomplex, -1, -1> snapshotCovMatrix = snapshotFreqSignal * snapshotFreqSignal.adjoint(); /* cov matrix for each frequency band */
        if (this->firstSnapshot)
        {
            this->mean_covMatrix[i] = snapshotCovMatrix;
        }
        else
        {
            this->mean_covMatrix[i] = this->EXP_WEIGHTED_MOVING_AVERAGE_INDEX * snapshotCovMatrix +
                                      this->EXP_WEIGHTED_MOVING_AVERAGE_INDEX_1 * this->mean_covMatrix[i];
        }
    }
    /* Update estimate covariance matrix */
    for (int i = 0; i < dataPoints; i++)
    {
        if (i <= 1 || i == dataPoints - 1)
        {
            this->estimate_covMatrix[i] = this->mean_covMatrix[i];
            continue;
        }
        this->estimate_covMatrix[i] = this->ADJACENT_FREQ_AVERAGE_INDEX_1 * this->mean_covMatrix[i - 1] +
                                      this->ADJACENT_FREQ_AVERAGE_INDEX * this->mean_covMatrix[i] +
                                      this->ADJACENT_FREQ_AVERAGE_INDEX_1 * this->mean_covMatrix[i + 1];
    }
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
    this->EXP_WEIGHTED_MOVING_AVERAGE_INDEX = 2.0 / double(this->COVARIANCE_SNAP_WINDOW_SIZE + 1); /* N = 2/a - 1 */
    this->EXP_WEIGHTED_MOVING_AVERAGE_INDEX_1 = 1.0 - this->EXP_WEIGHTED_MOVING_AVERAGE_INDEX;     /* 1 - a */
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
    RUNTIME_CHECK(dst && channelName, "bf", "Destination cannot be NULL");
    RUNTIME_CHECK(!this->is_signalEmpty, "bf", "No signal input");
    if (!considerPredelay)
    {
        *dst = this->resultWeightVectors;
        return;
    }
    uint64_t M = this->array.size();
    /* [T1, T2, ..., TM] */
    Eigen::Map<const Eigen::VectorXd> Tm_vec(this->elementPredelayTime.data(), M);
    /* [ exp(j*2*pi*fk*Tm) ] */
    Eigen::Matrix<Eigen::dcomplex, -1, -1> exp_arg = (Tm_vec * this->signalFrequencyList_complex.transpose() * 2.0 * PI).array().exp();
    /* w*(fk) x exp(j*2*pi*fk*Tm) */
    *dst = this->resultWeightVectors.array().conjugate() * exp_arg.array();
    *channelName = this->elementChannelName;
    /* set 0 & nyquist to real */
    int nyquist_idx = dst->cols() - 1;
    dst->col(0).imag().setZero();
    dst->col(nyquist_idx).imag().setZero();
}

void vuprs::WidebandBeamformerTemplate::UpdateElementPredelay_externalFS(double firLength,
                                                                         double fs,
                                                                         bool includeFIRGroupDelay,
                                                                         std::vector<int> *elementPredelayCount,
                                                                         std::vector<double> *elementPredelayTime,
                                                                         std::vector<std::string> *channelName)
{
    RUNTIME_CHECK(this->ConfigDone(), "bf", "No signal input");
    RUNTIME_CHECK(elementPredelayTime && elementPredelayCount && channelName, "bf", "Predelay pointer is NULL");
    /* Calculate predelay */
    uint64_t M = this->array.size();
    double Ts = 1.0 / fs;
    int minPredelayCount = INT_MAX;
    for (uint64_t i = 0; i < M; i++)
    {
        this->elementPredelayCount[i] = includeFIRGroupDelay ? -std::round(this->array[i].timeDelay / Ts + (firLength - 1.0) / 2.0) : -std::round(this->array[i].timeDelay / Ts);
        minPredelayCount = std::min(this->elementPredelayCount[i], minPredelayCount);
    }
    for (uint64_t i = 0; i < M; i++)
    {
        this->elementPredelayCount[i] -= minPredelayCount;
        this->elementPredelayTime[i] = this->elementPredelayCount[i] * Ts;
    }
    *elementPredelayTime = this->elementPredelayTime;
    *elementPredelayCount = this->elementPredelayCount;
    *channelName = this->elementChannelName;
}

void vuprs::WidebandBeamformerTemplate::UpdateAndGetElementPredelay(double firLength,
                                                                    bool includeFIRGroupDelay,
                                                                    std::vector<int> *elementPredelayCount,
                                                                    std::vector<double> *elementPredelayTime,
                                                                    std::vector<std::string> *channelName)
{
    RUNTIME_CHECK(this->ConfigDone(), "bf", "Signal is empty");
    this->UpdateElementPredelay_externalFS(firLength,
                                           this->fs,
                                           includeFIRGroupDelay,
                                           elementPredelayCount,
                                           elementPredelayTime,
                                           channelName);
}

void vuprs::WidebandBeamformerTemplate::UpdateAndGetElementPredelay(double firLength,
                                                                    double fs,
                                                                    bool includeFIRGroupDelay,
                                                                    std::vector<int> *elementPredelayCount,
                                                                    std::vector<double> *elementPredelayTime,
                                                                    std::vector<std::string> *channelName)
{
    this->UpdateElementPredelay_externalFS(firLength,
                                           fs, includeFIRGroupDelay,
                                           elementPredelayCount,
                                           elementPredelayTime,
                                           channelName);
}

bool vuprs::WidebandBeamformerTemplate::ScanForPositionPower(std::vector<double> *res,
                                                             double *maxValue,
                                                             double *minValue,
                                                             const std::vector<double> &alt,
                                                             const std::vector<double> &az,
                                                             double waveVelocity,
                                                             bool needRegenerate, bool log)
{
    RUNTIME_CHECK(res, "bf", "Result pointer cannot be NULL");
    RUNTIME_CHECK(this->ConfigDone(), "bf", "Config not complete");
    RUNTIME_CHECK(alt.size() == az.size(), "bf", "Altitude and azimuth size mismatch");
    if (this->is_covMatrixEmpty)
    {
        return false;
    }

    /* STEP 0: Prepare for data */
    double M = this->array.size();                                                   /* Element counts */
    int k = alt.size();                                                              /* Scan points counts */
    int f_count = this->estimate_covMatrix.size();                                   /* Frequency counts */
    Eigen::Matrix<double, 1, -1> totalPower = Eigen::Matrix<double, 1, -1>::Zero(k); /* Total power for each scan position */
    if (needRegenerate)
    {
        std::lock_guard<std::mutex> lock(this->mut_scan);
        this->imagTimedelay = this->scan_array.GetImagTimedelay(alt, az, waveVelocity);
    }
    std::mutex mut_total; /* Mutex for totalPower */
    std::vector<std::future<void>> futures;
    for (int i = 0; i < f_count; i++)
    {
        /* Calculate power (in each alt & az) for frequency index i */
        futures.emplace_back(this->threadPool->enqueue(
            [this, i, M, &totalPower, &mut_total]()
            {
                /* STEP 1: Get covariance matrix for the specified frequency */
                double f;
                Eigen::Matrix<Eigen::dcomplex, -1, -1> R;
                {
                    std::lock_guard<std::mutex> lock(this->mut);
                    f = this->signalFrequencyList(i);
                    R = this->estimate_covMatrix[i];
                }
                /* STEP 2: Eigenvalue decomposition */
                Eigen::Matrix<Eigen::dcomplex, -1, -1> B; /* R = B * B.H */
                vuprs::CholeskyDecomposition(R, &B);
                /* STEP 3: Get steering vectors and weights for all alt & az in frequency i */
                Eigen::Matrix<Eigen::dcomplex, -1, -1> weights;
                {
                    std::lock_guard<std::mutex> lock(this->mut_scan);
                    weights = -2.0 * PI * f * this->imagTimedelay;
                }
                weights.array() = weights.array().exp(); /* ps = [..., exp(-j * 2 * pi * f * Tm), ...] */
                weights /= M;                            /* for CBF: w = ps/M */
                /* STEP 4: Calculate power for each alt & az */
                Eigen::Matrix<Eigen::dcomplex, -1, -1> Y = B.adjoint() * weights; /* Y = B.H * w */
                Eigen::Matrix<double, 1, -1> power = Y.colwise().squaredNorm();   /* power(i) = ||Y.col(i)||^2 */
                /* STEP 5: add to result */
                {
                    std::lock_guard<std::mutex> lock(mut_total);
                    totalPower += power;
                }
            }));
    }

    for (auto &f : futures)
        f.get();

    /* Convert to result */
    double maxPower = totalPower.maxCoeff();
    double minPower = totalPower.minCoeff();
    if (log)
    {
        if (minPower > 0.0)
        {
            totalPower = totalPower.array().log10() * 20.0; /* Convert to dB */
        }
        else
        {
            totalPower.setZero();
        }
        maxPower = totalPower.maxCoeff();
        minPower = totalPower.minCoeff();
    }
    vuprs::eigenRow2stdVector(totalPower, res);
    if (maxValue != nullptr)
        *maxValue = maxPower;
    if (minValue != nullptr)
        *minValue = minPower;
    return true;
}

void vuprs::WidebandBeamformerTemplate::CalculateBeamforming()
{
    RUNTIME_CHECK(this->CalculateEnable(), "bf", "Cannot calculate beam forming at that time");
    int numFreqs = this->estimate_covMatrix.size(); /* = N / 2 + 1 */
    if (numFreqs == 0)
        return;
    int M = this->array.size();
    std::vector<std::future<void>> futures;
    this->resultWeightVectors.resize(M, numFreqs);
    futures.reserve(numFreqs);
    for (int i = 0; i < numFreqs; i++)
    {
        if (i == 0)
        {
            this->resultWeightVectors.col(i) *= 0;
            continue;
        }
        else if (i == numFreqs - 1) /* for Nyquist frequency */
        {
            Eigen::Matrix<Eigen::dcomplex, -1, 1> ps = this->steeringVectors.col(i); /* ps */
            this->resultWeightVectors.col(i) = ps / M;
            this->resultWeightVectors.col(i).imag().setZero();
            continue;
        }
        futures.emplace_back(this->threadPool->enqueue(
            [this, i]()
            { this->CalculateBeamformingForOneFreq(i); }));
    }
    for (auto &f : futures)
        f.get();
}
