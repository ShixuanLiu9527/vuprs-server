#include "algorithm/bf/fir.h"
#include "logger/log_manager.h"

vuprs::FIRCalculator::FIRCalculator()
{
    this->fir_length = 0;
    this->freq_range_l = 0.0;
    this->freq_range_u = 1000000.0;
    this->lastSignalPoints = 0;
    this->config_done = false;

    this->thread_pool = std::make_unique<vuprs::ThreadPool>(std::thread::hardware_concurrency());
}

vuprs::FIRCalculator::~FIRCalculator()
{
}

uint32_t vuprs::FIRCalculator::FIRLength() const
{
    return this->fir_length;
}

bool vuprs::FIRCalculator::ConfigFIRFromJsonFile(const std::string &json_filename)
{
    std::ifstream f;
    f.open(json_filename);
    RUNTIME_CHECK(f.is_open(), "bf", " in [FIRCalculator::ConfigFIRFromJsonFile] Cannot open file: " + json_filename);
    nlohmann::json json_data;
    try
    {
        f >> json_data;
    }
    catch (const std::exception &e)
    {
        RUNTIME_CHECK(false, "bf", " in [FIRCalculator::ConfigFIRFromJsonFile] Failed to load array data from: " + json_filename);
    }
    vuprs::__JsonStringParseINT<uint32_t>(&this->fir_length, json_data, "length", true);
    this->config_done = true;
    return true;
}

void vuprs::FIRCalculator::SetFrequencyRange(double lower, double upper)
{
    PARAM_CHECK(lower < upper, "bf", " in [FIRCalculator::SetFrequencyRange] Lower >= Upper.");
    this->freq_range_l = lower;
    this->freq_range_u = upper;
}

bool vuprs::FIRCalculator::SolveCoeffUseExpectedFrequencyResponse(const Eigen::Matrix<Eigen::dcomplex, -1, -1> &response,
                                                                  const std::vector<std::string> &channel_name, double fs)
{
    PARAM_CHECK(this->config_done, "bf", " in [FIRCalculator::SolveCoeffUseExpectedFrequencyResponse] Config not complete.");
    int M = response.rows();          /* M */
    int N_2_plus_1 = response.cols(); /* N/2+1 */
    int N = (N_2_plus_1 - 1) * 2;     /* N */
    PARAM_CHECK(N >= this->fir_length, "bf", " in [FIRCalculator::SolveCoeffUseExpectedFrequencyResponse] Too little response for solving.");
    this->fir_coefficient.resize(M);
    for (auto &val : this->fir_coefficient)
        val.resize(this->fir_length);
    this->max_abs_coefficient = 0.0;
    std::vector<std::future<void>> futures;
    futures.reserve(M);
    if (this->fft_managers.size() != M)
    {
        this->fft_managers.resize(M);
    }
    for (auto &manager : this->fft_managers)
    {
        manager.SetParameters(N, false); /* set to: N & ifft */
    }
    for (int i = 0; i < M; i++)
    {
        futures.emplace_back(this->thread_pool->enqueue(
            [&, i]()
            {
                Eigen::Matrix<Eigen::dcomplex, -1, 1> Hd = response.row(i).transpose();
                /* Remap: element FIR coef to channel FIR coef */
                int dst_index = vuprs::FindValueInVec(vuprs::ADC_CHANNEL_ADDR_MAP, channel_name[i]);
                Eigen::Matrix<Eigen::dcomplex, -1, 1> h(N, 1);
                std::vector<double> h_real_vec;
                /* add filter */
                vuprs::ApplyBandpassWindow(&Hd, this->freq_range_l, this->freq_range_u, fs, N);
                /* N/2+1 to N */
                vuprs::CompleteConjugateSymmetric(&Hd);
                /* IFFT */
                this->fft_managers[i].DoDFT(Hd.data(), h.data());
                h /= (double)N;
                /* Get Real */
                Eigen::Matrix<double, -1, 1> h_real = h.real();
                /* IFFT shift */
                vuprs::ifftshift(&h_real);
                /* resize to L */
                Eigen::Index start = (N - this->fir_length) / 2;
                Eigen::Matrix<double, -1, 1> h_cut = h_real.segment(start, this->fir_length);
                /* Add window */
                vuprs::AddWindow<double>(&h_cut, vuprs::WindowType::SIG_WINDOW__HANN);
                h_cut.array() -= h_cut.mean(); /* Delete DC gain */
                /* convert to std::vector */
                vuprs::eigenVector2stdVector<double>(h_cut, &h_real_vec);
                /* reverse FIR coef */
                std::reverse(h_real_vec.begin(), h_real_vec.end());
                /* dump */
                double channel_max = h_cut.array().abs().maxCoeff();
                {
                    std::unique_lock<std::mutex> lock(this->mtx); /* LOCK */
                    this->max_abs_coefficient = std::max(channel_max, this->max_abs_coefficient);
                    this->fir_coefficient[dst_index] = h_real_vec;
                }
            }));
    }
    for (auto &f : futures)
        f.get();
    return true;
}

void vuprs::FIRCalculator::GetZeroFIRBankCoefficient(std::vector<std::vector<double>> *dst, uint32_t channel_number) const
{
    PARAM_CHECK(this->config_done, "bf", " in [FIRCalculator::GetZeroFIRBankCoefficient] Config not complete.");
    PARAM_CHECK(this->fir_length > 0, "bf", " in [FIRCalculator::GetZeroFIRBankCoefficient] FIR length = 0");
    dst->resize(channel_number, std::vector<double>(this->fir_length, 0.0));
}

void vuprs::FIRCalculator::GetFIRBankCoefficient(std::vector<std::vector<double>> *dst) const
{
    *dst = this->fir_coefficient;
}

double vuprs::FIRCalculator::MaxAbsoluteFIRCoefficient() const
{
    return this->max_abs_coefficient;
}
