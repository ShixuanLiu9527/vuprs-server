#include "server/linux_server.h"
#include "logger/check.h"

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
        RUNTIME_CHECK(false, "bf", " in [BeamFormingArray::LoadArrayFromJson] Failed to load array data from: " + filename);
    }
    RUNTIME_CHECK(json_data.contains("hybrid-logger-dir"), "system", "Missing key: hybrid-logger-dir");
    RUNTIME_CHECK(json_data.contains("server-logger-dir"), "system", "Missing key: server-logger-dir");
    RUNTIME_CHECK(json_data.contains("inference-logger-dir"), "system", "Missing key: inference-logger-dir");
    this->hybrid_logger_dir = json_data["hybrid-logger-dir"].get<std::string>();
    this->server_logger_dir = json_data["server-logger-dir"].get<std::string>();
    this->inference_logger_dir = json_data["inference-logger-dir"].get<std::string>();
    return true;
}

/* --------------------------------------------------------------------------------------------------------------- */
/* ----------------------------------------------- Linux Server -------------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

vuprs::LinuxServer::LinuxServer()
{
    this->config_done = false;
    this->control_irq = false;
    this->server_response_irq = false;
    this->sending_irq = false; /* true: should send */
    this->control_irq = false;
    this->server_need_response = false; /* false = no need to send response */
}

vuprs::LinuxServer::~LinuxServer()
{
    this->Stop();
}

bool vuprs::LinuxServer::LoadServerConfigFromJson(const std::string &json_filename)
{
    std::ifstream f;
    /* open config json file */
    f.open(json_filename);
    RUNTIME_CHECK(f.is_open(), "server", " in [LinuxServer::LoadServerConfigFromJson] Cannot open file: " + json_filename);
    nlohmann::json json_data;
    try
    {
        f >> json_data;
    }
    catch (const std::exception &e)
    {
        RUNTIME_CHECK(false, "server", " Error: " + std::string(e.what()));
    }
    vuprs::__JsonStringParseINT<uint16_t>(&this->server_config.initialize_port, json_data, "initialize-port", true);
    vuprs::__JsonStringParseINT<uint16_t>(&this->server_config.maximum_port, json_data, "max-port", true);
    vuprs::__JsonStringParseINT<uint16_t>(&this->server_config.accept_client_counts, json_data, "accept-client-counts", true);
    if (json_data.contains("protocol"))
    {
        auto protocol = json_data["protocol"];

        vuprs::__JsonParseString(&this->server_config.protocol.command_header, protocol, "command-header", true);
        vuprs::__JsonParseString(&this->server_config.protocol.command_tailer, protocol, "command-tailer", true);
    }
    return true;
}

void vuprs::LinuxServer::InitSystemConfigFiles(const vuprs::SystemConfigFiles &config)
{
    bool config_result = true;
    try
    {
        /* Config server */
        config_result &= this->LoadServerConfigFromJson(config.server_config_json);
        /* Config beam former */
        {
            std::lock_guard<std::mutex> lock(this->mut_bf); /* LOCK */
            config_result &= this->beamformer.InitHybridBeamformer(config.fpga_config_json,
                                                                   config.bf_array_config_json,
                                                                   config.fir_config_json,
                                                                   config.logger_configs.hybrid_logger_dir);
        }
        {
            std::lock_guard<std::mutex> lock(this->mut_npu); /* LOCK */
            config_result &= this->fault_detector.InitDetector(config.inference_model_config_json,
                                                               config.logger_configs.inference_logger_dir);
        }
        this->server_logger = vuprs::LogManager::getLogger("server",
                                                           "log.txt",
                                                           config.logger_configs.server_logger_dir);
        RUNTIME_CHECK(config_result, "server", " in [LinuxServer::InitSystemConfigFiles] Config error.");
        this->config_done = true;
    }
    catch (const std::exception &e)
    {
        std::cout << "Error in [LinuxServer::InitSystemConfigFiles] " << e.what() << std::endl;
    }
}

bool vuprs::LinuxServer::ConfigDone() const
{
    return this->config_done;
}

bool vuprs::LinuxServer::InitServer()
{
    for (uint16_t current_port = this->server_config.initialize_port; current_port <= this->server_config.maximum_port; current_port++)
    {
        /* Get current port */
        this->server_port = current_port;
        std::cout << "[server] trying to start server use port [" << this->server_port << "]." << std::endl;
        try
        {
            /* STEP 1: Create socket */
            this->server_fd = socket(AF_INET, SOCK_STREAM, 0); /* IPV-4 TCP */
            RUNTIME_CHECK(this->server_fd >= 0, "server", " [server] failed to create socket.");
            int opt = 1;
            if (setsockopt(this->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
            {
                close(this->server_fd);
                this->server_fd = -1;
                RUNTIME_CHECK(false, "server", " [server] set option error.");
            }
            /* STEP 2: Bind this process to PORT */
            struct sockaddr_in server_addr;
            memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = INADDR_ANY;        /* bind to all IP address */
            server_addr.sin_port = htons(this->server_port); /* bind to certain PORT */
            if (bind(this->server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
            {
                close(this->server_fd);
                this->server_fd = -1;

                RUNTIME_CHECK(false, "server", " [server] bind failed for port: " + std::to_string(this->server_port));
            }
            /* STEP 3: Listen */
            if (listen(this->server_fd, this->server_config.accept_client_counts) < 0)
            {
                close(this->server_fd);
                this->server_fd = -1;

                RUNTIME_CHECK(false, "server", " [server] listening error.");
            }
            std::cout << "[server] server startup successfully on port [" << this->server_port << "]." << std::endl;
            return true;
        }
        catch (const std::exception &e)
        {
            std::cout << "[server] start server failed, with error occurred: " << e.what() << std::endl;
        }
    }
    /* Dead */
    std::cout << "[server] unable to start up, the server is dead." << std::endl;
    if (this->server_fd >= 0)
    {
        close(this->server_fd);
        this->server_fd = -1;
    }
    return false;
}

void vuprs::LinuxServer::THREAD__AcceptClient()
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = -1;
    std::cout << "[server][listening] waiting for client connect ... ..." << std::endl;
    while (this->server_running)
    {
        if (this->server_session && !this->server_session->IsRun()) /* Close session */
        {
            std::cout << "[server][listening] client disconnected." << std::endl;
            this->ConnectCallback(false, "");
            this->server_session->Stop();
            this->server_session.reset();
            std::lock_guard<std::mutex> lock(this->mut_client_io_manager); /* LOCK */
            this->client_io_manager.reset();
        }
        if (!this->server_session) /* try to connect */
        {
            client_fd = accept(this->server_fd, (struct sockaddr *)&client_addr, &client_len);
            if (client_fd >= 0)
            {
                /* Generate session */
                std::shared_ptr<vuprs::SocketIOManager> manager;
                {
                    std::lock_guard<std::mutex> lock(this->mut_client_io_manager);                              /* LOCK */
                    this->client_io_manager = std::make_shared<vuprs::SocketIOManager>(client_fd, client_addr); /* Generate IO manager */
                    manager = this->client_io_manager;
                }
                std::string header, tailer;
                {
                    std::lock_guard<std::mutex> lock(this->mut_server_config); /* LOCK */
                    header = this->server_config.protocol.command_header;
                    tailer = this->server_config.protocol.command_tailer;
                }
                this->server_session = std::make_unique<vuprs::LinuxSession>(header, tailer); /* Generate session */
                this->server_session->BindIOManager(manager);                                 /* Bind IO manager */
                this->server_session->SetMessageHandler(
                    [this](std::weak_ptr<vuprs::SocketIOManager> manager, const std::string &message)
                    {
                        this->SessionCallback(manager, message);
                    });                        /* Set message handler */
                this->server_session->Start(); /* Start session */
                std::cout << "[server][listening] successfully connect client: [" << manager->ClientInformation() << "]" << std::endl;
                this->ConnectCallback(true, manager->ClientInformation());
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void vuprs::LinuxServer::ConnectCallback(bool connect, const std::string &message)
{
    /* show connect information here */
}

void vuprs::LinuxServer::Run()
{
    bool bf_config_done;
    {
        std::lock_guard<std::mutex> lock(this->mut_bf); /* LOCK */
        bf_config_done = this->beamformer.ConfigDone();
    }
    if (this->config_done && bf_config_done)
    {
        /* Start server */
        RUNTIME_CHECK(this->InitServer(), "server", " in [LinuxServer::Run] Failed to init server.");
        this->server_running = true;
        this->threads.emplace_back([this]
                                   { this->THREAD__Control(); });
        this->threads.emplace_back([this]
                                   { this->THREAD__Send(); });
        this->threads.emplace_back([this]
                                   { this->THREAD__AcceptClient(); });
        /* Start beam former */
        vuprs::HybridBeamformerConfig config;
        {
            std::lock_guard<std::mutex> lock(this->mut_bf_config); /* LOCK */
            config = this->bf_config;
        }
        {
            std::lock_guard<std::mutex> lock(this->mut_bf); /* LOCK */
            this->beamformer.run(config);
        }
    }
    else
    {
        RUNTIME_CHECK(false, "server", " in [LinuxServer::Run] Initialize not complete.");
    }
}

void vuprs::LinuxServer::Stop()
{
    /* Stop beam former */
    {
        std::lock_guard<std::mutex> lock(this->mut_bf); /* LOCK */
        this->beamformer.stop();
    }
    /* Stop server */
    this->server_running = false;
    this->server_response_cv.notify_all();
    this->sending_cv.notify_all();
    this->control_cv.notify_all();
    if (this->server_fd >= 0)
    {
        shutdown(this->server_fd, SHUT_RDWR);
        close(this->server_fd);
        this->server_fd = -1;
    }
    if (this->server_session)
    {
        this->server_session->Stop();
    }
    for (auto &f : this->threads)
    {
        if (f.joinable())
        {
            f.join();
        }
    }
}

void vuprs::LinuxServer::THREAD__Send()
{
    std::vector<uint32_t> result_to_send;
    std::vector<uint16_t> scan_result_to_send;
    double max_scan_power_dB, min_scan_power_dB;
    bool queue_empty, status, send_ok;
    std::string header, tailer, send_string, info;
    uint32_t send_data_size; /* send data size in bytes (if send buffer) */
    {
        std::lock_guard<std::mutex> lock(this->mut_server_config); /* LOCK */
        header = this->server_config.protocol.command_header;
        tailer = this->server_config.protocol.command_tailer;
    }
    while (this->server_running)
    {
        {
            std::unique_lock<std::mutex> lock(this->mut_send); /* LOCK */
            this->sending_cv.wait(lock, [this]
                                  { return !this->server_running || this->sending_irq; });
        }
        if (!this->server_running)
            break;
        if (!this->sending_irq)
            continue;
        else
            this->sending_irq = false;
        std::shared_ptr<vuprs::SocketIOManager> manager;
        {
            std::lock_guard<std::mutex> lock(this->mut_client_io_manager); /* LOCK */
            manager = this->client_io_manager;
        }
        if (!manager)
        {
            std::cout << "[server][send] no client connected, cannot send." << std::endl;
            continue;
        }
        /* ---------------------------------------------------------------------- */
        /* ------------------------- Fork 1: Send data -------------------------- */
        /* ---------------------------------------------------------------------- */
        if (IS_BINARY_DATA_SENDING_CMD(this->sending_format))
        {
            /* STEP 1: Get data from queue */
            status = false;
            send_ok = true;
            {
                std::lock_guard<std::mutex> lock(this->mut_bf); /* LOCK */
                switch (this->sending_format)
                {
                case static_cast<uint32_t>(vuprs::ServerCommand::SERVER_CMD__GET_NEW_DATA):
                {
                    status = this->beamformer.ReadResult(&result_to_send);
                    send_data_size = result_to_send.size() * sizeof(uint32_t);
                    break;
                }
                case static_cast<uint32_t>(vuprs::ServerCommand::SERVER_CMD__SCAN_FOR_POSITION_POWER):
                {
                    status = this->beamformer.ReadScanPower(&scan_result_to_send, &max_scan_power_dB, &min_scan_power_dB);
                    send_data_size = scan_result_to_send.size() * sizeof(uint16_t);
                    break;
                }
                }
            }
            /* STEP 2: Generate send string */
            if (!status)
            {
                info = "failed";
            }
            switch (this->sending_format)
            {
            case static_cast<uint32_t>(vuprs::ServerCommand::SERVER_CMD__GET_NEW_DATA):
            {
                send_string = vuprs::PROTOCOL_MakeServerResultDataResponse(info, status);
                break;
            }
            case static_cast<uint32_t>(vuprs::ServerCommand::SERVER_CMD__SCAN_FOR_POSITION_POWER):
            {
                vuprs::ScanningConfig _scanConfig;
                {
                    std::lock_guard<std::mutex> lock(this->mut_scan_config); /* LOCK */
                    _scanConfig = this->scan_config;
                }
                send_string = vuprs::PROTOCOL_MakeServerScanningResponse(_scanConfig, min_scan_power_dB, max_scan_power_dB, info, status);
                break;
            }
            }
            send_string = vuprs::AddFrameIfMissing(send_string, header, tailer);

            /* STEP 3: Send data frames */
            /* Frame 1: basic information */
            send_ok &= manager->SendMessage(send_string);
            /* Frame 2: actual data */
            if (status)
            {
                send_ok &= manager->SendMessage(header);                /* Send header */
                send_ok &= manager->SendWord<uint32_t>(send_data_size); /* Send data size */
                switch (this->sending_format)
                {
                case static_cast<uint32_t>(vuprs::ServerCommand::SERVER_CMD__GET_NEW_DATA):
                {
                    send_ok &= manager->SendBuffer<uint32_t>(result_to_send); /* Send data */
                    break;
                }
                case static_cast<uint32_t>(vuprs::ServerCommand::SERVER_CMD__SCAN_FOR_POSITION_POWER):
                {
                    send_ok &= manager->SendBuffer<uint16_t>(scan_result_to_send); /* Send data */
                    break;
                }
                }
                send_ok &= manager->SendMessage(tailer); /* Send tailer */
            }
            if (!send_ok)
            {
                std::cout << "[server][send] send data frame failed." << std::endl;
            }
        }

        /* ---------------------------------------------------------------------- */
        /* ------------------- Fork 2: Send algorithm parameters ---------------- */
        /* ---------------------------------------------------------------------- */

        else if (this->sending_format == static_cast<uint32_t>(vuprs::ServerCommand::SERVER_CMD__GET_ALG_PARAM))
        {
            try
            {
                vuprs::HybridBeamformerConfig config;
                {
                    std::lock_guard<std::mutex> lock(this->mut_bf_config); /* LOCK */
                    config = this->bf_config;
                }
                send_string = vuprs::PROTOCOL_MakeServerParameterResponse(config);
                send_string = vuprs::AddFrameIfMissing(send_string, header, tailer);
            }
            catch (const std::exception &e)
            {
                std::cout << "Error in [LinuxServer::THREAD__Send] " << e.what() << std::endl;
            }
            send_ok = manager->SendMessage(send_string); /* Send data */
            if (!send_ok)
            {
                std::cout << "[server][send] send data frame failed." << std::endl;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

void vuprs::LinuxServer::THREAD__Control()
{
    vuprs::ServerCommandInformation _cmd_info;
    vuprs::HybridBeamformerConfig config;
    vuprs::ScanningConfig scan_config;
    bool operation_status = false;
    bool need_responseIn_this_thread = true;
    std::string header, tailer, response_message, error_info;
    {
        std::lock_guard<std::mutex> lock(this->mut_server_config); /* LOCK */
        header = this->server_config.protocol.command_header;
        tailer = this->server_config.protocol.command_tailer;
    }
    while (this->server_running)
    {
        {
            std::unique_lock<std::mutex> lock(this->mut_control); /* LOCK */
            this->control_cv.wait(lock, [this]
                                  { return !this->server_running || this->control_irq; });
        }
        if (!this->server_running)
            break;
        if (!this->control_irq)
            continue;
        else
            this->control_irq = false;
        /* Get command & config information */
        {
            std::lock_guard<std::mutex> lock(this->mut_cmd); /* LOCK */
            _cmd_info = this->cmd_info;
        }
        /* Change direction */
        operation_status = true;           /* Assume operation is successful, if any error occurs, set it to false */
        this->server_need_response = true; /* Indicate that a response is needed in session */
        error_info = "";                   /* Clear error info, if any error occurs, assign error info to this variable, and it will be added to response message */
        try
        {
            {
                std::lock_guard<std::mutex> lock(this->mut_bf_config); /* LOCK */
                config = this->bf_config;
            }
            switch (_cmd_info.cmd)
            {
            case vuprs::ServerCommand::SERVER_CMD__ACK:
            {
                /* Do nothing, just response with ack */
                break;
            }
            case vuprs::ServerCommand::SERVER_CMD__RESET: /* use this.config */
            {
                std::lock_guard<std::mutex> lock(this->mut_bf); /* LOCK */
                this->beamformer.stop();
                this->beamformer.run(config);
                break;
            }
            case vuprs::ServerCommand::SERVER_CMD__CHANGE_BEAMFORMER:
            {
                if (_cmd_info.beamformer_name == "dcrcb")
                {
                    std::lock_guard<std::mutex> lock(this->mut_bf); /* LOCK */
                    this->beamformer.BindBeamformer(std::make_unique<vuprs::Beamformer_DCRCB>());
                    this->beamformer.stop();
                    this->beamformer.run(config);
                }
                else if (_cmd_info.beamformer_name == "cbf")
                {
                    std::lock_guard<std::mutex> lock(this->mut_bf); /* LOCK */
                    this->beamformer.BindBeamformer(std::make_unique<vuprs::Beamformer_CBF>());
                    this->beamformer.stop();
                    this->beamformer.run(config);
                }
                else if (_cmd_info.beamformer_name == "mvdr")
                {
                    std::lock_guard<std::mutex> lock(this->mut_bf); /* LOCK */
                    this->beamformer.BindBeamformer(std::make_unique<vuprs::Beamformer_MVDR>());
                    this->beamformer.stop();
                    this->beamformer.run(config);
                }
                else
                {
                    PARAM_CHECK(false, "server", " in [LinuxServer::THREAD__Control] Invalid beamformer name: " + _cmd_info.beamformer_name);
                }
                break;
            }
            case vuprs::ServerCommand::SERVER_CMD__REDIRECT: /* use this.config */
            {
                {
                    std::lock_guard<std::mutex> lock(this->mut_bf_config); /* LOCK */
                    this->bf_config += _cmd_info.config;                   /* Merge config */
                    config = this->bf_config;
                }
                std::lock_guard<std::mutex> lock(this->mut_bf); /* LOCK */
                this->beamformer.ReDirect(config.bf_target__alt, config.bf_target__az, config.bf_wave_velocity);
                break;
            }
            case vuprs::ServerCommand::SERVER_CMD__START: /* use this.config */
            {
                std::lock_guard<std::mutex> lock(this->mut_bf); /* LOCK */
                this->beamformer.run(config);
                break;
            }
            case vuprs::ServerCommand::SERVER_CMD__STOP:
            {
                std::lock_guard<std::mutex> lock(this->mut_bf); /* LOCK */
                this->beamformer.stop();
                break;
            }
            case vuprs::ServerCommand::SERVER_CMD__SCAN_FOR_POSITION_POWER:
            {
                this->server_need_response = false; /* No need to response */
                this->sending_format = static_cast<uint32_t>(vuprs::ServerCommand::SERVER_CMD__SCAN_FOR_POSITION_POWER);
                {
                    std::lock_guard<std::mutex> lock(this->mut_scan_config); /* LOCK */
                    this->scan_config = _cmd_info.scan_config;
                }
                {
                    std::lock_guard<std::mutex> lock(this->mut_bf); /* LOCK */
                    this->beamformer.ScanOptions(
                        _cmd_info.scan_config.points_in_hemisphere,
                        _cmd_info.scan_config.alt_min,
                        config.bf_wave_velocity);
                    if (!this->beamformer.ScanSwitch())
                    {
                        this->beamformer.ScanSwitch(true); /* Enable scanning */
                    }
                }
                this->sending_irq = true;
                this->sending_cv.notify_all();
                break;
            }
            case vuprs::ServerCommand::SERVER_CMD__GET_NEW_DATA:
            {
                this->server_need_response = false; /* No need to response */
                this->sending_format = static_cast<uint32_t>(vuprs::ServerCommand::SERVER_CMD__GET_NEW_DATA);
                this->sending_irq = true;
                this->sending_cv.notify_all();
                break;
            }
            case vuprs::ServerCommand::SERVER_CMD__GET_ALG_PARAM:
            {
                this->server_need_response = false; /* Need to response */
                this->sending_format = static_cast<uint32_t>(vuprs::ServerCommand::SERVER_CMD__GET_ALG_PARAM);
                this->sending_irq = true;
                this->sending_cv.notify_all();
                break;
            }
            case vuprs::ServerCommand::SERVER_CMD__CHANGE_ALG_PARAM:
            {
                {
                    std::lock_guard<std::mutex> lock(this->mut_bf_config); /* LOCK */
                    this->bf_config += _cmd_info.config;                   /* Merge config */
                    config = this->bf_config;
                }
                std::lock_guard<std::mutex> lock(this->mut_bf); /* LOCK */
                this->beamformer.stop();
                this->beamformer.run(config);
                break;
            }
            default:
            {
                break;
            }
            }
        }
        catch (const std::exception &e)
        {
            operation_status = false;
            error_info = e.what();
            std::cout << "Error in [LinuxServer::THREAD__Control] " << error_info << std::endl;
        }
        /* Make server response in session callback (if needed) */
        if (this->server_need_response)
        {
            try
            {
                response_message = vuprs::PROTOCOL_MakeServerOperationResponse(_cmd_info, error_info, operation_status);
                response_message = vuprs::AddFrameIfMissing(response_message, header, tailer);
            }
            catch (const std::exception &e)
            {
                std::cout << "Error occurred while making server response: " << e.what() << std::endl;
            }

            {
                std::lock_guard<std::mutex> lock(this->mut_response);
                this->server_response_message = response_message;
            }
        }
        /* Wake up session and Send Response (session must be awake) */
        this->server_response_irq = true;
        this->server_response_cv.notify_all();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

/* ------------------------------------------------------------------------------------- */
/* -------------------------------- Called by session ---------------------------------- */
/* ------------------------------------------------------------------------------------- */

void vuprs::LinuxServer::SessionCallback(std::weak_ptr<vuprs::SocketIOManager> manager, const std::string &message)
{
    /* change algorithm parameters */
    bool parse_status = false;
    std::string send_string, header, tailer, _message;
    vuprs::ServerCommandInformation _cmd_info;
    /* Get Frame Header and Tailer */
    {
        std::lock_guard<std::mutex> lock(this->mut_server_config); /* LOCK */
        header = this->server_config.protocol.command_header;
        tailer = this->server_config.protocol.command_tailer;
    }
    /* Parse command */
    try
    {
        _message = vuprs::RemoveFrameIfExists(message, header, tailer);               /* Remove frame if exists, but do not check frame format */
        parse_status = vuprs::PROTOCOL_ParseCommandFromMessage(_message, &_cmd_info); /* NOTE: header & tailer is cut in advance */
    }
    catch (const std::exception &e)
    {
        std::cout << "Error occurred while parsing command from message: " << e.what() << std::endl;
    }
    /* --------------------------------------------------------------- */
    /* --------- Failed (invalid command): Immediate response -------- */
    /* --------------------------------------------------------------- */
    if (!parse_status)
    {
        /* Invalid command, response with error message */
        send_string = vuprs::PROTOCOL_MakeServerOperationResponse(_cmd_info, "invalid command", false);
        send_string = vuprs::AddFrameIfMissing(send_string, header, tailer);
        std::shared_ptr<vuprs::SocketIOManager> _manager = manager.lock();
        if (_manager != nullptr)
        {
            if (!_manager->SendMessage(send_string))
            {
                std::cout << "[server][response] failed to send parse-error response." << std::endl;
            }
        }
        return;
    }
    /* --------------------------------------------------------------- */
    /* ---------- Normal: Response by the certain thread ------------- */
    /* --------------------------------------------------------------- */
    /* Load command information */
    {
        std::lock_guard<std::mutex> lock(this->mut_cmd); /* LOCK */
        this->cmd_info = _cmd_info;
    }
    this->control_irq = true;
    this->control_cv.notify_all();
    /* Wait for wake up */
    {
        std::unique_lock<std::mutex> lock(this->mut_response);
        this->server_response_cv.wait(lock, [this]
                                      { return !this->server_running || this->server_response_irq.load(); });
    }
    if (!this->server_running)
        return;
    if (this->server_response_irq)
        this->server_response_irq = false;
    else
        return;
    /* Get response message */
    if (this->server_need_response)
    {
        {
            std::lock_guard<std::mutex> lock(this->mut_response);
            send_string = this->server_response_message;
        }
        /* Response */
        std::shared_ptr<vuprs::SocketIOManager> _manager;
        _manager = manager.lock();
        if (_manager != nullptr)
        {
            if (!_manager->SendMessage(send_string))
            {
                std::cout << "[server][response] failed to send response message." << std::endl;
            }
        }
    }
}
