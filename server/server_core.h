#ifndef VUPRS_SERVER_CORE_H
#define VUPRS_SERVER_CORE_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include "hybrid/hybrid_bf.h"
#include "server/net/control_worker.h"
#include "server/net/frame_publisher.h"
#include "3rdparty/nlohmann/json.hpp"

namespace spdlog
{
    class logger;
}

namespace vuprs
{
    struct SystemLoggerConfig
    {
        std::string hybrid_logger_dir;
        std::string server_logger_dir;
        std::string inference_logger_dir;
        SystemLoggerConfig() = default;
        bool InitFromJson(const std::string &filename);
    };

    struct SystemConfigFiles
    {
        std::string server_config_json;
        std::string fpga_config_json;
        std::string bf_array_config_json;
        std::string fir_config_json;
        std::string inference_model_config_json;
        std::string hybrid_default_config_json;
        SystemLoggerConfig logger_configs;
    };

    struct ServerConfig
    {
        uint16_t port = 8080;
        size_t max_payload_bytes = 8UL * 1024UL * 1024UL;
        size_t data_queue_frames = 64;
        size_t control_queue_frames = 16;
    };

    /**
     * @brief Algorithm/control core. Network transport is server/net/AsioServer.
     *
     * Commands are executed by ControlWorker; data is delivered through callbacks
     * to FramePublisher.
     */
    class ServerCore : public net::IControlService
    {
    public:
        ServerCore();
        ~ServerCore();

        ServerCore(const ServerCore &) = delete;
        ServerCore &operator=(const ServerCore &) = delete;

        void InitSystemConfigFiles(const vuprs::SystemConfigFiles &config);
        bool ConfigDone() const { return this->config_done_; }
        std::shared_ptr<spdlog::logger> Logger() const { return this->server_logger_; }
        bool Run();
        void Stop();
        uint16_t Port() const { return this->server_config_.port; }
        size_t MaxPayloadBytes() const { return this->server_config_.max_payload_bytes; }
        size_t DataQueueFrames() const { return this->server_config_.data_queue_frames; }
        size_t ControlQueueFrames() const { return this->server_config_.control_queue_frames; }

        void SetPublisher(const std::shared_ptr<net::FramePublisher> &publisher);

        net::ControlResult Execute(const std::string &cmd, const nlohmann::json &params) override;

    private:
        bool LoadServerConfigFromJson(const std::string &filename);
        void OnBfResult(BeamformerResultMeta &&meta);
        void OnScanResult(ScanResult &&result);
        void OnInferenceResult(InferenceResultMeta &&meta);
        void UpdateBeamformerName(const std::string &name);

        bool config_done_ = false;
        std::shared_ptr<spdlog::logger> server_logger_;
        std::shared_ptr<net::FramePublisher> publisher_;

        std::mutex mut_bf_;
        HybridBeamformer beamformer_;

        std::mutex mut_bf_config_;
        HybridBeamformerConfig bf_config_;

        std::mutex mut_scan_config_;
        ScanningConfig scan_config_;

        std::mutex mut_server_config_;
        ServerConfig server_config_;

        std::mutex mut_status_;
        std::string beamformer_name_ = "dcrcb";

        std::atomic<uint64_t> bf_sequence_{0};
        std::atomic<uint64_t> scan_sequence_{0};
        std::atomic<uint64_t> inference_sequence_{0};
    };
}

#endif
