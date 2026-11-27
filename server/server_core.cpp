#include <cmath>
#include <fstream>
#include <utility>
#include "config.h"
#include "logger/check.h"
#include "server/net/frame.h"
#include "server/server_core.h"

#define SERVER_LOG(level) _LOG(this->server_logger_ ? this->server_logger_ : spdlog::default_logger(), level)

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

    bool ParseAlgorithmParams(const nlohmann::json &params,
                              vuprs::HybridBeamformerConfig *config,
                              std::string *error)
    {
        if (!params.is_object())
        {
            *error = "params must be an object";
            return false;
        }
        if (params.contains("fs") && (!GetDouble(params, "fs", &config->fs) || config->fs <= 0.0))
        {
            *error = "invalid fs";
            return false;
        }
        if (params.contains("wave_velocity") &&
            (!GetDouble(params, "wave_velocity", &config->bf_wave_velocity) || config->bf_wave_velocity <= 0.0))
        {
            *error = "invalid wave_velocity";
            return false;
        }
        if (params.contains("lower_frequency") && !GetDouble(params, "lower_frequency", &config->bf_freq__lower))
        {
            *error = "invalid lower_frequency";
            return false;
        }
        if (params.contains("upper_frequency") && !GetDouble(params, "upper_frequency", &config->bf_freq__upper))
        {
            *error = "invalid upper_frequency";
            return false;
        }
        if (params.contains("snapshot_window_size") &&
            (!GetDouble(params, "snapshot_window_size", &config->bf_cov_snapshots_window_size) ||
             config->bf_cov_snapshots_window_size < MIN_COVARIANCE_SNAPSHOT_WINDOW))
        {
            *error = "invalid snapshot_window_size";
            return false;
        }
        if (params.contains("covariance_average_index") &&
            (!GetDouble(params, "covariance_average_index", &config->bf_cov_freq_average_index) ||
             config->bf_cov_freq_average_index < 0.0 || config->bf_cov_freq_average_index >= 1.0))
        {
            *error = "invalid covariance_average_index";
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
        this->server_logger_ = vuprs::LogManager::getLogger("server", "log.txt", config.logger_configs.server_logger_dir);
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
                    SERVER_LOG(V_WARN) << "[server] inference unavailable, continuing without fault detection.";
                }
            }
            catch (const std::exception &e)
            {
                SERVER_LOG(V_ERROR) << "[server] inference init failed: " << e.what()
                                    << ", continuing without fault detection.";
            }
        }
        {
            std::lock_guard<std::mutex> lock(this->mut_bf_config_); /* LOCK */
            config_result &= this->bf_config_.FromJson(config.hybrid_default_config_json);
        }
        RUNTIME_CHECK(config_result, "server", "Config error.");
        this->beamformer_.SetResultCallback([this](BeamformerResultMeta &&meta)
                                            { this->OnBfResult(std::move(meta)); });
        this->beamformer_.SetScanCallback([this](ScanResult &&result)
                                          { this->OnScanResult(std::move(result)); });
        this->beamformer_.SetInferenceCallback([this](InferenceResultMeta &&meta)
                                               { this->OnInferenceResult(std::move(meta)); });
        this->beamformer_.SetConsumerGate([this]()
                                          { return this->publisher_ && this->publisher_->HasSession(); });
        this->config_done_ = true;
    }
    catch (const std::exception &e)
    {
        SERVER_LOG(V_ERROR) << "Error in [ServerCore::InitSystemConfigFiles] " << e.what();
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
            SERVER_LOG(V_ERROR) << "[server] invalid beamformer config: " << info;
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
        SERVER_LOG(V_ERROR) << "[server] stop beamformer failed: " << e.what();
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

void vuprs::ServerCore::OnInferenceResult(InferenceResultMeta &&meta)
{
    if (!this->publisher_ || !this->publisher_->HasSession())
        return;
    auto tensor = std::make_shared<std::vector<uint32_t>>(std::move(meta.tensor));
    net::InferenceDataHeader header;
    header.tensor_length = static_cast<uint32_t>(tensor->size());
    header.data_format = static_cast<uint32_t>(net::InferenceFormat::INT32_Q31);
    this->publisher_->Publish(net::MakeInferenceFrame(header, std::move(tensor), ++this->inference_sequence_, net::NowNs()));
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
        result.params["inference_tensor_length"] = std::to_string(this->beamformer_.InferenceTensorLength());
        result.params["inference_format"] = "q31";

        HybridBeamformerConfig config;
        {
            std::lock_guard<std::mutex> lock(this->mut_bf_config_); /* LOCK */
            config = this->bf_config_;
        }
        nlohmann::json limits;
        limits["min_fs"] = std::to_string(MIN_SAMPLING_FREQUENCY);
        limits["max_fs"] = std::to_string(this->beamformer_.MaxSamplingFrequency());
        limits["min_frequency"] = std::to_string(MIN_BEAMFORMER_FREQUENCY);
        limits["max_frequency"] = std::to_string(this->beamformer_.MaxSamplingFrequency() / 2.0);
        limits["fft_points"] = std::to_string(this->beamformer_.SignalPoints());
        limits["dma_buffer_points"] = std::to_string(config.dma__buffer_points);
        limits["dma_buffer_count"] = std::to_string(config.dma__buffer_count);
        limits["fir_length"] = std::to_string(this->beamformer_.FIRLength());
        limits["inference_frame_time_ms"] = std::to_string(this->beamformer_.InferenceFrameTimeMs());
        limits["inference_enabled"] = this->beamformer_.InferenceReady() ? "1" : "0";
        limits["max_scan_points"] = std::to_string(MAX_SCANNING_POINTS_IN_HALF);
        limits["max_wave_velocity"] = std::to_string(MAX_WAVE_VELOCITY);
        result.params["limits"] = limits;
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
        std::string info;
        if (!this->beamformer_.CheckConfigValid(config, &info))
            return Error(ControlError::INVALID_PARAMS, info);
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
        std::unique_ptr<BeamFormingAlgo> algorithm;
        if (name == "mvdr")
            algorithm = std::make_unique<BF_MVDR>();
        else if (name == "cbf")
            algorithm = std::make_unique<BF_CBF>();
        else if (name == "dcrcb")
            algorithm = std::make_unique<BF_DCRCB>();
        else
            return Error(ControlError::INVALID_PARAMS, "invalid beamformer");
        HybridBeamformerConfig config;
        {
            std::lock_guard<std::mutex> lock(this->mut_bf_config_); /* LOCK */
            config = this->bf_config_;
        }
        std::lock_guard<std::mutex> lock(this->mut_bf_); /* LOCK */
        /* Stop and join the worker threads before replacing the algorithm object */
        if (!this->beamformer_.stop())
            return Error(ControlError::NOT_READY, "beamformer stop failed");
        if (!this->beamformer_.BindBeamformer(std::move(algorithm)))
            return Error(ControlError::NOT_READY, "beamformer change failed");
        /* Name reflects the bound algorithm even if run() fails. */
        this->UpdateBeamformerName(name);
        if (!this->beamformer_.run(config))
            return Error(ControlError::NOT_READY, "beamformer change failed");
        return Ok();
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
        }
        {
            std::lock_guard<std::mutex> lock(this->mut_bf_); /* LOCK */
            if (!this->beamformer_.ReDirect(alt, az, config.bf_wave_velocity))
                return Error(ControlError::INTERNAL, "redirect failed");
        }
        {
            std::lock_guard<std::mutex> lock(this->mut_bf_config_); /* LOCK */
            this->bf_config_ = config;
        }
        return Ok();
    }
    if (cmd == "validate_config")
    {
        HybridBeamformerConfig config;
        {
            std::lock_guard<std::mutex> lock(this->mut_bf_config_); /* LOCK */
            config = this->bf_config_;
        }
        std::string error;
        if (!ParseAlgorithmParams(params, &config, &error))
            return Error(ControlError::INVALID_PARAMS, error);

        std::string info;
        {
            std::lock_guard<std::mutex> lock(this->mut_bf_); /* LOCK */
            if (!this->beamformer_.CheckConfigValid(config, &info))
                return Error(ControlError::INVALID_PARAMS, info);
        }

        const uint32_t fft_points = this->beamformer_.SignalPoints();
        const double df = (fft_points > 0 && config.fs > 0.0)
                              ? config.fs / static_cast<double>(fft_points)
                              : 0.0;
        int band_bins = 0;
        if (df > 0.0 && config.bf_freq__upper > config.bf_freq__lower)
        {
            const double low_index = std::ceil(config.bf_freq__lower / df);
            const double high_index = std::floor(config.bf_freq__upper / df);
            const double count = high_index - low_index + 1.0;
            band_bins = count > 0.0 ? static_cast<int>(count) : 0;
        }

        const uint32_t inference_samples = this->beamformer_.InferenceReady()
                                               ? this->beamformer_.InferenceFrameSamples(config.fs)
                                               : 0;
        std::string warnings;
        if (df > 0.0 && band_bins < 16)
        {
            warnings += "band covers only " + std::to_string(band_bins) +
                        " FFT bins at df=" + std::to_string(df) + " Hz; ";
        }
        if (inference_samples > 0 && config.dma__buffer_points < inference_samples)
        {
            warnings += "DMA buffer " + std::to_string(config.dma__buffer_points) +
                        " is shorter than one inference frame (" +
                        std::to_string(inference_samples) + " samples); ";
        }

        auto result = Ok();
        result.params["frequency_resolution_hz"] = std::to_string(df);
        result.params["band_bins"] = std::to_string(band_bins);
        result.params["inference_frame_samples"] = std::to_string(inference_samples);
        result.params["dma_buffer_points"] = std::to_string(config.dma__buffer_points);
        result.params["fir_length"] = std::to_string(this->beamformer_.FIRLength());
        result.params["snapshot_time_seconds"] =
            std::to_string(fft_points > 0 && config.fs > 0.0
                               ? config.bf_cov_snapshots_window_size *
                                     static_cast<double>(fft_points) / config.fs
                               : 0.0);
        result.params["block_period_seconds"] =
            std::to_string(config.fs > 0.0
                               ? static_cast<double>(config.dma__buffer_points) / config.fs
                               : 0.0);
        result.params["warnings"] = warnings;
        return result;
    }
    if (cmd == "change_algorithm_parameters")
    {
        HybridBeamformerConfig config;
        {
            std::lock_guard<std::mutex> lock(this->mut_bf_config_); /* LOCK */
            config = this->bf_config_;
        }
        std::string error;
        if (!ParseAlgorithmParams(params, &config, &error))
            return Error(ControlError::INVALID_PARAMS, error);
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
            alt_min <= MIN_SCANNING_ALTITUDE_DEG || alt_min > 90.0)
            return Error(ControlError::INVALID_PARAMS, "invalid scan parameters (alt_min must be > 10 deg)");
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
        result.params["inference_tensor_length"] = std::to_string(this->beamformer_.InferenceTensorLength());
        result.params["inference_format"] = "q31";
        result.params["data_client"] = (this->publisher_ && this->publisher_->HasSession()) ? "1" : "0";
        result.params["protocol_version"] = "1";
        return result;
    }
    return Error(ControlError::UNKNOWN_COMMAND, "unknown command");
}
