#include "tcp_server.h"

bool vuprs::TcpServer::loadConfigFromJson(const std::string& jsonFilename)
{
    std::ifstream configJsonFile;
    uint64_t value;
    bool parseStatus = false, retvalue = true;

    /* open config json file */

    configJsonFile.open(jsonFilename);
    if (!configJsonFile.is_open())
    {
        throw std::runtime_error("Cannot open file: " + jsonFilename);
    }

    nlohmann::json configJsonData;
    int configSuccessCount = 0;

    configJsonFile >> configJsonData;

    /* Parse config value */

    if (configJsonData.contains("initialize-port"))
    {
        value = vuprs::ParseNumberFromString(configJsonData["initialize-port"].get<std::string>(), &parseStatus);
        if (parseStatus) this->serverConfig.initializePort = static_cast<uint16_t>(value);
        else 
        {
            std::cout << "[server] missing element <initialize-port> in " << jsonFilename << std::endl;
            retvalue = false;
        }
    }
    if (configJsonData.contains("max-port"))
    {
        value = vuprs::ParseNumberFromString(configJsonData["max-port"].get<std::string>(), &parseStatus);
        if (parseStatus) this->serverConfig.maximumPort = static_cast<uint16_t>(value);
        else 
        {
            std::cout << "[server] missing element <max-port> in " << jsonFilename << std::endl;
            retvalue = false;
        }
    }
    if (configJsonData.contains("accept-client-counts"))
    {
        value = vuprs::ParseNumberFromString(configJsonData["accept-client-counts"].get<std::string>(), &parseStatus);
        if (parseStatus) this->serverConfig.acceptClientCounts = static_cast<uint16_t>(value);
        else 
        {
            std::cout << "[server] missing element <accept-client-counts> in " << jsonFilename << std::endl;
            retvalue = false;
        }
    }

    if (this->serverConfig.maximumPort < this->serverConfig.initializePort)
    {
        std::cout << "[server] <max-port> must bigger than <initialize port> " << std::endl;
        retvalue = false;
    }

    return retvalue;
}

vuprs::TcpServer::TcpServer(const std::string& configJsonFilename)
{
    std::cout << "[server] load config information from: " << configJsonFilename << std::endl;
    if (!this->loadConfigFromJson(configJsonFilename))
    {
        std::cout << "[server] failed to load config information, the default parameters are used." << std::endl;

        this->serverConfig.initializePort = __DEFAULT_INITIALIZE_PORT__;
        this->serverConfig.maximumPort = __DEFAULT_MAX_PORT__;
        this->serverConfig.acceptClientCounts = __DEFAULT_ACCEPT_CLIENT_COUNT__;
    }
    this->port = this->serverConfig.initializePort;
    this->server_fd = -1;
    this->running = false;
}

vuprs::TcpServer::TcpServer(const uint64_t& port)
{
    this->port = port;
    this->server_fd = -1;
    this->running = false;
}

vuprs::TcpServer::~TcpServer() 
{
    this->stop();
}

void vuprs::TcpServer::start()
{
    if (this->running) return;
    
    if (this->initializeSocket())
    {
        this->running = true;
        std::cout << "[server] open listening thread." << std::endl;
        this->serverThread = std::thread(&TcpServer::acceptConnection, this);
    }
    else
    {
        this->running = false;
    }
}

void vuprs::TcpServer::stop()
{
    if (!this->running) return;
    
    this->running = false;
    
    if (this->currentSession) 
    {
        this->currentSession->stop();
        this->currentSession.reset();
    }
    
    if (this->server_fd >= 0) 
    {
        close(this->server_fd);
        this->server_fd = -1;
    }
    
    if (this->serverThread.joinable()) 
    {
        this->serverThread.join();
    }
    
    std::cout << "[server] server has stopped." << std::endl;
}

bool vuprs::TcpServer::isRunning() const 
{
    return this->running;
}

bool vuprs::TcpServer::sendToClient(const std::string& message) 
{
    if (!(this->currentSession && this->currentSession->isRunning())) 
    {
        std::cout << "[server] no client connect, operate <send> is invalid." << std::endl;
        return false;
    }
    
    if (this->currentSession->sendData(message))
    {
        std::cout << "[server] server successfully send message to client: " << message << std::endl;
        return true;
    }
    else
    {
        std::cerr << "[server] server failed to send message." << std::endl;
        return false;
    }
}

void vuprs::TcpServer::setServerConnectionCallback(ServerConnectionCallback callback)
{
    this->serverConnectionCallback = std::move(callback);
}

void vuprs::TcpServer::setSessionMessageHandler(vuprs::SessionMessageHandler handler)
{
    this->sessionMessageHandler = std::move(handler);
}

bool vuprs::TcpServer::initializeSocket() 
{
    for (uint16_t currentPort = this->serverConfig.initializePort; currentPort <= this->serverConfig.maximumPort; currentPort++)
    {
        /* Get current port */
        
        this->port = currentPort;
        std::cout << "[server] trying to start server use port [" << this->port << "]." << std::endl;

        try
        {
            this->server_fd = socket(AF_INET, SOCK_STREAM, 0);

            if (this->server_fd < 0)
            {
                throw NetworkException("[server] failed to create socket.", errno);
            }
        
            int opt = 1;
            if (setsockopt(this->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) 
            {
                close(this->server_fd);
                this->server_fd = -1;
                throw NetworkException("[server] set option error.", errno);
            }

            struct sockaddr_in server_addr;
            memset(&server_addr, 0, sizeof(server_addr));
        
            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = INADDR_ANY;
            server_addr.sin_port = htons(this->port);

            if (bind(this->server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) 
            {
                close(this->server_fd);
                this->server_fd = -1;

                if (errno == EADDRINUSE)
                {
                    std::cout << "[server] port [" << this->port << "] is already in use, trying next." << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }
                else if (errno == EACCES)
                {
                    std::cout << "[server] Fatal bind error: " << strerror(errno) << std::endl;
                    return false;
                }

                throw NetworkException("[server] bind failed.", errno);
            }
        
            if (listen(this->server_fd, this->serverConfig.acceptClientCounts) < 0) 
            {
                close(this->server_fd);
                this->server_fd = -1;
                throw NetworkException("[server] listening error.", errno);
            }

            std::cout << "[server] server startup successfully on port [" << this->port << "]." << std::endl;
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

void vuprs::TcpServer::acceptConnection()
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = -1;

    std::cout << "[server][listening] waiting for client connect ... ..." << std::endl;

    while (this->running)
    {
        if (this->currentSession && !this->currentSession->isRunning()) 
        {
            std::cout << "[server][listening] client disconnected." << std::endl;
            if (this->serverConnectionCallback)
            {
                this->serverConnectionCallback(false, "");
            }
            this->currentSession->stop();
            this->currentSession.reset();
        }
        if (!this->currentSession)  /* try to connect */
        {
            client_fd = accept(this->server_fd, (struct sockaddr*)&client_addr, &client_len);

            if (client_fd >= 0)
            {
                /* Generate session */

                this->currentSession = std::make_unique<TcpSession>(client_fd, client_addr);
                this->currentSession->setMessageHandler(this->sessionMessageHandler);
                this->currentSession->start();
            
                std::cout << "[server][listening] successfully connect client: [" << this->currentSession->getClientInfo() << "]" << std::endl;
            
                if (this->serverConnectionCallback) 
                {
                    this->serverConnectionCallback(true, this->currentSession->getClientInfo());
                }
            
                this->sendToClient("[server] you have successfully connected.");
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
