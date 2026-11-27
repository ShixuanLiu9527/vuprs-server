#include "config.h"
#include <algorithm>
#include "misc/file_processing.h"
#include "algo/bf/bf_basic.h"
#include "algo/utils/math_utils.h"
#include "logger/check.h"

/* --------------------------------------------------------------------------------------------------------------- */
/* ------------------------------------------- Beam Forming Element ---------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

vuprs::BeamFormingElement::BeamFormingElement()
{
}

void vuprs::BeamFormingElement::UpdataTimeDelay(double target_alt, double target_az, double wave_velocity)
{
    this->time_delay = -vuprs::AltAz2PointingVector(target_alt, target_az).dot(this->position_vector) / wave_velocity;
}

void vuprs::BeamFormingElement::AddWindowForSignal()
{
    RUNTIME_CHECK(!this->adc_channel.empty(), "bf", "Cannot do FFT in an empty element");
    RUNTIME_CHECK(this->element_signal_time_domain.size() > 0, "bf", "Signal is empty");
    /* Add window */
    vuprs::stdVector2eigenVector<Eigen::dcomplex>(this->element_signal_time_domain, &this->windowed_signal_eigen);
    Eigen::dcomplex mean = this->windowed_signal_eigen.mean(); /* - average */
    this->windowed_signal_eigen.array() -= mean;
    vuprs::AddWindow<Eigen::dcomplex>(&this->windowed_signal_eigen, vuprs::WindowType::SIG_WINDOW__HAMMING);
}

bool vuprs::BeamFormingElement::RunDFT()
{
    this->AddWindowForSignal();
    int N = this->windowed_signal_eigen.rows();
    RUNTIME_CHECK(N > 0, "bf", "Failed to allocate FFTW memory (data size = 0)");
    this->fft_manager.SetParameters(N, true);
    if (this->element_signal_frequency_domain_eigen.rows() != N)
    {
        this->element_signal_frequency_domain_eigen.resize(N, 1);
    }
    this->fft_manager.DoDFT(this->windowed_signal_eigen.data(), this->element_signal_frequency_domain_eigen.data());
    vuprs::CutTheFirstHalf(&this->element_signal_frequency_domain_eigen);
    return true;
}

bool vuprs::BeamFormingElement::empty() const
{
    return this->adc_channel.empty();
}

/* --------------------------------------------------------------------------------------------------------------- */
/* -------------------------------------------- Beam Forming Array ----------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

vuprs::BeamFormingArray::BeamFormingArray()
{
    this->fs = 0.0;
    this->sampling_time = 0.0;
    this->signal_point_counts = 0;
    this->wave_velocity = DEFAULT_WAVE_VELOCITY;
    this->thread_pool = std::make_shared<vuprs::ThreadPool>(std::thread::hardware_concurrency());
}

void vuprs::BeamFormingArray::SetThreadPool(std::shared_ptr<vuprs::ThreadPool> pool)
{
    RUNTIME_CHECK(pool != nullptr, "bf", "Thread pool cannot be NULL");
    this->thread_pool = std::move(pool);
}

vuprs::BeamFormingArrayBase::~BeamFormingArrayBase()
{
    vuprs::AlignedEigenVector<vuprs::BeamFormingElement>().swap(this->element_array);
}

double vuprs::BeamFormingArray::CalculateSteeringVectorErrorRadius(double signal_frequency) const
{
    RUNTIME_CHECK(!this->empty(), "bf", "Array is empty");
    PARAM_CHECK(this->wave_velocity > 0.0, "bf", "Wave velocity must be positive.");
    /* All elements share the same position error radius, so their error terms are identical:
     * e0 = 4 * sum_m sin^2(pi * f * tau) = 4 * M * sin^2(pi * f * tau),
     * tau = max_pos_err / c.
     */
    double timedelay_err = this->max_element_position_error / this->wave_velocity;
    double s = std::sin(PI * signal_frequency * timedelay_err);
    return 4.0 * (double)this->element_array.size() * s * s;
}

bool vuprs::BeamFormingArrayBase::LoadArrayFromJson(const std::string &filename)
{
    std::ifstream f;
    f.open(filename);
    RUNTIME_CHECK(f.is_open(), "bf", "Cannot open file: " + filename);
    nlohmann::json json_data;
    try
    {
        f >> json_data;
    }
    catch (const std::exception &e)
    {
        RUNTIME_CHECK(false, "bf", " Failed to load array data from: " + filename);
    }
    RUNTIME_CHECK(json_data.contains("beam_forming_array"), "bf", "Missing keys.");
    RUNTIME_CHECK(json_data["beam_forming_array"].contains("array"), "bf", "Missing key.");
    RUNTIME_CHECK(json_data["beam_forming_array"].contains("info"), "bf", "Missing key.");
    RUNTIME_CHECK(json_data["beam_forming_array"]["array"].is_array(), "bf", "array_data is not array.");
    auto array_data = json_data["beam_forming_array"]["array"];
    auto info = json_data["beam_forming_array"]["info"];
    int array_size = array_data.size();
    this->element_array.clear();
    this->element_array.reserve(array_size);
    for (int i = 0; i < array_size; i++)
    {
        RUNTIME_CHECK(array_data[i].contains("position-x"), "bf", "Missing key position_x");
        RUNTIME_CHECK(array_data[i].contains("position-y"), "bf", "Missing key position_y");
        RUNTIME_CHECK(array_data[i].contains("position-z"), "bf", "Missing key position_z");
        RUNTIME_CHECK(array_data[i].contains("adc-channel"), "bf", "Missing key adc_channel");
        double x = 0, y = 0, z = 0;
        std::string adc_channel;
        vuprs::BeamFormingElement one_element;
        vuprs::__JsonStringParseFLOAT<double>(&x, array_data[i], "position-x", true);
        vuprs::__JsonStringParseFLOAT<double>(&y, array_data[i], "position-y", true);
        vuprs::__JsonStringParseFLOAT<double>(&z, array_data[i], "position-z", true);
        one_element.position_vector(0, 0) = x;
        one_element.position_vector(1, 0) = y;
        one_element.position_vector(2, 0) = z;
        vuprs::__JsonParseString(&adc_channel, array_data[i], "adc-channel", true);
        one_element.adc_channel = adc_channel;
        RUNTIME_CHECK(IS_ADC_CHANNEL_NAME(adc_channel), "bf", " Invalid ADC channel name in file: " + filename);
        this->element_array.push_back(std::move(one_element));
    }
    /* Every ADC channel must be present exactly once: after sorting, element index == ADC channel index. */
    RUNTIME_CHECK(this->element_array.size() == ADC_CHANNEL_NUMBER,
                  "bf", " Array must contain exactly " + std::to_string(ADC_CHANNEL_NUMBER) + " channels.");
    std::vector<bool> channel_seen(ADC_CHANNEL_NUMBER, false);
    for (const auto &element : this->element_array)
    {
        int channel_index = vuprs::FindValueInVec(vuprs::ADC_CHANNEL_ADDR_MAP, element.adc_channel);
        RUNTIME_CHECK(channel_index >= 0, "bf", " Invalid ADC channel name in file: " + filename);
        RUNTIME_CHECK(!channel_seen[channel_index], "bf", " Duplicate ADC channel name in file: " + filename);
        channel_seen[channel_index] = true;
    }
    /* Sort into ADC channel order (A1..A8, B1..B8). */
    std::stable_sort(this->element_array.begin(),
                     this->element_array.end(),
                     [](const vuprs::BeamFormingElement &lhs, const vuprs::BeamFormingElement &rhs)
                     {
                         return vuprs::FindValueInVec(vuprs::ADC_CHANNEL_ADDR_MAP, lhs.adc_channel) <
                                vuprs::FindValueInVec(vuprs::ADC_CHANNEL_ADDR_MAP, rhs.adc_channel);
                     });
    for (size_t i = 0; i < this->element_array.size(); i++)
    {
        RUNTIME_CHECK(this->element_array[i].adc_channel == vuprs::ADC_CHANNEL_ADDR_MAP[i],
                      "bf", " Array element order mismatch after sorting.");
    }
    vuprs::__JsonStringParseFLOAT<double>(&this->max_element_position_error,
                                          info,
                                          "max-element-position-error-radius",
                                          true);
    return true;
}

void vuprs::BeamFormingArray::UpdateTimeDelay(double target_alt, double target_az, double wave_velocity)
{
    this->wave_velocity = wave_velocity;
    int array_size = this->element_array.size();
    this->time_delay_vector.resize(array_size, 1);
    for (int i = 0; i < array_size; i++)
    {
        this->element_array[i].UpdataTimeDelay(target_alt, target_az, wave_velocity);
        this->time_delay_vector(i, 0).real(this->element_array[i].time_delay);
        this->time_delay_vector(i, 0).imag(0.0);
    }
}

void vuprs::BeamFormingArray::InputElementSignal(vuprs::SignalData &&adc_data)
{
    RUNTIME_CHECK(!this->element_array.empty(), "bf", "Cannot input signal to an empty array.");
    RUNTIME_CHECK(adc_data._channel_data.size() == this->element_array.size(),
                  "bf", " Signal channel count & array element count mismatch.");

    this->fs = adc_data.fs;
    this->sampling_time = adc_data.sampling_time;
    this->signal_point_counts = adc_data.signal_points;
    for (size_t i = 0; i < this->element_array.size(); i++)
    {
        /* element i <-> ADC channel i (sorted at load), so buffers can be handed off by index. */
        adc_data._channel_data[i].swap(this->element_array[i].element_signal_time_domain);
        this->element_array[i].fs = adc_data.fs;
        this->element_array[i].sampling_time = adc_data.sampling_time;
    }
}

bool vuprs::BeamFormingArrayBase::empty() const
{
    return this->element_array.empty();
}

Eigen::Matrix<Eigen::dcomplex, -1, 1> vuprs::BeamFormingArray::GetSteeringVector(double frequency) const
{
    return (-this->time_delay_vector * std::complex<double>(0, 1) * 2.0 * PI * frequency).array().exp().matrix();
}

void vuprs::BeamFormingArray::GetSteeringVectorMatrix(Eigen::Matrix<Eigen::dcomplex, -1, -1> *matrix) const
{
    Eigen::Matrix<Eigen::dcomplex, -1, 1> _j_omega = -2.0 * PI * vuprs::GenerateComplexFrequencyList(this->signal_point_counts, this->fs); /* -j * omega */
    *matrix = (this->time_delay_vector * _j_omega.transpose()).array().exp().matrix();                                                     /* exp(-jwT{i}) */
}

void vuprs::BeamFormingArray::GetArraySignalMatrix(Eigen::Matrix<Eigen::dcomplex, -1, -1> *signal_matrix, double *fs, bool frequency_domain)
{
    uint64_t data_size = this->signal_point_counts;
    uint64_t element_size = this->element_array.size();
    RUNTIME_CHECK(data_size > 0, "bf", "Cannot get array signal matrix (no data input in advance)");
    RUNTIME_CHECK(element_size > 0, "bf", "Cannot get array signal matrix from an empty array");
    if (frequency_domain)
        signal_matrix->resize(this->element_array.size(), data_size / 2 + 1); /* M x (N/2 + 1) */
    else
        signal_matrix->resize(element_size, data_size); /* M x N */
    if (frequency_domain)
    {
        std::vector<std::future<void>> futures;
        futures.reserve(element_size);
        for (uint64_t i = 0; i < element_size; i++)
        {
            futures.emplace_back(this->thread_pool->enqueue(
                [this, i, &signal_matrix]()
                {
                    this->element_array[i].RunDFT();
                    signal_matrix->row(i) = this->element_array[i].element_signal_frequency_domain_eigen.transpose();
                }));
        }
        vuprs::WaitForAllFutures(futures);
    }
    else
    {
        for (uint64_t i = 0; i < element_size; i++)
        {
            signal_matrix->row(i) = Eigen::Map<Eigen::Matrix<Eigen::dcomplex, 1, -1>>(
                this->element_array[i].element_signal_time_domain.data(),
                this->element_array[i].element_signal_time_domain.size());
        }
    }
    if (fs != nullptr)
        *fs = this->fs;
}

double vuprs::BeamFormingArray::GetMaxAbsoluteTimeDelay() const
{
    return this->time_delay_vector.array().abs().matrix().maxCoeff();
}

Eigen::Matrix<double, -1, -1> vuprs::BeamFormingArrayBase::GetTimeDelay(const std::vector<double> &alt,
                                                                        const std::vector<double> &az,
                                                                        double wave_velocity) const
{
    PARAM_CHECK(!this->empty(), "bf", " Array is empty.");
    PARAM_CHECK(alt.size() == az.size(), "bf", " Size of alt and az should be the same.");

    int k = alt.size();
    int M = this->element_array.size();
    Eigen::Matrix<double, -1, -1> res(M, k);
    Eigen::Matrix<double, 3, 1> pointing_vector;
    Eigen::Matrix<double, -1, -1> element_position_matrix(M, 3);
    for (int i = 0; i < M; i++)
    {
        element_position_matrix.row(i) = this->element_array[i].position_vector.transpose();
    }
    for (int i = 0; i < k; i++)
    {
        pointing_vector = vuprs::AltAz2PointingVector(alt[i], az[i]);
        res.col(i) = element_position_matrix * pointing_vector / wave_velocity;
    }
    return res;
}

/* --------------------------------------------------------------------------------------------------------------- */
/* ------------------------------------------------ Tool functions ----------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

bool vuprs::SaveToCSV(const Eigen::Matrix<double, -1, 1> &data, const std::string &filename)
{
    std::vector<double> _data;
    vuprs::eigenVector2stdVector<double>(data, &_data);
    return vuprs::SaveToCSV(_data, filename);
}

bool vuprs::SaveToCSV_complex(const Eigen::Matrix<Eigen::dcomplex, -1, 1> &data, const std::string &filename)
{
    std::vector<std::complex<double>> _data;
    vuprs::eigenVector2stdVector<std::complex<double>>(data, &_data);
    return vuprs::SaveToCSV_complex(_data, filename);
}
