#include "linux_server.h"

/* --------------------------------------------------------------------------------------------------------------- */
/* ----------------------------------------------- Linux Server -------------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

vuprs::LinuxServer::LinuxServer()
{
    vuprs::Set_ARM_FPGA_BF_Config_ToDefault(&this->beamFormerConfig);
    this->configdone = false;
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

                this->server_session = std::make_unique<vuprs::LinuxSession>(client_fd, client_addr, 
                    this->server_config.protocol.commandHeader, this->server_config.protocol.commandTailer);

                this->server_session->SetMessageHandler([this](int client_fd, const struct sockaddr_in& client_addr, const std::string& message){
                    this->SessionCallback(client_fd, client_addr, message);
                });
                
                this->server_session->Start();
            
                std::cout << "[server][listening] successfully connect client: [" << this->server_session->GetClientInfo() << "]" << std::endl;
            
                this->ConnectCallback(true, this->server_session->GetClientInfo());
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void vuprs::LinuxServer::ConnectCallback(bool connect, const std::string &message)
{
    /* show connect information here */
}

void vuprs::LinuxServer::SessionCallback(int client_fd, const struct sockaddr_in& client_addr, const std::string& message)
{
    /* change algorithm parameters */
}

void vuprs::LinuxServer::run()
{
    if (this->configdone && this->beamformer.ConfigDone())
    {
        /* Start server */

        /* Start beam former */

        this->beamformer.RUN(this->beamFormerConfig);
    }
    else
    {
        throw std::runtime_error("Initialize not complete.");
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
                std::unique_lock<std::mutex> lock(this->mut_data);  /* LOCK */
                this->resultQueue.push(result);
                if (this->resultQueue.size() > DEFAULT_SENDING_DATA_QUEUE_LENGTH)
                {
                    this->resultQueue.pop();
                }
            }
        }
    }
}

void vuprs::LinuxServer::THREAD__SendToMaster()
{

}

void vuprs::LinuxServer::THREAD__BeamFormerControl()
{
    while (this->server_running)
    {
        {
            std::unique_lock<std::mutex> lock(this->mut_control);  /* LOCK */
            this->beamFormerParameterCV.wait(lock, [this]{
                return !this->server_running || this->newTargetDirectionIRQ;
            });
        }

        /* Change direction */
    }
}
