#include "fir.h"

vuprs::FIRCalculator::FIRCalculator()
{
    this->firLength = 0;
    this->freqRange_l = 0.0;
    this->freqRange_u = 1000000.0;
    this->lastSignalPoints = 0;
    this->configdone = false;

    this->threadPool = std::make_unique<vuprs::ThreadPool>(std::thread::hardware_concurrency());
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

bool vuprs::FIRCalculator::SolveCoeffUseExpectedFrequencyResponse(const Eigen::Matrix<Eigen::dcomplex, -1, -1> &response, 
                const std::vector<std::string> &channelName, double fs)
{
    if (!this->configdone)
    {
        throw std::runtime_error("in [FIRCalculator::SolveCoeffUseExpectedFrequencyResponse] Config not complete.");
    }
    int M = response.rows();  /* M */
    int N_2_plus_1 = response.cols();  /* N/2+1 */
    int N = (N_2_plus_1 - 1) * 2;  /* N */
    if (N < this->firLength)
    {
        throw std::runtime_error("in [FIRCalculator::SolveCoeffUseExpectedFrequencyResponse] Too little response for solving.");
    }

    this->firCoefficient.resize(M);
    for (auto &val: this->firCoefficient) val.resize(this->firLength);

    this->maxAbsCoefficient = 0.0;
    std::vector<std::future<void>> futures;
    std::vector<vuprs::FFTWManagerComplex> fftManagers(M);
    futures.reserve(M);
    for (auto &manager: fftManagers) manager.SetParameters(N, false);  /* set to: N & ifft */
    for (int i = 0; i < M; i++)
    {
        futures.emplace_back(this->threadPool->enqueue([&, i](){
            Eigen::Matrix<Eigen::dcomplex, -1, 1> Hd = response.row(i).transpose();
            /* Remap: element FIR coef to channel FIR coef */
            int dstIndex = vuprs::FindValueInVec(vuprs::ADC_CHANNEL_ADDR_MAP, channelName[i]);
            Eigen::Matrix<Eigen::dcomplex, -1, 1> h(N, 1);
            std::vector<double> h_real_vec;
            /* add filter */
            vuprs::ApplyBandpassWindow(&Hd, this->freqRange_l, this->freqRange_u, fs, N);
            /* N/2+1 to N */
            vuprs::CompleteConjugateSymmetric(&Hd);
            /* IFFT */
            fftManagers[i].DoDFT(Hd.data(), h.data());
            h /= (double)N;
            /* Get Real */
            Eigen::Matrix<double, -1, 1> h_real = h.real();
            /* IFFT shift */
            vuprs::ifftshift(&h_real);
            /* resize to L */
            Eigen::Index start = (N - this->firLength) / 2;
            Eigen::Matrix<double, -1, 1> h_cut = h_real.segment(start, this->firLength);
            /* Add window */
            vuprs::AddWindow<double>(&h_cut, vuprs::WindowType::SIG_WINDOW__HANN);
            h_cut.array() -= h_cut.mean();  /* Delete DC gain */
            /* convert to std::vector */
            vuprs::eigenVector2stdVector<double>(h_cut, &h_real_vec);
            /* reverse FIR coef */
            std::reverse(h_real_vec.begin(), h_real_vec.end());

            /* dump */
            double channelMaxCoefficient = h_cut.array().abs().maxCoeff();
            {
                std::unique_lock<std::mutex> lock(this->mtx);  /* LOCK */
                this->maxAbsCoefficient = std::max(channelMaxCoefficient, this->maxAbsCoefficient);
                this->firCoefficient[dstIndex] = h_real_vec;
            }
        }));
    }
    for (auto &f : futures) f.get();
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
