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
                    if (!params.is_object())
                        return Err(ControlError::INVALID_PARAMS, "params must be an object");
                    if (params.contains("fs") && !AssignDouble(params, "fs", &this->fs_))
                        return Err(ControlError::INVALID_PARAMS, "invalid fs");
                    if (params.contains("wave_velocity") && !AssignDouble(params, "wave_velocity", &this->wave_velocity_))
                        return Err(ControlError::INVALID_PARAMS, "invalid wave_velocity");
                    if (params.contains("lower_frequency") && !AssignDouble(params, "lower_frequency", &this->lower_frequency_))
                        return Err(ControlError::INVALID_PARAMS, "invalid lower_frequency");
                    if (params.contains("upper_frequency") && !AssignDouble(params, "upper_frequency", &this->upper_frequency_))
                        return Err(ControlError::INVALID_PARAMS, "invalid upper_frequency");
                    if (params.contains("snapshot_window_size") && !AssignDouble(params, "snapshot_window_size", &this->snapshot_window_size_))
                        return Err(ControlError::INVALID_PARAMS, "invalid snapshot_window_size");
                    if (params.contains("covariance_average_index") && !AssignDouble(params, "covariance_average_index", &this->covariance_average_index_))
                        return Err(ControlError::INVALID_PARAMS, "invalid covariance_average_index");
                    return Ok();
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
                    if (points <= 0 || points > 10000 || !std::isfinite(alt_min) || alt_min < 0.0 || alt_min > 90.0)
                        return Err(ControlError::INVALID_PARAMS, "scan parameters out of range");
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

            static bool AssignDouble(const nlohmann::json &params, const std::string &key, std::atomic<double> *target)
            {
                if (!params[key].is_string())
                    return false;
                const double value = ParseDouble(params[key].get<std::string>());
                if (!std::isfinite(value))
                    return false;
                *target = value;
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
