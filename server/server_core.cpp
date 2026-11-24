#include "server/server_core.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <utility>
#include "config.h"
#include "logger/check.h"
#include "server/net/frame.h"

namespace
{
    bool GetDouble(const nlohmann::json &params, const char *key, double *value)
    {
        if (!params.contains(key) || !params[key].is_string())
            return false;
        try
        {
            *value = std::stod(params[key].get<std::string>());
        }
        catch (...)
        {
            return false;
        }
        return std::isfinite(*value);
    }

    bool GetInt(const nlohmann::json &params, const char *key, int *value)
    {
        if (!params.contains(key) || !params[key].is_string())
            return false;
        try
        {
            *value = std::stoi(params[key].get<std::string>());
        }
        catch (...)
        {
            return false;
        }
        return true;
    }

    vuprs::net::ControlResult Error(vuprs::net::ControlError code, const std::string &info)
    {
        vuprs::net::ControlResult result;
        result.ok = false;
        result.error = static_cast<int>(code);
        result.info = info;
        return result;
    }

    vuprs::net::ControlResult Ok()
    {
        vuprs::net::ControlResult result;
        result.ok = true;
        result.error = static_cast<int>(vuprs::net::ControlError::OK);
        return result;
    }
}

bool vuprs::SystemLoggerConfig::InitFromJson(const std::string &filename)
{
    std::ifstream f;
    f.open(filename);
    RUNTIME_CHECK(f.is_open(), "system", "Cannot open file: " + filename);
    nlohmann::json json_data;
    try
    {
        f >> json_data;
    }
    catch (const std::exception &e)
    {
        RUNTIME_CHECK(false, "system", " " + std::string(e.what()));
    }
    RUNTIME_CHECK(json_data.contains("hybrid-logger-dir"), "system", "Missing key: hybrid-logger-dir");
    RUNTIME_CHECK(json_data.contains("server-logger-dir"), "system", "Missing key: server-logger-dir");
    RUNTIME_CHECK(json_data.contains("inference-logger-dir"), "system", "Missing key: inference-logger-dir");
    this->hybrid_logger_dir = json_data["hybrid-logger-dir"].get<std::string>();
    this->server_logger_dir = json_data["server-logger-dir"].get<std::string>();
    this->inference_logger_dir = json_data["inference-logger-dir"].get<std::string>();
    return true;
}

vuprs::ServerCore::ServerCore() = default;

vuprs::ServerCore::~ServerCore()
{
    this->Stop();
}

bool vuprs::ServerCore::LoadServerConfigFromJson(const std::string &filename)
{
    std::ifstream f;
    f.open(filename);
    RUNTIME_CHECK(f.is_open(), "server", " Cannot open file: " + filename);
    nlohmann::json json_data;
    try
    {
        f >> json_data;
    }
    catch (const std::exception &e)
    {
        RUNTIME_CHECK(false, "server", " Error: " + std::string(e.what()));
    }
    uint16_t port = 8080;
    if (json_data.contains("port"))
        vuprs::__JsonStringParseINT<uint16_t>(&port, json_data, "port", true);
    else if (json_data.contains("initialize-port"))
        vuprs::__JsonStringParseINT<uint16_t>(&port, json_data, "initialize-port", true);
    RUNTIME_CHECK(port > 0, "server", " invalid port.");
    this->server_config_.port = port;

    uint32_t max_payload = static_cast<uint32_t>(this->server_config_.max_payload_bytes);
    uint32_t data_queue = static_cast<uint32_t>(this->server_config_.data_queue_frames);
    uint32_t control_queue = static_cast<uint32_t>(this->server_config_.control_queue_frames);
    if (json_data.contains("max-payload-bytes"))
        vuprs::__JsonStringParseINT<uint32_t>(&max_payload, json_data, "max-payload-bytes", true);
    if (json_data.contains("data-queue-frames"))
        vuprs::__JsonStringParseINT<uint32_t>(&data_queue, json_data, "data-queue-frames", true);
    if (json_data.contains("control-queue-frames"))
        vuprs::__JsonStringParseINT<uint32_t>(&control_queue, json_data, "control-queue-frames", true);
    RUNTIME_CHECK(max_payload > 0 && data_queue > 0 && control_queue > 0,
                  "server", " invalid queue/payload limits.");
    this->server_config_.max_payload_bytes = max_payload;
    this->server_config_.data_queue_frames = data_queue;
    this->server_config_.control_queue_frames = control_queue;
    return true;
}

void vuprs::ServerCore::InitSystemConfigFiles(const vuprs::SystemConfigFiles &config)
{
    bool config_result = true;
    try
    {
        config_result &= this->LoadServerConfigFromJson(config.server_config_json);
        {
            std::lock_guard<std::mutex> lock(this->mut_bf_); /* LOCK */
            config_result &= this->beamformer_.InitHybridBeamformer(config.fpga_config_json,
                                                                   config.bf_array_config_json,
                                                                   config.fir_config_json,
                                                                   config.logger_configs.hybrid_logger_dir);
            try
            {
                if (!this->beamformer_.InitInference(config.inference_model_config_json,
                                                     config.logger_configs.inference_logger_dir))
                {
                    std::cout << "[server] inference unavailable, continuing without fault detection." << std::endl;
                }
            }
            catch (const std::exception &e)
            {
                std::cout << "[server] inference init failed: " << e.what()
                          << ", continuing without fault detection." << std::endl;
            }
        }
        {
            std::lock_guard<std::mutex> lock(this->mut_bf_config_); /* LOCK */
            config_result &= this->bf_config_.FromJson(config.hybrid_default_config_json);
        }
        RUNTIME_CHECK(config_result, "server", "Config error.");
        this->server_logger_ = vuprs::LogManager::getLogger("server", "log.txt", config.logger_configs.server_logger_dir);
        this->beamformer_.SetResultCallback([this](BeamformerResultMeta &&meta)
                                            { this->OnBfResult(std::move(meta)); });
        this->beamformer_.SetScanCallback([this](ScanResult &&result)
                                          { this->OnScanResult(std::move(result)); });
        this->beamformer_.SetConsumerGate([this]()
                                          { return this->publisher_ && this->publisher_->HasSession(); });
        this->config_done_ = true;
    }
    catch (const std::exception &e)
    {
        std::cout << "Error in [ServerCore::InitSystemConfigFiles] " << e.what() << std::endl;
    }
}

bool vuprs::ServerCore::Run()
{
    if (!this->config_done_)
        return false;
    HybridBeamformerConfig config;
    {
        std::lock_guard<std::mutex> lock(this->mut_bf_config_); /* LOCK */
        config = this->bf_config_;
    }
    std::string info;
    {
        std::lock_guard<std::mutex> lock(this->mut_bf_); /* LOCK */
        if (!this->beamformer_.CheckConfigValid(config, &info))
        {
            std::cerr << "[server] invalid beamformer config: " << info << std::endl;
            return false;
        }
        return this->beamformer_.run(config);
    }
}

void vuprs::ServerCore::Stop()
{
    try
    {
        std::lock_guard<std::mutex> lock(this->mut_bf_); /* LOCK */
        this->beamformer_.stop();
    }
    catch (const std::exception &e)
    {
        std::cerr << "[server] stop beamformer failed: " << e.what() << std::endl;
    }
}

void vuprs::ServerCore::SetPublisher(const std::shared_ptr<net::FramePublisher> &publisher)
{
    this->publisher_ = publisher;
}

void vuprs::ServerCore::UpdateBeamformerName(const std::string &name)
{
    std::lock_guard<std::mutex> lock(this->mut_status_);
    this->beamformer_name_ = name;
}

void vuprs::ServerCore::OnBfResult(BeamformerResultMeta &&meta)
{
    if (!this->publisher_ || !this->publisher_->HasSession())
        return;
    auto samples = std::make_shared<std::vector<uint32_t>>(std::move(meta.signal));
    net::BfDataHeader header;
    header.sample_rate_millihz = static_cast<uint32_t>(std::llround(this->beamformer_.HardwareSamplingFrequency() * 1000.0));
    header.sample_count = static_cast<uint32_t>(samples->size());
    header.sample_format = static_cast<uint32_t>(net::SampleFormat::INT32_Q16_16);
    header.inference_identity = meta.inference_result_identity;
    header.inference_valid = meta.inference_valid ? 1U : 0U;
    this->publisher_->Publish(net::MakeBfFrame(header, std::move(samples), ++this->bf_sequence_, net::NowNs()));
}

void vuprs::ServerCore::OnScanResult(ScanResult &&result)
{
    if (!this->publisher_ || !this->publisher_->HasSession())
        return;
    auto values = std::make_shared<std::vector<uint16_t>>(std::move(result.scan_result));
    net::ScanDataHeader header;
    header.points_half = static_cast<uint32_t>(result.points_in_hemisphere);
    header.data_points = static_cast<uint32_t>(values->size());
    header.sample_rate_millihz = static_cast<uint32_t>(std::llround(this->beamformer_.HardwareSamplingFrequency() * 1000.0));
    header.alt_min_mdeg = static_cast<int32_t>(std::llround(result.alt_min * 1000.0));
    header.min_power_mdb = static_cast<int32_t>(std::llround(result.min_power_db * 1000.0));
    header.max_power_mdb = static_cast<int32_t>(std::llround(result.max_power_db * 1000.0));
    header.scan_format = static_cast<uint32_t>(net::ScanFormat::UINT16_MINMAX_NORMALIZED);
    this->publisher_->Publish(net::MakeScanFrame(header, std::move(values), ++this->scan_sequence_, net::NowNs()));
}

vuprs::net::ControlResult vuprs::ServerCore::Execute(const std::string &cmd, const nlohmann::json &params)
{
    using vuprs::net::ControlError;

    if (cmd == "ack")
    {
        auto result = Ok();
        result.params["protocol_version"] = "1";
        result.params["server_version"] = "vuprs-1";
        result.params["capabilities"] = nlohmann::json::object({{"bf", "1"}, {"scan", "1"}, {"inference", this->beamformer_.InferenceReady() ? "1" : "0"}});
        return result;
    }
    if (cmd == "start")
    {
        HybridBeamformerConfig config;
        {
            std::lock_guard<std::mutex> lock(this->mut_bf_config_); /* LOCK */
            config = this->bf_config_;
        }
        std::lock_guard<std::mutex> lock(this->mut_bf_); /* LOCK */
        return this->beamformer_.run(config) ? Ok() : Error(ControlError::NOT_READY, "start failed");
    }
    if (cmd == "stop")
    {
        std::lock_guard<std::mutex> lock(this->mut_bf_); /* LOCK */
        return this->beamformer_.stop() ? Ok() : Error(ControlError::INTERNAL, "stop failed");
    }
    if (cmd == "reset")
    {
        HybridBeamformerConfig config;
        {
            std::lock_guard<std::mutex> lock(this->mut_bf_config_); /* LOCK */
            config = this->bf_config_;
        }
        std::lock_guard<std::mutex> lock(this->mut_bf_); /* LOCK */
        bool ok = this->beamformer_.stop();
        ok = this->beamformer_.run(config) && ok;
        return ok ? Ok() : Error(ControlError::NOT_READY, "reset failed");
    }
    if (cmd == "change_beamformer")
    {
        if (!params.contains("beamformer") || !params["beamformer"].is_string())
            return Error(ControlError::INVALID_PARAMS, "missing beamformer");
        const std::string name = params["beamformer"].get<std::string>();
        std::unique_ptr<WidebandBeamformerTemplate> algorithm;
        if (name == "mvdr")
            algorithm = std::make_unique<Beamformer_MVDR>();
        else if (name == "cbf")
            algorithm = std::make_unique<Beamformer_CBF>();
        else if (name == "dcrcb")
            algorithm = std::make_unique<Beamformer_DCRCB>();
        else
            return Error(ControlError::INVALID_PARAMS, "invalid beamformer");
        HybridBeamformerConfig config;
        {
            std::lock_guard<std::mutex> lock(this->mut_bf_config_); /* LOCK */
            config = this->bf_config_;
        }
        std::lock_guard<std::mutex> lock(this->mut_bf_); /* LOCK */
        bool ok = this->beamformer_.BindBeamformer(std::move(algorithm));
        ok = this->beamformer_.stop() && ok;
        ok = this->beamformer_.run(config) && ok;
        if (ok)
            this->UpdateBeamformerName(name);
        return ok ? Ok() : Error(ControlError::NOT_READY, "beamformer change failed");
    }
    if (cmd == "redirect")
    {
        double alt = 0.0, az = 0.0;
        if (!GetDouble(params, "alt", &alt) || !GetDouble(params, "az", &az) ||
            alt < -90.0 || alt > 90.0 || az < -180.0 || az > 180.0)
            return Error(ControlError::INVALID_PARAMS, "invalid alt/az");
        HybridBeamformerConfig config;
        {
            std::lock_guard<std::mutex> lock(this->mut_bf_config_); /* LOCK */
            config = this->bf_config_;
            config.bf_target__alt = alt;
            config.bf_target__az = az;
            this->bf_config_ = config;
        }
        std::lock_guard<std::mutex> lock(this->mut_bf_); /* LOCK */
        return this->beamformer_.ReDirect(alt, az, config.bf_wave_velocity) ? Ok() : Error(ControlError::INTERNAL, "redirect failed");
    }
    if (cmd == "change_algorithm_parameters")
    {
        if (!params.is_object())
            return Error(ControlError::INVALID_PARAMS, "params must be an object");
        HybridBeamformerConfig config;
        {
            std::lock_guard<std::mutex> lock(this->mut_bf_config_); /* LOCK */
            config = this->bf_config_;
        }
        if (params.contains("fs") && (!GetDouble(params, "fs", &config.fs) || config.fs <= 0.0))
            return Error(ControlError::INVALID_PARAMS, "invalid fs");
        if (params.contains("wave_velocity") && (!GetDouble(params, "wave_velocity", &config.bf_wave_velocity) || config.bf_wave_velocity <= 0.0))
            return Error(ControlError::INVALID_PARAMS, "invalid wave_velocity");
        if (params.contains("lower_frequency") && !GetDouble(params, "lower_frequency", &config.bf_freq__lower))
            return Error(ControlError::INVALID_PARAMS, "invalid lower_frequency");
        if (params.contains("upper_frequency") && !GetDouble(params, "upper_frequency", &config.bf_freq__upper))
            return Error(ControlError::INVALID_PARAMS, "invalid upper_frequency");
        if (params.contains("snapshot_window_size") && (!GetDouble(params, "snapshot_window_size", &config.bf_cov_snapshots_window_size) || config.bf_cov_snapshots_window_size <= 0.0))
            return Error(ControlError::INVALID_PARAMS, "invalid snapshot_window_size");
        if (params.contains("covariance_average_index") && (!GetDouble(params, "covariance_average_index", &config.bf_cov_freq_average_index) || config.bf_cov_freq_average_index < 0.0 || config.bf_cov_freq_average_index >= 1.0))
            return Error(ControlError::INVALID_PARAMS, "invalid covariance_average_index");
        std::string info;
        {
            std::lock_guard<std::mutex> lock(this->mut_bf_); /* LOCK */
            if (!this->beamformer_.CheckConfigValid(config, &info))
                return Error(ControlError::INVALID_PARAMS, info);
            bool ok = this->beamformer_.stop();
            ok = this->beamformer_.run(config) && ok;
            if (!ok)
                return Error(ControlError::NOT_READY, "apply parameters failed");
        }
        {
            std::lock_guard<std::mutex> lock(this->mut_bf_config_); /* LOCK */
            this->bf_config_ = config;
        }
        return Ok();
    }
    if (cmd == "read_algorithm_parameters")
    {
        HybridBeamformerConfig config;
        {
            std::lock_guard<std::mutex> lock(this->mut_bf_config_); /* LOCK */
            config = this->bf_config_;
        }
        auto result = Ok();
        result.params["fs"] = std::to_string(config.fs);
        result.params["wave_velocity"] = std::to_string(config.bf_wave_velocity);
        result.params["lower_frequency"] = std::to_string(config.bf_freq__lower);
        result.params["upper_frequency"] = std::to_string(config.bf_freq__upper);
        result.params["snapshot_window_size"] = std::to_string(config.bf_cov_snapshots_window_size);
        result.params["covariance_average_index"] = std::to_string(config.bf_cov_freq_average_index);
        return result;
    }
    if (cmd == "enable_scan")
    {
        int points = 0;
        double alt_min = 0.0;
        if (!GetInt(params, "points", &points) || !GetDouble(params, "alt_min", &alt_min) ||
            points <= 0 || points > MAX_SCANNING_POINTS_IN_HALF ||
            alt_min < 0.0 || alt_min > 90.0)
            return Error(ControlError::INVALID_PARAMS, "invalid scan parameters");
        ScanningConfig scan_config;
        {
            std::lock_guard<std::mutex> lock(this->mut_scan_config_); /* LOCK */
            scan_config = this->scan_config_;
            scan_config.points_in_hemisphere = points;
            scan_config.alt_min = alt_min;
        }
        HybridBeamformerConfig config;
        {
            std::lock_guard<std::mutex> lock(this->mut_bf_config_); /* LOCK */
            config = this->bf_config_;
        }
        std::lock_guard<std::mutex> lock(this->mut_bf_); /* LOCK */
        std::string info;
        if (!this->beamformer_.CheckScanningConfigValid(scan_config, &info))
            return Error(ControlError::INVALID_PARAMS, info);
        if (!this->beamformer_.ScanOptions(scan_config, config.bf_wave_velocity))
            return Error(ControlError::INTERNAL, "scan options failed");
        {
            std::lock_guard<std::mutex> lock_scan(this->mut_scan_config_); /* LOCK */
            this->scan_config_ = scan_config;
        }
        if (!this->beamformer_.ScanSwitch())
            this->beamformer_.ScanSwitch(true);
        return Ok();
    }
    if (cmd == "disable_scan")
    {
        std::lock_guard<std::mutex> lock(this->mut_bf_); /* LOCK */
        if (this->beamformer_.ScanSwitch())
            this->beamformer_.ScanSwitch(false);
        return Ok();
    }
    if (cmd == "get_status")
    {
        auto result = Ok();
        result.params["running"] = this->beamformer_.isRun() ? "1" : "0";
        {
            std::lock_guard<std::mutex> lock(this->mut_status_); /* LOCK */
            result.params["beamformer"] = this->beamformer_name_;
        }
        HybridBeamformerConfig config;
        {
            std::lock_guard<std::mutex> lock(this->mut_bf_config_); /* LOCK */
            config = this->bf_config_;
        }
        result.params["fs"] = std::to_string(config.fs);
        {
            std::lock_guard<std::mutex> lock(this->mut_scan_config_); /* LOCK */
            result.params["scan_points"] = std::to_string(this->scan_config_.points_in_hemisphere);
            result.params["scan_alt_min"] = std::to_string(this->scan_config_.alt_min);
        }
        result.params["scan_enabled"] = this->beamformer_.ScanSwitch() ? "1" : "0";
        result.params["inference_ready"] = this->beamformer_.InferenceReady() ? "1" : "0";
        result.params["data_client"] = (this->publisher_ && this->publisher_->HasSession()) ? "1" : "0";
        result.params["protocol_version"] = "1";
        return result;
    }
    return Error(ControlError::UNKNOWN_COMMAND, "unknown command");
}
