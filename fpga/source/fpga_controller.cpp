#include "fpga_controller.h"

vuprs::FPGAController::FPGAController()
{
    this->ioManagerList.clear();
    this->ioManagerList.reserve(FPGA_MODULE_COUNT * 2);
    this->configdone = false;
}

vuprs::FPGAController::FPGAController(const std::string &configJsonFilename)
{
    this->ioManagerList.clear();
    this->ioManagerList.reserve(FPGA_MODULE_COUNT * 2);
    this->configdone = false;

    this->ConfigFPGAFromJson(configJsonFilename);
}

vuprs::FPGAController::~FPGAController()
{
    this->ioManagerList.clear();
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
    catch(const std::exception& e)
    {
        throw std::runtime_error("Error occurred in parsing: " + std::string(e.what()));
    }

    configStatus &= this->BindIOManager();

    if (!configStatus)
    {
        throw std::runtime_error("Config failed.");
    }

    this->configdone = true;
    return true;
}

void vuprs::FPGAController::GetOrCreateIOManagerIndex(const std::string &deviceFile, int *index)
{
    int len = this->ioManagerList.size();
    if (index == nullptr)
    {
        throw std::runtime_error("Index is NULL.");
    }
    *index = 0;
    for (int i = 0; i < len; i++)
    {
        if (this->ioManagerList[i]->GetDeviceFilename() == deviceFile)
        {
            *index = i;
            return;
        }
    }
    this->ioManagerList.emplace_back(std::make_shared<vuprs::FPGA_IOManager>(deviceFile));
    *index = this->ioManagerList.size() - 1;
}

bool vuprs::FPGAController::BindIOManager()
{
    int i_AXI_DMA, i_ADC_Controller, i_Circular_Buffer, i_FIR_Filter_Bank, i_PreDelay_Unit;

    int i_h2c_DDR, i_c2h_DDR;
    int i_h2c_FIR_BRAM, i_c2h_FIR_BRAM;
    int i_h2c_SG_BRAM, i_c2h_SG_BRAM;
    int i_h2c_Circular_Buffer_BRAM, i_c2h_Circular_Buffer_BRAM;

    bool bindStatus = true;

    /* Generate */

    this->GetOrCreateIOManagerIndex(this->dev__AXI_DMA.ControlDeviceFilename(), &i_AXI_DMA);
    this->GetOrCreateIOManagerIndex(this->dev__ADC_Controller.ControlDeviceFilename(), &i_ADC_Controller);
    this->GetOrCreateIOManagerIndex(this->dev__Circular_Buffer.ControlDeviceFilename(), &i_Circular_Buffer);
    this->GetOrCreateIOManagerIndex(this->dev__FIR_Filter_Bank.ControlDeviceFilename(), &i_FIR_Filter_Bank);
    this->GetOrCreateIOManagerIndex(this->dev__PreDelay_Unit.ControlDeviceFilename(), &i_PreDelay_Unit);

    this->GetOrCreateIOManagerIndex(this->mem__DDR.H2C_ControlDeviceFilename(), &i_h2c_DDR);
    this->GetOrCreateIOManagerIndex(this->mem__DDR.C2H_ControlDeviceFilename(), &i_c2h_DDR);

    this->GetOrCreateIOManagerIndex(this->mem__FIR_BRAM.H2C_ControlDeviceFilename(), &i_h2c_FIR_BRAM);
    this->GetOrCreateIOManagerIndex(this->mem__FIR_BRAM.C2H_ControlDeviceFilename(), &i_c2h_FIR_BRAM);

    this->GetOrCreateIOManagerIndex(this->mem__SG_BRAM.H2C_ControlDeviceFilename(), &i_h2c_SG_BRAM);
    this->GetOrCreateIOManagerIndex(this->mem__SG_BRAM.C2H_ControlDeviceFilename(), &i_c2h_SG_BRAM);

    this->GetOrCreateIOManagerIndex(this->mem__Circular_Buffer_BRAM.H2C_ControlDeviceFilename(), &i_h2c_Circular_Buffer_BRAM);
    this->GetOrCreateIOManagerIndex(this->mem__Circular_Buffer_BRAM.C2H_ControlDeviceFilename(), &i_c2h_Circular_Buffer_BRAM);

    /* Bind */

    bindStatus &= this->dev__AXI_DMA.BindFPGAFileManager(this->ioManagerList[i_AXI_DMA]);
    bindStatus &= this->dev__ADC_Controller.BindFPGAFileManager(this->ioManagerList[i_ADC_Controller]);
    bindStatus &= this->dev__Circular_Buffer.BindFPGAFileManager(this->ioManagerList[i_Circular_Buffer]);
    bindStatus &= this->dev__FIR_Filter_Bank.BindFPGAFileManager(this->ioManagerList[i_FIR_Filter_Bank]);
    bindStatus &= this->dev__PreDelay_Unit.BindFPGAFileManager(this->ioManagerList[i_PreDelay_Unit]);

    bindStatus &= this->mem__DDR.BindFPGAFileManager(this->ioManagerList[i_h2c_DDR], this->ioManagerList[i_c2h_DDR]);
    bindStatus &= this->mem__FIR_BRAM.BindFPGAFileManager(this->ioManagerList[i_h2c_FIR_BRAM], this->ioManagerList[i_c2h_FIR_BRAM]);
    bindStatus &= this->mem__SG_BRAM.BindFPGAFileManager(this->ioManagerList[i_h2c_SG_BRAM], this->ioManagerList[i_c2h_SG_BRAM]);
    bindStatus &= this->mem__Circular_Buffer_BRAM.BindFPGAFileManager(this->ioManagerList[i_h2c_Circular_Buffer_BRAM], this->ioManagerList[i_c2h_Circular_Buffer_BRAM]);

    return bindStatus;
}

bool vuprs::FPGAController::ConfigDown() const
{
    return this->configdone;
}
