#include "fpga_controller.h"

vuprs::FPGAController::FPGAController()
{
    this->ResetController();
}

vuprs::FPGAController::FPGAController(const std::string &configJsonFilename)
{
    this->ResetController();
    this->ConfigFPGAFromJson(configJsonFilename);
}

void vuprs::FPGAController::ResetController()
{
    std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */

    this->ioManagerList_dev.clear();
    this->ioManagerList_mem.clear();
    this->ioManagerList_irq.clear();

    this->ioManagerList_dev.reserve(FPGA_MODULE_COUNT * 2);
    this->ioManagerList_mem.reserve(FPGA_MODULE_COUNT * 2);
    this->ioManagerList_irq.reserve(FPGA_MODULE_COUNT);
    
    this->configdone = false;
}

vuprs::FPGAController::~FPGAController()
{
    std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */

    this->ioManagerList_dev.clear();
    this->ioManagerList_mem.clear();
    this->ioManagerList_irq.clear();
}

bool vuprs::FPGAController::ConfigFPGAFromJson(const std::string &configJsonFilename)
{
    std::ifstream configJsonFile;

    /* open config json file */

    configJsonFile.open(configJsonFilename);
    if (!configJsonFile.is_open())
    {
        throw std::runtime_error("Cannot open file: " + configJsonFilename);
    }

    nlohmann::json configJsonData;

    try
    {
        configJsonFile >> configJsonData;
    }
    catch(const std::exception &e)
    {
        throw std::runtime_error("Error occurred when parsing JSON file." + std::string(e.what()));
    }

    bool configStatus = true;

    try
    {
        auto devices = configJsonData["devices"];
        {
            std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */

            /* Load devices */

            configStatus &= this->dev__AXI_DMA.LoadFromJsonObj(devices["axi_dma"]);
            configStatus &= this->dev__ADC_Controller.LoadFromJsonObj(devices["adc_controller"]);
            configStatus &= this->dev__Circular_Buffer.LoadFromJsonObj(devices["circular_buffer"]);
            configStatus &= this->dev__FIR_Filter_Bank.LoadFromJsonObj(devices["fir_bank"]);
            configStatus &= this->dev__PreDelay_Unit.LoadFromJsonObj(devices["pre_delay_unit"]);

            /* Load memorys */

            configStatus &= this->mem__DDR.LoadFromJsonObj(devices["ddr"]);
            configStatus &= this->mem__FIR_BRAM.LoadFromJsonObj(devices["fir_bram"]);
            configStatus &= this->mem__SG_BRAM.LoadFromJsonObj(devices["sg_bram"]);
            configStatus &= this->mem__Circular_Buffer_BRAM.LoadFromJsonObj(devices["cbuf_bram"]);
        }
        configStatus &= this->BindIOManager();
    }
    catch(const std::exception& e)
    {
        throw std::runtime_error("Error occurred in parsing: " + std::string(e.what()));
    }

    if (!configStatus)
    {
        throw std::runtime_error("Config failed.");
    }

    this->configdone = true;
    return true;
}

void vuprs::FPGAController::GetOrCreateInterruptIOManagerIndex(const std::string &deviceFile, int *index)
{
    if (index == nullptr)
    {
        throw std::runtime_error("Index is NULL.");
    }

    int len;

    {
        std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */
        len = this->ioManagerList_irq.size();
    }

    *index = 0;
    {
        std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */
        
        for (int i = 0; i < len; i++)
        {
            if (this->ioManagerList_irq[i]->GetDeviceFilename() == deviceFile)
            {
                *index = i;
                return;
            }
        }
        this->ioManagerList_irq.emplace_back(std::make_shared<vuprs::FPGA_IOManagerForInterrput>(deviceFile));
        *index = this->ioManagerList_irq.size() - 1;
    }
}

void vuprs::FPGAController::GetOrCreateNormalIOManagerIndex(const std::string &deviceFile, int *index, bool isDevice)
{
    if (index == nullptr)
    {
        throw std::runtime_error("Index is NULL.");
    }

    int len;

    {
        std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */
        if (isDevice) 
        {
            len = this->ioManagerList_dev.size();
        }
        else 
        {
            len = this->ioManagerList_mem.size();
        }
    }

    *index = 0;

    {
        std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */

        for (int i = 0; i < len; i++)
        {
            if (isDevice)
            {
                if (this->ioManagerList_dev[i]->GetDeviceFilename() == deviceFile)
                {
                    *index = i;
                    return;
                }
            }
            else
            {
                if (this->ioManagerList_mem[i]->GetDeviceFilename() == deviceFile)
                {
                    *index = i;
                    return;
                }
            }
        }
        if (isDevice)
        {
            this->ioManagerList_dev.emplace_back(std::make_shared<vuprs::FPGA_IOManagerForDevice>(deviceFile));
            *index = this->ioManagerList_dev.size() - 1;
        }
        else
        {
            this->ioManagerList_mem.emplace_back(std::make_shared<vuprs::FPGA_IOManagerForMemory>(deviceFile));
            *index = this->ioManagerList_mem.size() - 1;
        }
    }
}

bool vuprs::FPGAController::BindIOManager()
{
    int i_AXI_DMA, i_ADC_Controller, i_Circular_Buffer, i_FIR_Filter_Bank, i_PreDelay_Unit;

    int i_h2c_DDR, i_c2h_DDR;
    int i_h2c_FIR_BRAM, i_c2h_FIR_BRAM;
    int i_h2c_SG_BRAM, i_c2h_SG_BRAM;
    int i_h2c_Circular_Buffer_BRAM, i_c2h_Circular_Buffer_BRAM;

    int i_AXI_DMA_irq;

    bool bindStatus = true;

    /* Generate devices */

    this->GetOrCreateNormalIOManagerIndex(this->dev__AXI_DMA.ControlDeviceFilename(), &i_AXI_DMA, true);
    this->GetOrCreateNormalIOManagerIndex(this->dev__ADC_Controller.ControlDeviceFilename(), &i_ADC_Controller, true);
    this->GetOrCreateNormalIOManagerIndex(this->dev__Circular_Buffer.ControlDeviceFilename(), &i_Circular_Buffer, true);
    this->GetOrCreateNormalIOManagerIndex(this->dev__FIR_Filter_Bank.ControlDeviceFilename(), &i_FIR_Filter_Bank, true);
    this->GetOrCreateNormalIOManagerIndex(this->dev__PreDelay_Unit.ControlDeviceFilename(), &i_PreDelay_Unit, true);

    this->GetOrCreateInterruptIOManagerIndex(this->dev__AXI_DMA.EventDeviceFilename(), &i_AXI_DMA_irq);

    /* Generate memories */

    this->GetOrCreateNormalIOManagerIndex(this->mem__DDR.H2C_ControlDeviceFilename(), &i_h2c_DDR, false);
    this->GetOrCreateNormalIOManagerIndex(this->mem__DDR.C2H_ControlDeviceFilename(), &i_c2h_DDR, false);

    this->GetOrCreateNormalIOManagerIndex(this->mem__FIR_BRAM.H2C_ControlDeviceFilename(), &i_h2c_FIR_BRAM, false);
    this->GetOrCreateNormalIOManagerIndex(this->mem__FIR_BRAM.C2H_ControlDeviceFilename(), &i_c2h_FIR_BRAM, false);

    this->GetOrCreateNormalIOManagerIndex(this->mem__SG_BRAM.H2C_ControlDeviceFilename(), &i_h2c_SG_BRAM, false);
    this->GetOrCreateNormalIOManagerIndex(this->mem__SG_BRAM.C2H_ControlDeviceFilename(), &i_c2h_SG_BRAM, false);

    this->GetOrCreateNormalIOManagerIndex(this->mem__Circular_Buffer_BRAM.H2C_ControlDeviceFilename(), &i_h2c_Circular_Buffer_BRAM, false);
    this->GetOrCreateNormalIOManagerIndex(this->mem__Circular_Buffer_BRAM.C2H_ControlDeviceFilename(), &i_c2h_Circular_Buffer_BRAM, false);

    {
        std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */

        /* Bind devices */

        bindStatus &= this->dev__AXI_DMA.BindFPGAFileManager(this->ioManagerList_dev[i_AXI_DMA]);
        bindStatus &= this->dev__ADC_Controller.BindFPGAFileManager(this->ioManagerList_dev[i_ADC_Controller]);
        bindStatus &= this->dev__Circular_Buffer.BindFPGAFileManager(this->ioManagerList_dev[i_Circular_Buffer]);
        bindStatus &= this->dev__FIR_Filter_Bank.BindFPGAFileManager(this->ioManagerList_dev[i_FIR_Filter_Bank]);
        bindStatus &= this->dev__PreDelay_Unit.BindFPGAFileManager(this->ioManagerList_dev[i_PreDelay_Unit]);

        bindStatus &= this->dev__AXI_DMA.BindFPGAFileManager_Interrupt(this->ioManagerList_irq[i_AXI_DMA_irq]);

        /* Bind memories */

        bindStatus &= this->mem__DDR.BindFPGAFileManager(this->ioManagerList_mem[i_h2c_DDR], this->ioManagerList_mem[i_c2h_DDR]);
        bindStatus &= this->mem__FIR_BRAM.BindFPGAFileManager(this->ioManagerList_mem[i_h2c_FIR_BRAM], this->ioManagerList_mem[i_c2h_FIR_BRAM]);
        bindStatus &= this->mem__SG_BRAM.BindFPGAFileManager(this->ioManagerList_mem[i_h2c_SG_BRAM], this->ioManagerList_mem[i_c2h_SG_BRAM]);
        bindStatus &= this->mem__Circular_Buffer_BRAM.BindFPGAFileManager(this->ioManagerList_mem[i_h2c_Circular_Buffer_BRAM], this->ioManagerList_mem[i_c2h_Circular_Buffer_BRAM]);
    }

    return bindStatus;
}

bool vuprs::FPGAController::ConfigDown() const
{
    return this->configdone;
}
