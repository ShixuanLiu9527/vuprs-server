#include "beam_former_template.h"

vuprs::WidebandBeamformerTemplate::WidebandBeamformerTemplate()
{
    this->fs = 0.0;

    this->is_arrayConfigDown = false;
    this->is_beamfomerConfigDown = false;

    this->is_signalEmpty = true;
    this->is_covMatrixEmpty = true;

    this->threadPool = std::make_unique<vuprs::ThreadPool>(std::thread::hardware_concurrency());
    
    this->firstSnapshot = true;
    this->COVARIANCE_SNAP_WINDOW_SIZE = DEFAULT_COVARIANCE_SNAP_WINDOW_SIZE;
    this->ADJACENT_FREQ_AVERAGE_INDEX = DEFAULT_ADJACENT_FREQ_AVERAGE_INDEX;
    this->UpdateParameters();
}

bool vuprs::WidebandBeamformerTemplate::ConfigArrayFromJson(const std::string &arrayConfigJsonFilename)
{
    if (this->array.LoadArrayFromJson(arrayConfigJsonFilename))
    {
        int arraySize = this->array.elementArray.size();
        this->elementChannelName.resize(arraySize);
        this->elementPredelay.resize(arraySize);
        this->elementPredelayCount.resize(arraySize);
        for (int i = 0; i < arraySize; i++)
        {
            this->elementChannelName[i] = this->array.elementArray[i].adcChannel;
        }
        this->is_arrayConfigDown = true;
        return true;
    }
    return false;
}

bool vuprs::WidebandBeamformerTemplate::ConfigDown() const
{
    return this->is_arrayConfigDown && this->is_beamfomerConfigDown;
}

bool vuprs::WidebandBeamformerTemplate::CalculateEnable() const
{
    return this->ConfigDown() && !this->is_signalEmpty && !this->is_covMatrixEmpty;
}

bool vuprs::WidebandBeamformerTemplate::PredelayEnable() const
{
    return this->ConfigDown() && !this->is_signalEmpty;
}

void vuprs::WidebandBeamformerTemplate::InputSignal(const vuprs::SignalData &signal)
{
    if (!this->ConfigDown())
    {
        throw std::runtime_error("Config not complete.");
    }

    this->array.InputElementSignal(signal);
    this->fs = signal.samplingFrequency;
    this->is_signalEmpty = false;
}

void vuprs::WidebandBeamformerTemplate::SetTargetDirection(double alt, double az, double waveVelocity)
{
    if (!this->ConfigDown())
    {
        throw std::runtime_error("Config not complete.");
    }
    if (this->is_signalEmpty)
    {
        throw std::runtime_error("Signal is empty.");
    }

    this->array.UpdateTimeDelay(alt, az, waveVelocity);  /* Update time delay */
    this->array.GetSteeringVectorMatrix(&this->steeringVectors);  /* Get steering vectors */
}

void vuprs::WidebandBeamformerTemplate::UpdateCovarianceMatrix()
{
    if (!this->ConfigDown())
    {
        throw std::runtime_error("Config not complete.");
    }
    if (this->is_signalEmpty)
    {
        throw std::runtime_error("Signal is empty.");
    }

    this->array.GetArraySignalMatrix(&this->snap_signalMatrix_freqDomain, nullptr, true);  /* Get array signal */

    int dataPoints = this->snap_signalMatrix_freqDomain.cols();

    if (this->estimate_covMatrix.size() <= 0)
    {
        this->estimate_covMatrix.resize(dataPoints);
    }
    else if (this->estimate_covMatrix.size() != dataPoints)
    {
        throw std::runtime_error("Data points in snapshot != latest");
    }

    if (this->mean_covMatrix.size() <= 0)
    {
        this->mean_covMatrix.resize(dataPoints);
    }
    else if (this->mean_covMatrix.size() != dataPoints)
    {
        throw std::runtime_error("Data points in snapshot != latest");
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
    this->estimate_covMatrix.clear();
}

void vuprs::WidebandBeamformerTemplate::GetWeightVectorValues(Eigen::Matrix<Eigen::dcomplex, -1, -1> *dst) const
{
    *dst = this->resultWeightVectors;
}

void vuprs::WidebandBeamformerTemplate::GetElementPredelay(
    double firLength, bool includeFIRGroupDelay, 
    std::vector<int> *elementPredelayCount, 
    std::vector<double> *elementPredelay, 
    std::vector<std::string> *channelName)
{
    if (this->is_signalEmpty)
    {
        throw std::runtime_error("No signal input.");
    }

    /* Calculate predelay */

    int arraySize = this->array.elementArray.size(), minPredelay = 0;
    double Ts = 1.0 / this->fs;

    for (int i = 0; i < arraySize; i++)
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
        
        if (this->elementPredelayCount[i] < minPredelay)
        {
            minPredelay = this->elementPredelayCount[i];
        }
    }

    for (int i = 0; i < arraySize; i++)
    {
        if (minPredelay < 0)
        {
            this->elementPredelayCount[i] = this->elementPredelayCount[i] - minPredelay;
        }
        this->elementPredelay[i] = this->elementPredelayCount[i] * Ts;
    }

    *elementPredelayCount = this->elementPredelayCount;
    *elementPredelay = this->elementPredelay;
    *channelName = this->elementChannelName;
}

void vuprs::WidebandBeamformerTemplate::CalculateBeamforming()
{
    if (!this->CalculateEnable())
    {
        throw std::runtime_error("Cannot calculate beam forming at that time.");
    }

    int numFreqs = this->estimate_covMatrix.size();
    if (numFreqs == 0) return;
    
    int numElements = this->array.elementArray.size();
    this->resultWeightVectors.resize(numElements, numFreqs);
    
    std::vector<std::future<void>> futures;
    futures.reserve(numFreqs);
    
    for (int i = 0; i < numFreqs; i++)
    {
        futures.emplace_back(threadPool->enqueue(
            [this, i]() {this->CalculateBeamformingForOneFreq(i);}
        ));
    }
    
    for (auto &f : futures) 
    {
        f.get();
    }
}
