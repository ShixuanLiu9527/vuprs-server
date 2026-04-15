#include "linux_server.h"

/* --------------------------------------------------------------------------------------------------------------- */
/* ----------------------------------------------- Linux Server -------------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

vuprs::LinuxServer::LinuxServer()
{
    this->configdone = false;

    this->controlIRQ = false;
    this->readResultIRQ = false;  /* true: should send */
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

        configResult &= this->beamformer.InitCollaborationBeamfomer(
            config.fpgaConfigJsonFile, 
            config.beamFormingArrayConfigJsonFile, 
            config.firFilterBankConfigJsonFile);

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
            this->client_io_manager.reset();
        }
        if (!this->server_session)  /* try to connect */
        {
            client_fd = accept(this->server_fd, (struct sockaddr*)&client_addr, &client_len);

            if (client_fd >= 0)
            {
                /* Generate session */

                this->client_io_manager = std::make_shared<vuprs::SocketIOManager>(client_fd, client_addr);  /* Generate IO manager */

                {
                    std::unique_lock<std::mutex> lock(this->mut_server_config);  /* LOCK */
                    this->server_session = std::make_unique<vuprs::LinuxSession>( 
                        this->server_config.protocol.commandHeader, this->server_config.protocol.commandTailer);  /* Generate session */
                }

                this->server_session->BindIOManager(this->client_io_manager);  /* Bind IO manager */

                this->server_session->SetMessageHandler(
                    [this](std::weak_ptr<vuprs::SocketIOManager> manager, const std::string& message){
                        this->SessionCallback(manager, message);
                });  /* Set message handler */
                
                this->server_session->Start();  /* Start session */
            
                std::cout << "[server][listening] successfully connect client: [" << this->client_io_manager->ClientInformation() << "]" << std::endl;
            
                this->ConnectCallback(true, this->client_io_manager->ClientInformation());
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
    if (this->configdone && this->beamformer.ConfigDone())
    {
        /* Start server */

        if (!this->InitServer()) throw std::runtime_error("in [LinuxServer::Run] Failed to init server.");

        this->server_running = true;

        this->threads.emplace_back([this]{this->THREAD__Control();});
        this->threads.emplace_back([this]{this->THREAD__GetResult();});
        this->threads.emplace_back([this]{this->THREAD__SendToMaster();});
        this->threads.emplace_back([this]{this->THREAD__AcceptClient();});

        /* Start beam former */

        std::unique_lock<std::mutex> lock(this->mut_bf_config);  /* LOCK */
        this->beamformer.RUN(this->beamFormerConfig);
    }
    else
    {
        throw std::runtime_error("in [LinuxServer::Run] Initialize not complete.");
    }
}

void vuprs::LinuxServer::Stop()
{
    /* Stop beam former */

    this->beamformer.STOP();

    /* Stop server */

    this->server_running = false;

    this->readResultCV.notify_all();
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

void vuprs::LinuxServer::THREAD__GetResult()
{
    std::vector<uint32_t> result;
    while (this->server_running)
    {
        if (this->beamformer.NewResultDataInput())
        {
            this->beamformer.ReadResultFromQueue(&result);
            {
                std::unique_lock<std::mutex> lock(this->mut_readResult);  /* LOCK */
                this->resultQueue.push(result);
                if (this->resultQueue.size() > DEFAULT_SENDING_DATA_QUEUE_LENGTH)
                {
                    this->resultQueue.pop();
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void vuprs::LinuxServer::THREAD__SendToMaster()
{
    std::vector<uint32_t> resultToSend;
    vuprs::ServerCommandInformation _cmdINFO;
    bool queueEmpty;
    std::string header, tailer, sendString;

    {
        std::unique_lock<std::mutex> lock(this->mut_server_config);  /* LOCK */
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

        /* ---------------------------------------------------------------------- */
        /* ------------------- Fork 1: Send result data ------------------------- */
        /* ---------------------------------------------------------------------- */

        if (this->sendingFormat == static_cast<uint32_t>(vuprs::ServerCommand::SERVER_CMD__GET_NEW_DATA))
        {
            /* Get data from queue */

            queueEmpty = true;

            {
                std::unique_lock<std::mutex> lock(this->mut_readResult);  /* LOCK */
                if (!this->resultQueue.empty())
                {
                    queueEmpty = false;
                    resultToSend = this->resultQueue.front();
                    this->resultQueue.pop();
                }
            }

            /* Send data */

            if (!queueEmpty)
            {
                std::shared_ptr<vuprs::SocketIOManager> manager = this->client_io_manager;
                if (!manager)
                {
                    continue;
                }

                bool sendOk = true;
                sendOk &= manager->SendMessage(header);  /* Send header */
                sendOk &= manager->SendBuffer<uint32_t>(resultToSend);  /* Send data */
                sendOk &= manager->SendMessage(tailer);  /* Send tailer */

                if (!sendOk)
                {
                    std::cout << "[server][send] send data frame failed." << std::endl;
                }
            }
        }

        /* ---------------------------------------------------------------------- */
        /* ------------------- Fork 2: Send algorithm parameters ---------------- */
        /* ---------------------------------------------------------------------- */

        else if (this->sendingFormat == static_cast<uint32_t>(vuprs::ServerCommand::SERVER_CMD__GET_ALG_PARAM))
        {
            try
            {
                std::unique_lock<std::mutex> lock(this->mut_bf_config);  /* LOCK */
                sendString = vuprs::PROTOCOL_MakeServerParameterResponse(this->beamFormerConfig);
                sendString = vuprs::AddFrameIfMissing(sendString, header, tailer);
            }
            catch(const std::exception& e)
            {
                std::cout << "Error in [LinuxServer::THREAD__SendToMaster] " << e.what() << std::endl;
            }

            std::shared_ptr<vuprs::SocketIOManager> manager = this->client_io_manager;
            if (!manager)
            {
                continue;
            }

            bool sendOk = true;
            sendOk &= manager->SendMessage(sendString);  /* Send data */

            if (!sendOk)
            {
                std::cout << "[server][send] send data frame failed." << std::endl;
            }
        }
    }
}

void vuprs::LinuxServer::THREAD__Control()
{
    vuprs::ServerCommandInformation _cmdINFO;
    bool operationStatus = false;
    bool needResponseInThisThread = true;

    std::string header, tailer, responseMessage, errorInfo;

    {
        std::unique_lock<std::mutex> lock(this->mut_server_config);  /* LOCK */
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
            std::unique_lock<std::mutex> lock(this->mut_cmd);  /* LOCK */
            _cmdINFO = this->cmdINFO;
        }

        /* Change direction */

        operationStatus = true;  /* Assume operation is successful, if any error occurs, set it to false */
        this->serverNeedResponse = true;  /* Indicate that a response is needed in session */
        errorInfo = "";  /* Clear error info, if any error occurs, assign error info to this variable, and it will be added to response message */

        try
        {
            switch (_cmdINFO.cmd)
            {
                case vuprs::ServerCommand::SERVER_CMD__ACK:
                {
                    /* Do nothing, just response with ack */
                    break;
                }
                case vuprs::ServerCommand::SERVER_CMD__RESET:  /* use this.config */
                {
                    this->beamformer.STOP();
                    std::unique_lock<std::mutex> lock(this->mut_bf_config);  /* LOCK */
                    this->beamformer.RUN(this->beamFormerConfig);
                    break;
                }
                case vuprs::ServerCommand::SERVER_CMD__CHANGE_BEAMFORMER:
                {
                    if (_cmdINFO.beamformer_name == "dcrcb")
                    {
                        this->beamformer.BindBeamformer(std::make_unique<vuprs::Beamformer_DCRCB>());
                        this->beamformer.STOP();
                        std::unique_lock<std::mutex> lock(this->mut_bf_config);  /* LOCK */
                        this->beamformer.RUN(this->beamFormerConfig);
                    }
                    else if (_cmdINFO.beamformer_name == "cbf")
                    {
                        this->beamformer.BindBeamformer(std::make_unique<vuprs::Beamformer_CBF>());
                        this->beamformer.STOP();
                        std::unique_lock<std::mutex> lock(this->mut_bf_config);  /* LOCK */
                        this->beamformer.RUN(this->beamFormerConfig);
                    }
                    else if (_cmdINFO.beamformer_name == "mvdr")
                    {
                        this->beamformer.BindBeamformer(std::make_unique<vuprs::Beamformer_MVDR>());
                        this->beamformer.STOP();
                        std::unique_lock<std::mutex> lock(this->mut_bf_config);  /* LOCK */
                        this->beamformer.RUN(this->beamFormerConfig);
                    }
                    else
                    {
                        throw std::runtime_error("in [LinuxServer::THREAD__Control] Invalid beamformer name: " + _cmdINFO.beamformer_name);
                    }
                    break;
                }
                case vuprs::ServerCommand::SERVER_CMD__REDIRECT:  /* use this.config */
                {
                    std::unique_lock<std::mutex> lock(this->mut_bf_config);  /* LOCK */
                    vuprs::Merge_ARM_FPGA_BF_Config(&this->beamFormerConfig, _cmdINFO.config, _cmdINFO.configMask);
                    this->beamformer.ReDirect(this->beamFormerConfig.bf_target__alt, this->beamFormerConfig.bf_target__az, this->beamFormerConfig.bf_waveVelocity);
                    break;
                }
                case vuprs::ServerCommand::SERVER_CMD__START:  /* use this.config */
                {
                    std::unique_lock<std::mutex> lock(this->mut_bf_config);  /* LOCK */
                    this->beamformer.RUN(this->beamFormerConfig);
                    break;
                }
                case vuprs::ServerCommand::SERVER_CMD__STOP: 
                {
                    this->beamformer.STOP();
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
                    std::unique_lock<std::mutex> lock(this->mut_bf_config);  /* LOCK */
                    vuprs::Merge_ARM_FPGA_BF_Config(&this->beamFormerConfig, _cmdINFO.config, _cmdINFO.configMask);
                    this->beamformer.STOP();
                    this->beamformer.RUN(this->beamFormerConfig);

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

        /* Make server response (if needed) */

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
                std::unique_lock<std::mutex> lock(this->mut_response);
                this->serverResponseMessage = responseMessage;
            }
        }

        /* Wake up session and Send Response (session must be awake) */

        this->serverResponseIRQ = true;
        this->serverResponseCV.notify_all();

        usleep(1000);
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
        std::unique_lock<std::mutex> lock(this->mut_server_config);  /* LOCK */
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
        std::unique_lock<std::mutex> lock(this->mut_cmd);  /* LOCK */
        this->cmdINFO = _cmdINFO;
    }

    this->controlIRQ = true;
    this->controlCV.notify_all();

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
            std::unique_lock<std::mutex> lock(this->mut_response);
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
