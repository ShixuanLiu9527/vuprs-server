#include "fpga_controller.h"

vuprs::FPGAController::FPGAController()
{
    this->ioManagerList.clear();
    this->ioManagerList.reserve(FPGA_MODULE_COUNT * 2);
    this->configdown = false;
}

vuprs::FPGAController::FPGAController(const std::string &configJsonFilename)
{
    this->ioManagerList.clear();
    this->ioManagerList.reserve(FPGA_MODULE_COUNT * 2);
    this->configdown = false;

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

    try
    {
        auto devices = configJsonData["devices"];

        /* Load devices */

        this->dev__AXI_DMA.LoadFromJsonObj(devices["axi_dma"]);
        this->dev__ADC_Controller.LoadFromJsonObj(devices["adc_controller"]);
        this->dev__Circular_Buffer.LoadFromJsonObj(devices["circular_buffer"]);
        this->dev__FIR_Filter_Bank.LoadFromJsonObj(devices["fir_bank"]);
        this->dev__PreDelay_Unit.LoadFromJsonObj(devices["pre_delay_unit"]);

        /* Load memorys */

        this->mem__DDR.LoadFromJsonObj(devices["ddr"]);
        this->mem__FIR_BRAM.LoadFromJsonObj(devices["fir_bram"]);
        this->mem__SG_BRAM.LoadFromJsonObj(devices["sg_bram"]);
        this->mem__Circular_Buffer_BRAM.LoadFromJsonObj(devices["cbuf_bram"]);

    }
    catch(const std::exception& e)
    {
        throw std::runtime_error("Error occurred in parsing: " + std::string(e.what()));
    }

    this->BindIOManager();

    this->configdown = true;
    return true;
}

bool vuprs::FPGAController::GetIOManagerIndex(const std::string &deviceFile, int *index)
{
    bool isNew = true;
    int len = this->ioManagerList.size();
    if (index == nullptr)
    {
        throw std::runtime_error("Index is NULL.");
    }
    *index = 0;
    for (int i = 0; i < len; i++)
    {
        if (this->ioManagerList[i].GetDeviceFilename() == deviceFile)
        {
            *index = i;
            isNew = false;
            break;
        }
    }
    if (isNew)
    {
        this->ioManagerList.push_back(vuprs::FPGA_IOManager(deviceFile));
        *index = this->ioManagerList.size() - 1;
        return true;
    }
    return false;
}

bool vuprs::FPGAController::BindIOManager()
{
    int i_AXI_DMA, i_ADC_Controller, i_Circular_Buffer, i_FIR_Filter_Bank, i_PreDelay_Unit;

    int i_h2c_DDR, i_c2h_DDR;
    int i_h2c_FIR_BRAM, i_c2h_FIR_BRAM;
    int i_h2c_SG_BRAM, i_c2h_SG_BRAM;
    int i_h2c_Circular_Buffer_BRAM, i_c2h_Circular_Buffer_BRAM;

    /* Generate */

    this->GetIOManagerIndex(this->dev__AXI_DMA.ControlDeviceFilename(), &i_AXI_DMA);
    this->GetIOManagerIndex(this->dev__ADC_Controller.ControlDeviceFilename(), &i_ADC_Controller);
    this->GetIOManagerIndex(this->dev__Circular_Buffer.ControlDeviceFilename(), &i_Circular_Buffer);
    this->GetIOManagerIndex(this->dev__FIR_Filter_Bank.ControlDeviceFilename(), &i_FIR_Filter_Bank);
    this->GetIOManagerIndex(this->dev__PreDelay_Unit.ControlDeviceFilename(), &i_PreDelay_Unit);

    this->GetIOManagerIndex(this->mem__DDR.H2C_ControlDeviceFilename(), &i_h2c_DDR);
    this->GetIOManagerIndex(this->mem__DDR.C2H_ControlDeviceFilename(), &i_c2h_DDR);

    this->GetIOManagerIndex(this->mem__FIR_BRAM.H2C_ControlDeviceFilename(), &i_h2c_FIR_BRAM);
    this->GetIOManagerIndex(this->mem__FIR_BRAM.C2H_ControlDeviceFilename(), &i_c2h_FIR_BRAM);

    this->GetIOManagerIndex(this->mem__SG_BRAM.H2C_ControlDeviceFilename(), &i_h2c_SG_BRAM);
    this->GetIOManagerIndex(this->mem__SG_BRAM.C2H_ControlDeviceFilename(), &i_c2h_SG_BRAM);

    this->GetIOManagerIndex(this->mem__Circular_Buffer_BRAM.H2C_ControlDeviceFilename(), &i_h2c_Circular_Buffer_BRAM);
    this->GetIOManagerIndex(this->mem__Circular_Buffer_BRAM.C2H_ControlDeviceFilename(), &i_c2h_Circular_Buffer_BRAM);

    /* Bind */

    this->dev__AXI_DMA.BindFPGAFileManager(&this->ioManagerList[i_AXI_DMA]);
    this->dev__ADC_Controller.BindFPGAFileManager(&this->ioManagerList[i_ADC_Controller]);
    this->dev__Circular_Buffer.BindFPGAFileManager(&this->ioManagerList[i_Circular_Buffer]);
    this->dev__FIR_Filter_Bank.BindFPGAFileManager(&this->ioManagerList[i_FIR_Filter_Bank]);
    this->dev__PreDelay_Unit.BindFPGAFileManager(&this->ioManagerList[i_PreDelay_Unit]);

    this->mem__DDR.BindFPGAFileManager(&this->ioManagerList[i_h2c_DDR], &this->ioManagerList[i_c2h_DDR]);
    this->mem__FIR_BRAM.BindFPGAFileManager(&this->ioManagerList[i_h2c_FIR_BRAM], &this->ioManagerList[i_c2h_FIR_BRAM]);
    this->mem__SG_BRAM.BindFPGAFileManager(&this->ioManagerList[i_h2c_SG_BRAM], &this->ioManagerList[i_c2h_SG_BRAM]);
    this->mem__Circular_Buffer_BRAM.BindFPGAFileManager(&this->ioManagerList[i_h2c_Circular_Buffer_BRAM], &this->ioManagerList[i_c2h_Circular_Buffer_BRAM]);

    return true;
}

bool vuprs::FPGAController::ConfigDown() const
{
    return this->configdown;
}
