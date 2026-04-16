#include "linux_server.h"

/* --------------------------------------------------------------------------------------------------------------- */
/* ----------------------------------------------- Linux Server -------------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

vuprs::LinuxServer::LinuxServer()
{
    this->configdone = false;

    this->controlIRQ = false;
    this->serverResponseIRQ = false;
    this->sendingIRQ = false;  /* true: should send */
    this->controlIRQ = false;

    this->serverNeedResponse = false;  /* false = no need to send response */
}

vuprs::LinuxServer::~LinuxServer()
{
    this->Stop();
}

bool vuprs::LinuxServer::LoadServerConfigFromJson(const std::string& jsonFilename)
{
    std::ifstream configJsonFile;
    
    /* open config json file */

    configJsonFile.open(jsonFilename);
    if (!configJsonFile.is_open())
    {
        throw std::runtime_error("in [LinuxServer::LoadServerConfigFromJson] Cannot open file: " + jsonFilename);
    }

    nlohmann::json configJsonData;
    
    try
    {
        configJsonFile >> configJsonData;
    }
    catch(const std::exception& e)
    {
        throw std::runtime_error("Error" + std::string(e.what()));
    }

    vuprs::__JsonStringParseINT<uint16_t>(&this->server_config.initializePort, configJsonData, "initialize-port", true);
    vuprs::__JsonStringParseINT<uint16_t>(&this->server_config.maximumPort, configJsonData, "max-port", true);
    vuprs::__JsonStringParseINT<uint16_t>(&this->server_config.acceptClientCounts, configJsonData, "accept-client-counts", true);

    if (configJsonData.contains("protocol"))
    {
        auto protocol = configJsonData["protocol"];

        vuprs::__JsonParseString(&this->server_config.protocol.commandHeader, protocol, "command-header", true);
        vuprs::__JsonParseString(&this->server_config.protocol.commandTailer, protocol, "command-tailer", true);
    }

    return true;
}

void vuprs::LinuxServer::InitSystemConfigFiles(const vuprs::SystemConfigFiles &config)
{
    bool configResult = true;
    try
    {
        /* Config server */

        configResult &= this->LoadServerConfigFromJson(config.serverConfigJsonFile);

        /* Config beam former */

        {
            std::lock_guard<std::mutex> lock(this->mut_bf);  /* LOCK */
            configResult &= this->beamformer.InitCollaborationBeamformer(
                config.fpgaConfigJsonFile, 
                config.beamFormingArrayConfigJsonFile, 
                config.firFilterBankConfigJsonFile);
        }

        if (!configResult)
        {
            throw std::runtime_error("in [LinuxServer::InitSystemConfigFiles] Config error.");
        }

        this->configdone = true;
    }
    catch (const std::exception &e)
    {
        std::cout << "Error in [LinuxServer::InitSystemConfigFiles] " << e.what() << std::endl;
    }
}

bool vuprs::LinuxServer::ConfigDone() const
{
    return this->configdone;
}

bool vuprs::LinuxServer::InitServer()
{
    for (uint16_t currentPort = this->server_config.initializePort; currentPort <= this->server_config.maximumPort; currentPort++)
    {
        /* Get current port */
        
        this->server_port = currentPort;
        std::cout << "[server] trying to start server use port [" << this->server_port << "]." << std::endl;

        try
        {
            /* STEP 1: Create socket */

            this->server_fd = socket(AF_INET, SOCK_STREAM, 0);  /* IPV-4 TCP */

            if (this->server_fd < 0)
            {
                throw std::runtime_error("[server] failed to create socket.");
            }
        
            int opt = 1;
            if (setsockopt(this->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) 
            {
                close(this->server_fd);
                this->server_fd = -1;
                throw std::runtime_error("[server] set option error.");
            }

            /* STEP 2: Bind this process to PORT */

            struct sockaddr_in server_addr;
            memset(&server_addr, 0, sizeof(server_addr));
        
            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = INADDR_ANY;  /* bind to all IP address */
            server_addr.sin_port = htons(this->server_port);  /* bind to certain PORT */

            if (bind(this->server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) 
            {
                close(this->server_fd);
                this->server_fd = -1;

                throw std::runtime_error("[server] bind failed for port: " + std::to_string(this->server_port));
            }

            /* STEP 3: Listen */
        
            if (listen(this->server_fd, this->server_config.acceptClientCounts) < 0) 
            {
                close(this->server_fd);
                this->server_fd = -1;

                throw std::runtime_error("[server] listening error.");
            }

            std::cout << "[server] server startup successfully on port [" << this->server_port << "]." << std::endl;
            return true;
        }
        catch(const std::exception& e)
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
        if (this->server_session && !this->server_session->IsRun())  /* Close session */
        {
            std::cout << "[server][listening] client disconnected." << std::endl;

            this->ConnectCallback(false, "");

            this->server_session->Stop();
            this->server_session.reset();

            std::lock_guard<std::mutex> lock(this->mut_client_io_manager);  /* LOCK */
            this->client_io_manager.reset();
        }
        if (!this->server_session)  /* try to connect */
        {
            client_fd = accept(this->server_fd, (struct sockaddr*)&client_addr, &client_len);

            if (client_fd >= 0)
            {
                /* Generate session */

                std::shared_ptr<vuprs::SocketIOManager> manager;

                {
                    std::lock_guard<std::mutex> lock(this->mut_client_io_manager);  /* LOCK */
                    this->client_io_manager = std::make_shared<vuprs::SocketIOManager>(client_fd, client_addr);  /* Generate IO manager */
                    manager = this->client_io_manager;
                }

                std::string header, tailer;

                {
                    std::lock_guard<std::mutex> lock(this->mut_server_config);  /* LOCK */
                    header = this->server_config.protocol.commandHeader;
                    tailer = this->server_config.protocol.commandTailer;
                }

                this->server_session = std::make_unique<vuprs::LinuxSession>(header, tailer);  /* Generate session */
                this->server_session->BindIOManager(manager);  /* Bind IO manager */
                this->server_session->SetMessageHandler(
                    [this](std::weak_ptr<vuprs::SocketIOManager> manager, const std::string& message){
                        this->SessionCallback(manager, message);
                });  /* Set message handler */
                
                this->server_session->Start();  /* Start session */
            
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
    bool bfConfigDone;

    {
        std::lock_guard<std::mutex> lock(this->mut_bf);  /* LOCK */
        bfConfigDone = this->beamformer.ConfigDone();
    }
    
    if (this->configdone && bfConfigDone)
    {
        /* Start server */

        if (!this->InitServer()) throw std::runtime_error("in [LinuxServer::Run] Failed to init server.");

        this->server_running = true;

        this->threads.emplace_back([this]{this->THREAD__Control();});
        this->threads.emplace_back([this]{this->THREAD__Send();});
        this->threads.emplace_back([this]{this->THREAD__AcceptClient();});

        /* Start beam former */

        vuprs::ARM_FPGA_BF_Config config;

        {
            std::lock_guard<std::mutex> lock(this->mut_bf_config);  /* LOCK */
            config = this->beamFormerConfig;
        }
        
        {
            std::lock_guard<std::mutex> lock(this->mut_bf);  /* LOCK */
            this->beamformer.RUN(config);
        }
    }
    else
    {
        throw std::runtime_error("in [LinuxServer::Run] Initialize not complete.");
    }
}

void vuprs::LinuxServer::Stop()
{
    /* Stop beam former */

    {
        std::lock_guard<std::mutex> lock(this->mut_bf);  /* LOCK */
        this->beamformer.STOP();
    }

    /* Stop server */

    this->server_running = false;

    this->serverResponseCV.notify_all();
    this->sendingCV.notify_all();
    this->controlCV.notify_all();

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

    for (auto &f: this->threads)
    {
        if (f.joinable())
        {
            f.join();
        }
    }
}

void vuprs::LinuxServer::THREAD__Send()
{
    std::vector<uint32_t> resultToSend;
    std::vector<uint16_t> scanResultToSend;
    double maxScanPowerDB, minScanPowerDB;
    bool queueEmpty, status, sendOk;
    std::string header, tailer, sendString, info;
    uint32_t sendDataSize;  /* send data size in bytes (if send buffer) */

    {
        std::lock_guard<std::mutex> lock(this->mut_server_config);  /* LOCK */
        header = this->server_config.protocol.commandHeader;
        tailer = this->server_config.protocol.commandTailer;
    }

    while (this->server_running)
    {
        {
            std::unique_lock<std::mutex> lock(this->mut_send);  /* LOCK */
            this->sendingCV.wait(lock, [this]{
                return !this->server_running || this->sendingIRQ;
            });
        }

        if (!this->server_running) break;
        
        if (!this->sendingIRQ) continue;
        else this->sendingIRQ = false;

        std::shared_ptr<vuprs::SocketIOManager> manager;

        {
            std::lock_guard<std::mutex> lock(this->mut_client_io_manager);  /* LOCK */
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

        if (IS_BINARY_DATA_SENDING_CMD(this->sendingFormat))
        {
            /* STEP 1: Get data from queue */

            status = false;
            sendOk = true;
            
            {
                std::lock_guard<std::mutex> lock(this->mut_bf);  /* LOCK */
                switch (this->sendingFormat)
                {
                    case static_cast<uint32_t>(vuprs::ServerCommand::SERVER_CMD__GET_NEW_DATA):
                    {
                        status = this->beamformer.ReadResultFromQueue(&resultToSend);
                        sendDataSize = resultToSend.size() * sizeof(uint32_t);
                        break;
                    }
                    case static_cast<uint32_t>(vuprs::ServerCommand::SERVER_CMD__SCAN_FOR_POSITION_POWER):
                    {
                        status = this->beamformer.ReadScanPowerFromQueue(&scanResultToSend, &maxScanPowerDB, &minScanPowerDB);
                        sendDataSize = scanResultToSend.size() * sizeof(uint16_t);
                        break;
                    }
                }
            }

            /* STEP 2: Generate send string */

            if (!status)
            {
                info = "failed";
            }
            switch (this->sendingFormat)
            {
                case static_cast<uint32_t>(vuprs::ServerCommand::SERVER_CMD__GET_NEW_DATA):
                {
                    sendString = vuprs::PROTOCOL_MakeServerResultDataResponse(info, status);
                    break;
                }
                case static_cast<uint32_t>(vuprs::ServerCommand::SERVER_CMD__SCAN_FOR_POSITION_POWER):
                {
                    vuprs::ScanningConfig _scanConfig;
                    {
                        std::lock_guard<std::mutex> lock(this->mut_scan_config);  /* LOCK */
                        _scanConfig = this->scanningConfig;
                    }
                    sendString = vuprs::PROTOCOL_MakeServerScanningResponse(_scanConfig, minScanPowerDB, maxScanPowerDB, info, status);
                    break;
                }
            }
            sendString = vuprs::AddFrameIfMissing(sendString, header, tailer);
            
            /* STEP 3: Send data frames */

            /* Frame 1: basic information */

            sendOk &= manager->SendMessage(sendString);

            /* Frame 2: actual data */

            if (status)
            {
                sendOk &= manager->SendMessage(header);  /* Send header */
                sendOk &= manager->SendWord<uint32_t>(sendDataSize);  /* Send data size */
                switch (this->sendingFormat)
                {
                    case static_cast<uint32_t>(vuprs::ServerCommand::SERVER_CMD__GET_NEW_DATA):
                    {
                        sendOk &= manager->SendBuffer<uint32_t>(resultToSend);  /* Send data */
                        break;
                    }
                    case static_cast<uint32_t>(vuprs::ServerCommand::SERVER_CMD__SCAN_FOR_POSITION_POWER):
                    {
                        sendOk &= manager->SendBuffer<uint16_t>(scanResultToSend);  /* Send data */
                        break;
                    }
                }
                sendOk &= manager->SendMessage(tailer);  /* Send tailer */
            }
            
            if (!sendOk)
            {
                std::cout << "[server][send] send data frame failed." << std::endl;
            }
        }

        /* ---------------------------------------------------------------------- */
        /* ------------------- Fork 2: Send algorithm parameters ---------------- */
        /* ---------------------------------------------------------------------- */

        else if (this->sendingFormat == static_cast<uint32_t>(vuprs::ServerCommand::SERVER_CMD__GET_ALG_PARAM))
        {
            try
            {
                vuprs::ARM_FPGA_BF_Config config;
                {
                    std::lock_guard<std::mutex> lock(this->mut_bf_config);  /* LOCK */
                    config = this->beamFormerConfig;
                }
                sendString = vuprs::PROTOCOL_MakeServerParameterResponse(config);
                sendString = vuprs::AddFrameIfMissing(sendString, header, tailer);
            }
            catch(const std::exception& e)
            {
                std::cout << "Error in [LinuxServer::THREAD__Send] " << e.what() << std::endl;
            }

            sendOk = manager->SendMessage(sendString);  /* Send data */

            if (!sendOk)
            {
                std::cout << "[server][send] send data frame failed." << std::endl;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

void vuprs::LinuxServer::THREAD__Control()
{
    vuprs::ServerCommandInformation _cmdINFO;
    vuprs::ARM_FPGA_BF_Config config;
    vuprs::ScanningConfig scanningConfig;
    bool operationStatus = false;
    bool needResponseInThisThread = true;

    std::string header, tailer, responseMessage, errorInfo;

    {
        std::lock_guard<std::mutex> lock(this->mut_server_config);  /* LOCK */
        header = this->server_config.protocol.commandHeader;
        tailer = this->server_config.protocol.commandTailer;
    }

    while (this->server_running)
    {
        {
            std::unique_lock<std::mutex> lock(this->mut_control);  /* LOCK */
            this->controlCV.wait(lock, [this]{
                return !this->server_running || this->controlIRQ;
            });
        }

        if (!this->server_running) break;

        if (!this->controlIRQ) continue;
        else this->controlIRQ = false;

        /* Get command & config information */

        {
            std::lock_guard<std::mutex> lock(this->mut_cmd);  /* LOCK */
            _cmdINFO = this->cmdINFO;
        }

        /* Change direction */

        operationStatus = true;  /* Assume operation is successful, if any error occurs, set it to false */
        this->serverNeedResponse = true;  /* Indicate that a response is needed in session */
        errorInfo = "";  /* Clear error info, if any error occurs, assign error info to this variable, and it will be added to response message */

        try
        {
            {
                std::lock_guard<std::mutex> lock(this->mut_bf_config);  /* LOCK */
                config = this->beamFormerConfig;
            }
            switch (_cmdINFO.cmd)
            {
                case vuprs::ServerCommand::SERVER_CMD__ACK:
                {
                    /* Do nothing, just response with ack */
                    break;
                }
                case vuprs::ServerCommand::SERVER_CMD__RESET:  /* use this.config */
                {
                    std::lock_guard<std::mutex> lock(this->mut_bf);  /* LOCK */
                    this->beamformer.STOP();
                    this->beamformer.RUN(config);
                    break;
                }
                case vuprs::ServerCommand::SERVER_CMD__CHANGE_BEAMFORMER:
                {
                    if (_cmdINFO.beamformer_name == "dcrcb")
                    {
                        std::lock_guard<std::mutex> lock(this->mut_bf);  /* LOCK */
                        this->beamformer.BindBeamformer(std::make_unique<vuprs::Beamformer_DCRCB>());
                        this->beamformer.STOP();
                        this->beamformer.RUN(config);
                    }
                    else if (_cmdINFO.beamformer_name == "cbf")
                    {
                        std::lock_guard<std::mutex> lock(this->mut_bf);  /* LOCK */
                        this->beamformer.BindBeamformer(std::make_unique<vuprs::Beamformer_CBF>());
                        this->beamformer.STOP();
                        this->beamformer.RUN(config);
                    }
                    else if (_cmdINFO.beamformer_name == "mvdr")
                    {
                        std::lock_guard<std::mutex> lock(this->mut_bf);  /* LOCK */
                        this->beamformer.BindBeamformer(std::make_unique<vuprs::Beamformer_MVDR>());
                        this->beamformer.STOP();
                        this->beamformer.RUN(config);
                    }
                    else
                    {
                        throw std::runtime_error("in [LinuxServer::THREAD__Control] Invalid beamformer name: " + _cmdINFO.beamformer_name);
                    }
                    break;
                }
                case vuprs::ServerCommand::SERVER_CMD__REDIRECT:  /* use this.config */
                {
                    {
                        std::lock_guard<std::mutex> lock(this->mut_bf_config);  /* LOCK */
                        vuprs::Merge_ARM_FPGA_BF_Config(&this->beamFormerConfig, _cmdINFO.config, _cmdINFO.configMask);
                        config = this->beamFormerConfig;
                    }
                    std::lock_guard<std::mutex> lock(this->mut_bf);  /* LOCK */
                    this->beamformer.ReDirect(config.bf_target__alt, config.bf_target__az, config.bf_waveVelocity);
                    break;
                }
                case vuprs::ServerCommand::SERVER_CMD__START:  /* use this.config */
                {
                    std::lock_guard<std::mutex> lock(this->mut_bf);  /* LOCK */
                    this->beamformer.RUN(config);
                    break;
                }
                case vuprs::ServerCommand::SERVER_CMD__STOP: 
                {
                    std::lock_guard<std::mutex> lock(this->mut_bf);  /* LOCK */
                    this->beamformer.STOP();
                    break;
                }
                case vuprs::ServerCommand::SERVER_CMD__SCAN_FOR_POSITION_POWER:
                {
                    this->serverNeedResponse = false;  /* No need to response */
                    this->sendingFormat = static_cast<uint32_t>(vuprs::ServerCommand::SERVER_CMD__SCAN_FOR_POSITION_POWER);

                    {
                        std::lock_guard<std::mutex> lock(this->mut_scan_config);  /* LOCK */
                        this->scanningConfig = _cmdINFO.scanningConfig;
                    }

                    {
                        std::lock_guard<std::mutex> lock(this->mut_bf);  /* LOCK */
                        this->beamformer.ScanOptions(
                            _cmdINFO.scanningConfig.pointsInHalf, 
                            _cmdINFO.scanningConfig.alt_min, 
                            config.bf_waveVelocity);
                        if (!this->beamformer.ScanSwitch()) 
                        {
                            this->beamformer.ScanSwitch(true);  /* Enable scanning */
                        }
                    }
                    
                    this->sendingIRQ = true;
                    this->sendingCV.notify_all();

                    break;
                }
                case vuprs::ServerCommand::SERVER_CMD__GET_NEW_DATA: 
                {
                    this->serverNeedResponse = false;  /* No need to response */
                    this->sendingFormat = static_cast<uint32_t>(vuprs::ServerCommand::SERVER_CMD__GET_NEW_DATA);

                    this->sendingIRQ = true;
                    this->sendingCV.notify_all();

                    break;
                }
                case vuprs::ServerCommand::SERVER_CMD__GET_ALG_PARAM:
                {
                    this->serverNeedResponse = false;  /* Need to response */
                    this->sendingFormat = static_cast<uint32_t>(vuprs::ServerCommand::SERVER_CMD__GET_ALG_PARAM);

                    this->sendingIRQ = true;
                    this->sendingCV.notify_all();

                    break;
                }
                case vuprs::ServerCommand::SERVER_CMD__CHANGE_ALG_PARAM: 
                {
                    {
                        std::lock_guard<std::mutex> lock(this->mut_bf_config);  /* LOCK */
                        vuprs::Merge_ARM_FPGA_BF_Config(&this->beamFormerConfig, _cmdINFO.config, _cmdINFO.configMask);
                        config = this->beamFormerConfig;
                    }
                    std::lock_guard<std::mutex> lock(this->mut_bf);  /* LOCK */
                    this->beamformer.STOP();
                    this->beamformer.RUN(config);
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
            operationStatus = false;
            errorInfo = e.what();
            std::cout << "Error in [LinuxServer::THREAD__Control] " << errorInfo << std::endl;
        }

        /* Make server response in session callback (if needed) */

        if (this->serverNeedResponse)
        {
            try
            {
                responseMessage = vuprs::PROTOCOL_MakeServerOperationResponse(_cmdINFO, errorInfo, operationStatus);
                responseMessage = vuprs::AddFrameIfMissing(responseMessage, header, tailer);
            }
            catch(const std::exception& e)
            {
                std::cout << "Error occurred while making server response: " << e.what() << std::endl;
            }

            {
                std::lock_guard<std::mutex> lock(this->mut_response);
                this->serverResponseMessage = responseMessage;
            }
        }

        /* Wake up session and Send Response (session must be awake) */

        this->serverResponseIRQ = true;
        this->serverResponseCV.notify_all();

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

/* ------------------------------------------------------------------------------------- */
/* -------------------------------- Called by session ---------------------------------- */
/* ------------------------------------------------------------------------------------- */

void vuprs::LinuxServer::SessionCallback(std::weak_ptr<vuprs::SocketIOManager> manager, const std::string& message)
{
    /* change algorithm parameters */

    bool parseStatus = false;
    std::string sendString, header, tailer, _message;
    vuprs::ServerCommandInformation _cmdINFO;

    /* Get Frame Header and Tailer */

    {
        std::lock_guard<std::mutex> lock(this->mut_server_config);  /* LOCK */
        header = this->server_config.protocol.commandHeader;
        tailer = this->server_config.protocol.commandTailer;
    }

    /* Parse command */

    try
    {
        _message = vuprs::RemoveFrameIfExists(message, header, tailer);  /* Remove frame if exists, but do not check frame format */
        parseStatus = vuprs::PROTOCOL_ParseCommandFromMessage(_message, &_cmdINFO);  /* NOTE: header & tailer is cut in advance */
    }
    catch(const std::exception& e)
    {
        std::cout << "Error occurred while parsing command from message: " << e.what() << std::endl;
    }

    /* --------------------------------------------------------------- */
    /* --------- Failed (invalid command): Immediate response -------- */
    /* --------------------------------------------------------------- */

    if (!parseStatus)
    {
        /* Invalid command, response with error message */

        sendString = vuprs::PROTOCOL_MakeServerOperationResponse(_cmdINFO, "invalid command", false);
        sendString = vuprs::AddFrameIfMissing(sendString, header, tailer);

        std::shared_ptr<vuprs::SocketIOManager> _manager = manager.lock();
        if (_manager != nullptr)
        {
            if (!_manager->SendMessage(sendString))
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
        std::lock_guard<std::mutex> lock(this->mut_cmd);  /* LOCK */
        this->cmdINFO = _cmdINFO;
    }

    this->controlIRQ = true;
    this->controlCV.notify_all();

    /* Wait for wake up */

    {
        std::unique_lock<std::mutex> lock(this->mut_response);
        this->serverResponseCV.wait(lock, [this]{
            return !this->server_running || this->serverResponseIRQ.load();
        });
    }

    if (!this->server_running) return;
    if (this->serverResponseIRQ) this->serverResponseIRQ = false;
    else return;

    /* Get response message */

    if (this->serverNeedResponse)
    {
        {
            std::lock_guard<std::mutex> lock(this->mut_response);
            sendString = this->serverResponseMessage;
        }

        /* Response */

        std::shared_ptr<vuprs::SocketIOManager> _manager;

        _manager = manager.lock();
        if (_manager != nullptr)
        {
            if (!_manager->SendMessage(sendString))
            {
                std::cout << "[server][response] failed to send response message." << std::endl;
            }
        }
    }
}
