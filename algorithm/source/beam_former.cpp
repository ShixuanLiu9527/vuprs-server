#include "beam_former.h"

/* --------------------------------------------------------------------------------------------------------------- */
/* -------------------------------------------------- CBF -------------------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

void vuprs::BeamFormerCBF::GetOutputSignal(std::vector<std::complex<double>> *outputSignal)
{
    if (this->array.empty())
    {
        throw std::runtime_error("Cannot beam forming use empty array.");
    }

    int elementCount = this->array.elementArray.size();
    Eigen::Matrix<Eigen::dcomplex, -1, 1> totalSignalFrequencyDomain;  /* beam forming output */
    Eigen::Matrix<Eigen::dcomplex, -1, 1> beamFormingFrequencyVector;  /* beam forming frequency vector */
    Eigen::Matrix<Eigen::dcomplex, -1, 1> weightVector;  /* beam forming weight vector */

    totalSignalFrequencyDomain.resize(this->array.signalPointCounts / 2 + 1, 1);
    totalSignalFrequencyDomain.setZero();

    beamFormingFrequencyVector = vuprs::GenerateBeamFormingFrequencyList(this->array.signalPointCounts, this->array.samplingFrequency);  /* 2 * pi * f * j */

    for (int i = 0; i < elementCount; i++)
    {
        this->array.elementArray[i].DoFFT();
        weightVector = (beamFormingFrequencyVector * this->array.timeDelayVector(i, 0)).array().exp().matrix();
        this->array.elementArray[i].phasedElementSignalFrequencyDomain_eigen = (this->array.elementArray[i].elementSignalFrequencyDomain_eigen.array() * weightVector.array()).matrix();
        totalSignalFrequencyDomain += this->array.elementArray[i].phasedElementSignalFrequencyDomain_eigen;
    }

    totalSignalFrequencyDomain /= (double)elementCount;
    vuprs::SignalMontage(&totalSignalFrequencyDomain);

    /* IFFT */

    std::vector<std::complex<double>> ifftInput;
    vuprs::eigenVector2stdVector(&totalSignalFrequencyDomain, &ifftInput);
    vuprs::FFT(&ifftInput, outputSignal, true);
}

/* --------------------------------------------------------------------------------------------------------------- */
/* ------------------------------------------------- MVDR -------------------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

vuprs::BeamFormerMVDR::BeamFormerMVDR()
{
    this->windowSize = DEFAULT_MVDR_FRAME_WINDOW_LENGTH;
    this->frameCovarianceMatrixListWindow.reserve(this->windowSize + 1);
}

void vuprs::BeamFormerMVDR::CalculateSignalCovarianceMatrixInCurrentFrame()
{
    Eigen::Matrix<Eigen::dcomplex, -1, -1> identity;
    double samplingFrequency;

    this->currentSignalMatrix = this->array.ArraySignalMatrix(true, &samplingFrequency);

    int elementCount = this->currentSignalMatrix.rows(), frequencySignalPoints = this->currentSignalMatrix.cols();

    if (elementCount == 0 || frequencySignalPoints == 0)
    {
        throw std::runtime_error("Empty array or empty signal");
    }
    if (this->currentSignalPoints == -1)
    {
        this->currentSignalPoints = frequencySignalPoints;
    }
    else
    {
        if (this->currentSignalPoints != frequencySignalPoints)
        {
            throw std::runtime_error("Different points: " + std::to_string(this->currentSignalPoints) + ", " + std::to_string(frequencySignalPoints));
        }
    }

    std::vector<Eigen::Matrix<Eigen::dcomplex, -1, -1>> currentMatrixList(frequencySignalPoints);
    identity = Eigen::Matrix<Eigen::dcomplex, -1, -1>::Identity(elementCount, elementCount) * VUPRS_EPS_1;

    for (int i = 0; i < frequencySignalPoints; i++)
    {
        currentMatrixList[i] = this->currentSignalMatrix.col(i) * this->currentSignalMatrix.col(i).adjoint() + identity;
    }

    this->frameCovarianceMatrixListWindow.push_back(currentMatrixList);
    if (this->frameCovarianceMatrixListWindow.size() > this->windowSize)
    {
        this->frameCovarianceMatrixListWindow.erase(this->frameCovarianceMatrixListWindow.begin());
    }

    this->CalculateAverageCovarianceMatrix();
}

void vuprs::BeamFormerMVDR::SetWindowSize(int newSize)
{
    if (newSize > 0)
    {
        this->windowSize = newSize;
    }
    else
    {
        this->windowSize = DEFAULT_MVDR_FRAME_WINDOW_LENGTH;
    }

    this->ResetCovarianceMatrixParam();
}

void vuprs::BeamFormerMVDR::CalculateAverageCovarianceMatrix()
{
    if (this->currentSignalPoints == -1)
    {
        return;
    }

    /* Linear weight factor */

    int frameSize = this->frameCovarianceMatrixListWindow.size();

    if (frameSize <= 0)
    {
        return;
    }

    this->averageCovarianceMatrixList.resize(this->currentSignalPoints);
    double totalWeight = (1.0 + (double)frameSize) * (double)frameSize / 2.0;

    for (int i = 0; i < this->currentSignalPoints; i++)
    {
        this->averageCovarianceMatrixList[i].setZero();
        for (int j = 0; j < frameSize; j++)
        {
            double weight = double(j + 1);
            this->averageCovarianceMatrixList[i] += weight * this->frameCovarianceMatrixListWindow[j][i];
        }
        this->averageCovarianceMatrixList[i] /= totalWeight;
    }
}

void vuprs::BeamFormerMVDR::ResetCovarianceMatrixParam()
{
    this->currentSignalPoints = -1;
    std::vector<std::vector<Eigen::Matrix<Eigen::dcomplex, -1, -1>>>().swap(this->frameCovarianceMatrixListWindow);  /* release */
    this->frameCovarianceMatrixListWindow.reserve(this->windowSize);
}

void vuprs::BeamFormerMVDR::GetOutputSignal(std::vector<std::complex<double>> *outputSignal)
{
    this->CalculateSignalCovarianceMatrixInCurrentFrame();

    int frequencySignalPoints = this->currentSignalMatrix.cols(), elements = this->currentSignalMatrix.rows();
    Eigen::Matrix<Eigen::dcomplex, -1, 1> result(frequencySignalPoints), fullResult;
    Eigen::Matrix<Eigen::dcomplex, -1, 1> frequencyList = vuprs::GenerateFrequencyList(this->array.signalPointCounts, this->array.samplingFrequency);
    Eigen::Matrix<Eigen::dcomplex, -1, -1> invCovarianceMatrix;
    Eigen::Matrix<Eigen::dcomplex, -1, 1> arrayResponseVector;
    Eigen::Matrix<Eigen::dcomplex, -1, 1> weightedVector;

    /* Weighted and calculate */

    for (int i = 0; i < frequencySignalPoints; i++)
    {
        invCovarianceMatrix = this->averageCovarianceMatrixList[i].inverse();
        arrayResponseVector = this->array.ArrayResponseVector(frequencyList(i, 0).imag());
        weightedVector = invCovarianceMatrix * arrayResponseVector / (arrayResponseVector.adjoint() * invCovarianceMatrix * arrayResponseVector)(0, 0);
        result(i, 0) = (weightedVector.adjoint() * this->currentSignalMatrix.col(i));
    }

    /* Montage */

    vuprs::SignalMontage(&result);

    /* IFFT */

    std::vector<std::complex<double>> ifftInput;
    vuprs::eigenVector2stdVector(&result, &ifftInput);
    vuprs::FFT(&ifftInput, outputSignal, true);
}
