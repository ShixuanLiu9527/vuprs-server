#include "linux_server.h"

/* --------------------------------------------------------------------------------------------------------------- */
/* ----------------------------------------------- Linux Server -------------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

vuprs::LinuxServer::LinuxServer()
{
    vuprs::Set_ARM_FPGA_BF_Config_ToDefault(&this->beamFormerConfig);

    this->configdone = false;

    this->controlIRQ = false;
    this->readResultIRQ = false;  /* true: should send */
    this->serverResponseIRQ = false;
    this->resultSendingIRQ = false;  /* true: should send */
    this->controlIRQ = false;

    this->serverNeedResponse = false;  /* false = no need to send response */
}

vuprs::LinuxServer::~LinuxServer()
{

}

bool vuprs::LinuxServer::LoadServerConfigFromJson(const std::string& jsonFilename)
{
    std::ifstream configJsonFile;
    
    /* open config json file */

    configJsonFile.open(jsonFilename);
    if (!configJsonFile.is_open())
    {
        throw std::runtime_error("Cannot open file: " + jsonFilename);
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

    this->configdone = true;

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
            throw std::runtime_error("Config error.");
        }
    }
    catch (const std::exception &e)
    {
        std::cout << "Error: " << e.what() << std::endl;
    }
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
        if (this->server_session && !this->server_session->IsRun()) 
        {
            std::cout << "[server][listening] client disconnected." << std::endl;

            this->ConnectCallback(false, "");

            this->server_session->Stop();
            this->server_session.reset();
        }
        if (!this->server_session)  /* try to connect */
        {
            client_fd = accept(this->server_fd, (struct sockaddr*)&client_addr, &client_len);

            if (client_fd >= 0)
            {
                /* Generate session */

                this->socketIOManager = std::make_shared<vuprs::SocketIOManager>(client_fd, client_addr);  /* Generate IO manager */

                {
                    std::unique_lock<std::mutex> lock(this->mut_config);  /* LOCK */
                    this->server_session = std::make_unique<vuprs::LinuxSession>( 
                        this->server_config.protocol.commandHeader, this->server_config.protocol.commandTailer);  /* Generate session */
                }

                this->server_session->BindIOManager(this->socketIOManager);  /* Bind IO manager */

                this->server_session->SetMessageHandler([this](std::weak_ptr<vuprs::SocketIOManager> manager, const std::string& message){
                    this->SessionCallback(manager, message);
                });  /* Set message handler */
                
                this->server_session->Start();  /* Start session */
            
                std::cout << "[server][listening] successfully connect client: [" << this->socketIOManager->ClientInformation() << "]" << std::endl;
            
                this->ConnectCallback(true, this->socketIOManager->ClientInformation());
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

        if (!this->InitServer()) throw std::runtime_error("Failed to init server.");

        this->threads.emplace_back([this]{this->THREAD__AcceptClient();});
        this->threads.emplace_back([this]{this->THREAD__Control();});
        this->threads.emplace_back([this]{this->THREAD__GetResult();});
        this->threads.emplace_back([this]{this->THREAD__SendToMaster();});

        /* Start beam former */

        this->beamformer.RUN(this->beamFormerConfig);
    }
    else
    {
        throw std::runtime_error("Initialize not complete.");
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
    this->resultSendingCV.notify_all();
    this->controlCV.notify_all();

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
    std::vector<double> result;
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
    std::vector<double> resultToSend;
    vuprs::ServerCommandInformation _cmdINFO;
    bool queueEmpty;
    std::string header, tailer;

    {
        std::unique_lock<std::mutex> lock(this->mut_config);  /* LOCK */
        header = this->server_config.protocol.commandHeader;
        tailer = this->server_config.protocol.commandTailer;
    }

    while (this->server_running)
    {
        {
            std::unique_lock<std::mutex> lock(this->mut_send);  /* LOCK */
            this->resultSendingCV.wait(lock, [this]{
                return !this->server_running || this->resultSendingIRQ;
            });
        }

        if (!this->server_running) break;
        
        if (!this->resultSendingIRQ) continue;
        else this->resultSendingIRQ = false;

        queueEmpty = true;

        /* Get data from queue */

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
            this->socketIOManager->SendMessage(header);  /* Send header */
            this->socketIOManager->SendBuffer(resultToSend);  /* Send data */
            this->socketIOManager->SendMessage(tailer);  /* Send tailer */
        }
    }
}

void vuprs::LinuxServer::SessionCallback(std::weak_ptr<vuprs::SocketIOManager> manager, const std::string& message)
{
    /* change algorithm parameters */

    bool parseStatus = false;
    std::string sendString, header, tailer;

    try
    {
        std::unique_lock<std::mutex> lock(this->mut_cmd);  /* LOCK */
        parseStatus = vuprs::PROTOCOL_ParseCommandFromMessage(message, &this->cmdINFO);
    }
    catch(const std::exception& e)
    {
        std::cout << "Error: " << e.what() << std::endl;
    }

    if (parseStatus)
    {
        this->controlIRQ = true;
        this->controlCV.notify_all();
    }

    /* Wait for response */

    {
        std::unique_lock<std::mutex> lock(this->mut_response);
        this->serverResponseCV.wait(lock, [this]{
            return this->serverResponseIRQ.load();
        });
    }

    /* Get response message */

    if (this->serverResponseIRQ) this->serverResponseIRQ = false;

    {
        std::unique_lock<std::mutex> lock(this->mut_response);
        sendString = this->serverResponseMessage;
    }

    /* Response */

    std::shared_ptr<vuprs::SocketIOManager> _manager;

    if (this->serverNeedResponse)
    {
        {
            std::unique_lock<std::mutex> lock(this->mut_config);  /* LOCK */
            header = this->server_config.protocol.commandHeader;
            tailer = this->server_config.protocol.commandTailer;
        }
        _manager = manager.lock();
        if (_manager != nullptr)
        {
            _manager->SendMessage(header + sendString + tailer);
        }
    }
}

void vuprs::LinuxServer::THREAD__Control()
{
    vuprs::ServerCommandInformation _cmdINFO;
    bool operationStatus = false;

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

        operationStatus = false;
        this->serverNeedResponse = true;

        switch (_cmdINFO.cmd)
        {
            case vuprs::ServerCommand::SERVER_CMD__RESET:  /* use this.config */
            {
                this->beamformer.STOP();
                this->beamformer.RUN(this->beamFormerConfig);
                operationStatus = true;
                break;
            }
            case vuprs::ServerCommand::SERVER_CMD__REDIRECT:  /* use this.config */
            {
                this->beamformer.ReDirect(_cmdINFO.config.bf_target__alt, _cmdINFO.config.bf_target__az, _cmdINFO.config.bf_waveVelocity);
                operationStatus = true;
                break;
            }
            case vuprs::ServerCommand::SERVER_CMD__START:  /* use this.config */
            {
                this->beamformer.RUN(this->beamFormerConfig);
                operationStatus = true;
                break;
            }
            case vuprs::ServerCommand::SERVER_CMD__STOP: 
            {
                this->beamformer.STOP();
                operationStatus = true;
                break;
            }
            case vuprs::ServerCommand::SERVER_CMD__GET_NEW_DATA: 
            {
                this->serverNeedResponse = false;  /* No need to response */
                this->resultSendingIRQ = true;
                this->resultSendingCV.notify_all();
                operationStatus = true;
                break;
            }
            case vuprs::ServerCommand::SERVER_CMD__CHANGE_ALG_PARAM: 
            {
                this->beamFormerConfig = _cmdINFO.config;
                this->beamformer.STOP();
                this->beamformer.RUN(this->beamFormerConfig);
                operationStatus = true;
                break;
            }
            default:
            {
                break;
            }
        }

        /* Make server response */

        {
            std::unique_lock<std::mutex> lock(this->mut_response);
            this->serverResponseMessage = vuprs::PROTOCOL_MakeServerResponse(_cmdINFO, operationStatus);
        }

        /* Send Response */

        this->serverResponseIRQ = true;
        this->serverResponseCV.notify_all();

        usleep(1000);
    }
}
