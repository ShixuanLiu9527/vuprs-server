#ifndef VUPRS_MOCK_CONTROL_SERVICE_H
#define VUPRS_MOCK_CONTROL_SERVICE_H

#include <atomic>
#include <cmath>
#include <mutex>
#include <string>
#include "server/net/control_worker.h"

namespace vuprs
{
    namespace mock
    {
        class MockControlService : public vuprs::net::IControlService
        {
        public:
            vuprs::net::ControlResult Execute(const std::string &cmd,
                                              const nlohmann::json &params) override
            {
                using vuprs::net::ControlError;
                using vuprs::net::ControlResult;

                if (cmd == "ack")
                {
                    ControlResult result = Ok();
                    result.params["protocol_version"] = "1";
                    result.params["server_version"] = "mock-1";
                    result.params["capabilities"] = nlohmann::json::object({{"bf", "1"}, {"scan", "1"}, {"inference", "0"}});
                    result.params["inference_tensor_length"] = "0";
                    result.params["inference_format"] = "q31";
                    result.params["limits"] = nlohmann::json::object({
                        {"min_fs", "10.0"},
                        {"max_fs", "120000.0"},
                        {"min_frequency", "10.0"},
                        {"max_frequency", "60000.0"},
                        {"fft_points", "512"},
                        {"dma_buffer_points", "8192"},
                        {"dma_buffer_count", "20"},
                        {"fir_length", "256"},
                        {"inference_frame_time_ms", "15.0"},
                        {"inference_enabled", "0"},
                        {"max_scan_points", "10000"},
                        {"max_wave_velocity", "100000.0"}});
                    return result;
                }
                if (cmd == "start")
                {
                    this->running_ = true;
                    return Ok();
                }
                if (cmd == "stop")
                {
                    this->running_ = false;
                    return Ok();
                }
                if (cmd == "reset")
                {
                    this->running_ = true;
                    return Ok();
                }
                if (cmd == "change_beamformer")
                {
                    if (!params.contains("beamformer") || !params["beamformer"].is_string())
                        return Err(ControlError::INVALID_PARAMS, "missing beamformer");
                    const std::string name = params["beamformer"].get<std::string>();
                    if (name != "mvdr" && name != "cbf" && name != "dcrcb")
                        return Err(ControlError::INVALID_PARAMS, "invalid beamformer");
                    {
                        std::lock_guard<std::mutex> lock(this->mut_);
                        this->beamformer_ = name;
                    }
                    return Ok();
                }
                if (cmd == "redirect")
                {
                    if (!params.contains("alt") || !params.contains("az") ||
                        !params["alt"].is_string() || !params["az"].is_string())
                        return Err(ControlError::INVALID_PARAMS, "missing alt/az");
                    const double alt = ParseDouble(params["alt"].get<std::string>());
                    const double az = ParseDouble(params["az"].get<std::string>());
                    if (!std::isfinite(alt) || !std::isfinite(az) ||
                        alt < -90.0 || alt > 90.0 || az < -180.0 || az > 180.0)
                        return Err(ControlError::INVALID_PARAMS, "alt/az out of range");
                    return Ok();
                }
                if (cmd == "change_algorithm_parameters")
                {
                    double fs = this->fs_.load();
                    double wave_velocity = this->wave_velocity_.load();
                    double lower = this->lower_frequency_.load();
                    double upper = this->upper_frequency_.load();
                    double snapshot = this->snapshot_window_size_.load();
                    double covariance = this->covariance_average_index_.load();
                    if (!AssignDoubleField(params, "fs", &fs) ||
                        !AssignDoubleField(params, "wave_velocity", &wave_velocity) ||
                        !AssignDoubleField(params, "lower_frequency", &lower) ||
                        !AssignDoubleField(params, "upper_frequency", &upper) ||
                        !AssignDoubleField(params, "snapshot_window_size", &snapshot) ||
                        !AssignDoubleField(params, "covariance_average_index", &covariance))
                        return Err(ControlError::INVALID_PARAMS, "invalid algorithm parameters");
                    std::string error;
                    if (!ValidateAlgorithmConfig(fs, wave_velocity, lower, upper, snapshot, covariance, &error))
                        return Err(ControlError::INVALID_PARAMS, error);
                    this->fs_ = fs;
                    this->wave_velocity_ = wave_velocity;
                    this->lower_frequency_ = lower;
                    this->upper_frequency_ = upper;
                    this->snapshot_window_size_ = snapshot;
                    this->covariance_average_index_ = covariance;
                    return Ok();
                }
                if (cmd == "validate_config")
                {
                    double fs = this->fs_.load();
                    double wave_velocity = this->wave_velocity_.load();
                    double lower = this->lower_frequency_.load();
                    double upper = this->upper_frequency_.load();
                    double snapshot = this->snapshot_window_size_.load();
                    double covariance = this->covariance_average_index_.load();
                    if (!AssignDoubleField(params, "fs", &fs) ||
                        !AssignDoubleField(params, "wave_velocity", &wave_velocity) ||
                        !AssignDoubleField(params, "lower_frequency", &lower) ||
                        !AssignDoubleField(params, "upper_frequency", &upper) ||
                        !AssignDoubleField(params, "snapshot_window_size", &snapshot) ||
                        !AssignDoubleField(params, "covariance_average_index", &covariance))
                        return Err(ControlError::INVALID_PARAMS, "invalid algorithm parameters");
                    std::string error;
                    if (!ValidateAlgorithmConfig(fs, wave_velocity, lower, upper, snapshot, covariance, &error))
                        return Err(ControlError::INVALID_PARAMS, error);

                    const double df = fs / 512.0;
                    const double low_index = std::ceil(lower / df);
                    const double high_index = std::floor(upper / df);
                    const int band_bins = high_index >= low_index
                                              ? static_cast<int>(high_index - low_index + 1.0)
                                              : 0;
                    std::string warnings;
                    if (band_bins < 16)
                    {
                        warnings += "band covers only " + std::to_string(band_bins) +
                                    " FFT bins at df=" + std::to_string(df) + " Hz; ";
                    }
                    ControlResult result = Ok();
                    result.params["frequency_resolution_hz"] = std::to_string(df);
                    result.params["band_bins"] = std::to_string(band_bins);
                    result.params["inference_frame_samples"] = "0";
                    result.params["dma_buffer_points"] = "8192";
                    result.params["fir_length"] = "256";
                    result.params["snapshot_time_seconds"] = std::to_string(snapshot * 512.0 / fs);
                    result.params["block_period_seconds"] = std::to_string(8192.0 / fs);
                    result.params["warnings"] = warnings;
                    return result;
                }
                if (cmd == "read_algorithm_parameters")
                {
                    ControlResult result = Ok();
                    result.params["fs"] = std::to_string(this->fs_.load());
                    result.params["wave_velocity"] = std::to_string(this->wave_velocity_.load());
                    result.params["lower_frequency"] = std::to_string(this->lower_frequency_.load());
                    result.params["upper_frequency"] = std::to_string(this->upper_frequency_.load());
                    result.params["snapshot_window_size"] = std::to_string(this->snapshot_window_size_.load());
                    result.params["covariance_average_index"] = std::to_string(this->covariance_average_index_.load());
                    return result;
                }
                if (cmd == "enable_scan")
                {
                    if (!params.contains("points") || !params.contains("alt_min") ||
                        !params["points"].is_string() || !params["alt_min"].is_string())
                        return Err(ControlError::INVALID_PARAMS, "missing points/alt_min");
                    int points = 0;
                    const double alt_min = ParseDouble(params["alt_min"].get<std::string>());
                    try
                    {
                        points = std::stoi(params["points"].get<std::string>());
                    }
                    catch (...)
                    {
                        return Err(ControlError::INVALID_PARAMS, "invalid points");
                    }
                    if (points <= 0 || points > 10000 || !std::isfinite(alt_min) || alt_min <= 10.0 || alt_min > 90.0)
                        return Err(ControlError::INVALID_PARAMS, "scan parameters out of range (alt_min must be > 10 deg)");
                    this->scan_points_ = points;
                    this->scan_alt_min_ = alt_min;
                    this->scan_enabled_ = true;
                    return Ok();
                }
                if (cmd == "disable_scan")
                {
                    this->scan_enabled_ = false;
                    return Ok();
                }
                if (cmd == "get_status")
                {
                    ControlResult result = Ok();
                    result.params["running"] = this->running_.load() ? "1" : "0";
                    {
                        std::lock_guard<std::mutex> lock(this->mut_);
                        result.params["beamformer"] = this->beamformer_;
                    }
                    result.params["fs"] = std::to_string(this->fs_.load());
                    result.params["scan_enabled"] = this->scan_enabled_.load() ? "1" : "0";
                    result.params["scan_points"] = std::to_string(this->scan_points_.load());
                    result.params["scan_alt_min"] = std::to_string(this->scan_alt_min_.load());
                    result.params["inference_ready"] = "0";
                    result.params["inference_tensor_length"] = "0";
                    result.params["inference_format"] = "q31";
                    result.params["protocol_version"] = "1";
                    return result;
                }
                return Err(ControlError::UNKNOWN_COMMAND, "unknown command");
            }

            bool IsRunning() const { return this->running_.load(); }
            bool IsScanEnabled() const { return this->scan_enabled_.load(); }
            int ScanPoints() const { return this->scan_points_.load(); }
            double ScanAltMin() const { return this->scan_alt_min_.load(); }
            double SampleRate() const { return this->fs_.load(); }

        private:
            static vuprs::net::ControlResult Ok()
            {
                vuprs::net::ControlResult result;
                result.ok = true;
                result.error = static_cast<int>(vuprs::net::ControlError::OK);
                return result;
            }

            static vuprs::net::ControlResult Err(vuprs::net::ControlError code, const std::string &info)
            {
                vuprs::net::ControlResult result;
                result.ok = false;
                result.error = static_cast<int>(code);
                result.info = info;
                return result;
            }

            static double ParseDouble(const std::string &value)
            {
                try
                {
                    return std::stod(value);
                }
                catch (...)
                {
                    return std::nan("");
                }
            }

            static bool AssignDoubleField(const nlohmann::json &params, const std::string &key, double *target)
            {
                if (!params.contains(key))
                    return true;
                if (!params[key].is_string())
                    return false;
                const double value = ParseDouble(params[key].get<std::string>());
                if (!std::isfinite(value))
                    return false;
                *target = value;
                return true;
            }

            static bool ValidateAlgorithmConfig(double fs,
                                                double wave_velocity,
                                                double lower,
                                                double upper,
                                                double snapshot,
                                                double covariance,
                                                std::string *error)
            {
                if (!(fs >= 10.0 && fs <= 120000.0))
                {
                    *error = "fs out of range";
                    return false;
                }
                if (!(wave_velocity > 0.0 && wave_velocity <= 100000.0))
                {
                    *error = "invalid wave_velocity";
                    return false;
                }
                if (!(lower >= 10.0 && upper >= 10.0 && lower < upper && upper < fs / 2.0))
                {
                    *error = "band out of range";
                    return false;
                }
                if (!(snapshot >= 1.0))
                {
                    *error = "invalid snapshot_window_size";
                    return false;
                }
                if (!(covariance >= 0.0 && covariance < 1.0))
                {
                    *error = "invalid covariance_average_index";
                    return false;
                }
                return true;
            }

            std::atomic<bool> running_{false};
            std::atomic<bool> scan_enabled_{false};
            std::atomic<int> scan_points_{100};
            std::atomic<double> scan_alt_min_{15.0};
            std::atomic<double> fs_{10000.0};
            std::atomic<double> wave_velocity_{346.0};
            std::atomic<double> lower_frequency_{500.0};
            std::atomic<double> upper_frequency_{3000.0};
            std::atomic<double> snapshot_window_size_{200.0};
            std::atomic<double> covariance_average_index_{0.8};
            std::mutex mut_;
            std::string beamformer_ = "mvdr";
        };
    }
}

#endif
