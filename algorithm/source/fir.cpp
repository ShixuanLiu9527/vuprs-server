#include "fir.h"

vuprs::FIRCalculator::FIRCalculator()
{
    this->firLength = 0;
    this->freqRange_l = 0.0;
    this->freqRange_u = 1000000.0;
    this->lastSignalPoints = 0;
    this->configdone = false;
}

vuprs::FIRCalculator::~FIRCalculator()
{

}

uint32_t vuprs::FIRCalculator::FIRLength() const
{
    return this->firLength;
}

bool vuprs::FIRCalculator::ConfigFIRFromJsonFile(const std::string &jsonFilename)
{
    std::ifstream arrayConfigJsonFile;

    arrayConfigJsonFile.open(jsonFilename);
    if (!arrayConfigJsonFile.is_open())
    {
        throw std::runtime_error("in [FIRCalculator::ConfigFIRFromJsonFile] Cannot open file: " + jsonFilename);
    }

    nlohmann::json configJsonData;

    try
    {
        arrayConfigJsonFile >> configJsonData;
    }
    catch(const std::exception& e)
    {
        throw std::runtime_error("in [FIRCalculator::ConfigFIRFromJsonFile] Failed to load array data from: " + jsonFilename);
    }

    vuprs::__JsonStringParseINT<uint32_t>(&this->firLength, configJsonData, "length", true);

    this->configdone = true;
    return true;
}

void vuprs::FIRCalculator::SetFrequencyRange(double lower, double upper)
{
    if (lower >= upper)
    {
        throw std::runtime_error("in [FIRCalculator::SetFrequencyRange] Lower >= Upper.");
    }
    this->freqRange_l = lower;
    this->freqRange_u = upper;
}

bool vuprs::FIRCalculator::SolveCoeffUseExpectedFrequencyResponse(const Eigen::Matrix<Eigen::dcomplex, -1, -1> &response, double fs)
{
    if (!this->configdone)
    {
        throw std::runtime_error("in [FIRCalculator::SolveCoeffUseExpectedFrequencyResponse] Config not complete.");
    }

    int M = response.rows();  /* M */
    int N_2_plus_1 = response.cols();  /* N/2+1 */
    int N = (N_2_plus_1 - 1) * 2;  /* N */
    
    if (N_2_plus_1 < this->firLength)
    {
        throw std::runtime_error("in [FIRCalculator::SolveCoeffUseExpectedFrequencyResponse] Too little response for solving.");
    }

    if (this->lastSignalPoints != N)
    {
        vuprs::Get_FIR_EXPMatrix(this->firLength, N, &this->matrixE, false);
        this->lastSignalPoints = N;
    }

    Eigen::Matrix<Eigen::dcomplex, -1, 1> h;
    Eigen::Matrix<double, -1, 1> W_vec;

    Eigen::Matrix<double, -1, 1> h_real;

    W_vec.resize(N_2_plus_1, 1);   /* length = N / 2 + 1 */
    W_vec.setOnes();

    W_vec *= 0.01;
    
    for (int i = 0; i < N_2_plus_1; i++)  /* for positive frequency */
    {
        double fk = fs * i / N;
        if (fk >= this->freqRange_l && fk <= this->freqRange_u)
        {
            W_vec(i, 0) = 1.0;
        }
    }

    vuprs::CompleteSymmetric(&W_vec);  /* Montage */

    if (W_vec.rows() != N)
    {
        throw std::runtime_error("in [FIRCalculator::SolveCoeffUseExpectedFrequencyResponse] Internal error.");
    }

    Eigen::Matrix<Eigen::dcomplex, -1, -1> EH = this->matrixE.adjoint();
    Eigen::Matrix<Eigen::dcomplex, -1, -1> W_E = W_vec.asDiagonal() * this->matrixE;
    Eigen::Matrix<Eigen::dcomplex, -1, -1> EH_W_E = EH * W_E;  /* E.H * W * E */
    Eigen::Matrix<Eigen::dcomplex, -1, -1> EH_W = EH * W_vec.asDiagonal();  /* E.H * W */
    Eigen::Matrix<Eigen::dcomplex, -1, 1> EH_W_Hd;  /* E.H * W * Hd */
    Eigen::Matrix<Eigen::dcomplex, -1, 1> Hd;

    this->firCoefficient.resize(M);
    this->maxAbsCoefficient = 0;
    double channelMaxCoefficient = 0.0;

    for (int i = 0; i < M; i++)
    {
        this->firCoefficient[i].resize(this->firLength);

        Hd = response.row(i).transpose();
        
        vuprs::CompleteConjugateSymmetric(&Hd);

        EH_W_Hd = EH_W * Hd;

        h = EH_W_E.ldlt().solve(EH_W_Hd);

        if (h.imag().norm() > 1e-5)
        {
            return false;
        }

        h_real = h.real();

        channelMaxCoefficient = h_real.array().abs().maxCoeff();
        if (channelMaxCoefficient > this->maxAbsCoefficient)
        {
            this->maxAbsCoefficient = channelMaxCoefficient;
        }

        vuprs::eigenVector2stdVector<double>(h_real, &this->firCoefficient[i]);
    }

    return true;
}

void vuprs::FIRCalculator::GetZeroFIRBankCoefficient(std::vector<std::vector<double>> *dst, uint32_t channelNumber) const
{
    if (!this->configdone)
    {
        throw std::runtime_error("in [FIRCalculator::GetZeroFIRBankCoefficient] Config not complete.");
    }
    if (this->firLength == 0)
    {
        throw std::runtime_error("in [FIRCalculator::GetZeroFIRBankCoefficient] FIR length = 0");
    }
    dst->resize(channelNumber, std::vector<double>(this->firLength, 0.0));
}

void vuprs::FIRCalculator::GetFIRBankCoefficient(std::vector<std::vector<double>> *dst) const
{
    *dst = this->firCoefficient;
}

double vuprs::FIRCalculator::MaxAbsoluteFIRCoefficient() const
{
    return this->maxAbsCoefficient;
}
