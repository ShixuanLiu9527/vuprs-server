#include "fpga/fpga_controller.h"
#include "logger/log_manager.h"

vuprs::FPGAController::FPGAController()
{
    this->ResetController();
}

vuprs::FPGAController::FPGAController(const std::string &json_filename)
{
    this->ResetController();
    this->ConfigFPGAFromJson(json_filename);
}

void vuprs::FPGAController::ResetController()
{
    std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
    this->io_manager_list_dev.clear();
    this->io_manager_list_mem.clear();
    this->io_manager_list_irq.clear();
    this->io_manager_list_dev.reserve(FPGA_MODULE_COUNT * 2);
    this->io_manager_list_mem.reserve(FPGA_MODULE_COUNT * 2);
    this->io_manager_list_irq.reserve(FPGA_MODULE_COUNT);

    this->config_done = false;
}

vuprs::FPGAController::~FPGAController()
{
    std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
    this->io_manager_list_dev.clear();
    this->io_manager_list_mem.clear();
    this->io_manager_list_irq.clear();
}

bool vuprs::FPGAController::ConfigFPGAFromJson(const std::string &json_filename)
{
    std::ifstream f;

    /* open config json file */
    f.open(json_filename);
    RUNTIME_CHECK(f.is_open(), "fpga", " in [FPGAController::ConfigFPGAFromJson] Cannot open file: " + json_filename);
    nlohmann::json json_data;
    try
    {
        f >> json_data;
    }
    catch (const std::exception &e)
    {
        RUNTIME_CHECK(false, "fpga", " in [FPGAController::ConfigFPGAFromJson] Error occurred when parsing JSON file." + std::string(e.what()));
    }
    bool config_status = true;
    try
    {
        auto devices = json_data["devices"];
        {
            std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
            /* Load devices */
            config_status &= this->dev__axi_dma.LoadFromJsonObj(devices["axi_dma"]);
            config_status &= this->dev__adc_controller.LoadFromJsonObj(devices["adc_controller"]);
            config_status &= this->dev__circular_buffer.LoadFromJsonObj(devices["circular_buffer"]);
            config_status &= this->dev__fir_filter_bank.LoadFromJsonObj(devices["fir_bank"]);
            config_status &= this->dev__predelay_unit.LoadFromJsonObj(devices["pre_delay_unit"]);
            /* Load memorys */
            config_status &= this->mem__ddr.LoadFromJsonObj(devices["ddr"]);
            config_status &= this->mem__fir_bram.LoadFromJsonObj(devices["fir_bram"]);
            config_status &= this->mem__sg_bram.LoadFromJsonObj(devices["sg_bram"]);
            config_status &= this->mem___circular_buffer_bram.LoadFromJsonObj(devices["cbuf_bram"]);
        }
        config_status &= this->BindIOManager();
    }
    catch (const std::exception &e)
    {
        RUNTIME_CHECK(false, "fpga", " in [FPGAController::ConfigFPGAFromJson] Error occurred in parsing: " + std::string(e.what()));
    }
    RUNTIME_CHECK(config_status, "fpga", " in [FPGAController::ConfigFPGAFromJson] Config failed.");
    this->config_done = true;
    return true;
}

void vuprs::FPGAController::GetOrCreateInterruptIOManagerIndex(const std::string &device_file, int *index)
{
    PARAM_CHECK(index != nullptr, "fpga", " in [FPGAController::GetOrCreateInterruptIOManagerIndex] Index is NULL.");
    int len;
    {
        std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
        len = this->io_manager_list_irq.size();
    }
    *index = 0;
    {
        std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
        for (int i = 0; i < len; i++)
        {
            if (this->io_manager_list_irq[i]->GetDeviceFilename() == device_file)
            {
                *index = i;
                return;
            }
        }
        this->io_manager_list_irq.emplace_back(std::make_shared<vuprs::FPGA_IOManagerForInterrput>(device_file));
        *index = this->io_manager_list_irq.size() - 1;
    }
}

void vuprs::FPGAController::GetOrCreateNormalIOManagerIndex(const std::string &device_file, int *index, bool is_device)
{
    PARAM_CHECK(index != nullptr, "fpga", " in [FPGAController::GetOrCreateNormalIOManagerIndex] Index is NULL.");
    int len;
    {
        std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
        if (is_device)
        {
            len = this->io_manager_list_dev.size();
        }
        else
        {
            len = this->io_manager_list_mem.size();
        }
    }
    *index = 0;
    {
        std::unique_lock<std::mutex> lock(this->mut); /* LOCK */

        for (int i = 0; i < len; i++)
        {
            if (is_device)
            {
                if (this->io_manager_list_dev[i]->GetDeviceFilename() == device_file)
                {
                    *index = i;
                    return;
                }
            }
            else
            {
                if (this->io_manager_list_mem[i]->GetDeviceFilename() == device_file)
                {
                    *index = i;
                    return;
                }
            }
        }
        if (is_device)
        {
            this->io_manager_list_dev.emplace_back(std::make_shared<vuprs::FPGA_IOManagerForDevice>(device_file));
            *index = this->io_manager_list_dev.size() - 1;
        }
        else
        {
            this->io_manager_list_mem.emplace_back(std::make_shared<vuprs::FPGA_IOManagerForMemory>(device_file));
            *index = this->io_manager_list_mem.size() - 1;
        }
    }
}

bool vuprs::FPGAController::BindIOManager()
{
    int i_axi_dma, i_adc_controller, i_circular_buffer, i_fir_filter_bank, i_predelay_unit;
    int i_h2c_ddr, i_c2h_ddr;
    int i_h2c_fir_bram, i_c2h_fir_bram;
    int i_h2c_sg_bram, i_c2h_sg_bram;
    int i_h2c_circular_buffer_bram, i_c2h_circular_buffer_bram;
    int i_axi_dma_irq;
    bool bind_status = true;
    /* Generate devices */
    this->GetOrCreateNormalIOManagerIndex(this->dev__axi_dma.ControlDeviceFilename(), &i_axi_dma, true);
    this->GetOrCreateNormalIOManagerIndex(this->dev__adc_controller.ControlDeviceFilename(), &i_adc_controller, true);
    this->GetOrCreateNormalIOManagerIndex(this->dev__circular_buffer.ControlDeviceFilename(), &i_circular_buffer, true);
    this->GetOrCreateNormalIOManagerIndex(this->dev__fir_filter_bank.ControlDeviceFilename(), &i_fir_filter_bank, true);
    this->GetOrCreateNormalIOManagerIndex(this->dev__predelay_unit.ControlDeviceFilename(), &i_predelay_unit, true);
    this->GetOrCreateInterruptIOManagerIndex(this->dev__axi_dma.EventDeviceFilename(), &i_axi_dma_irq);
    /* Generate memories */
    this->GetOrCreateNormalIOManagerIndex(this->mem__ddr.H2C_ControlDeviceFilename(), &i_h2c_ddr, false);
    this->GetOrCreateNormalIOManagerIndex(this->mem__ddr.C2H_ControlDeviceFilename(), &i_c2h_ddr, false);
    this->GetOrCreateNormalIOManagerIndex(this->mem__fir_bram.H2C_ControlDeviceFilename(), &i_h2c_fir_bram, false);
    this->GetOrCreateNormalIOManagerIndex(this->mem__fir_bram.C2H_ControlDeviceFilename(), &i_c2h_fir_bram, false);
    this->GetOrCreateNormalIOManagerIndex(this->mem__sg_bram.H2C_ControlDeviceFilename(), &i_h2c_sg_bram, false);
    this->GetOrCreateNormalIOManagerIndex(this->mem__sg_bram.C2H_ControlDeviceFilename(), &i_c2h_sg_bram, false);
    this->GetOrCreateNormalIOManagerIndex(this->mem___circular_buffer_bram.H2C_ControlDeviceFilename(), &i_h2c_circular_buffer_bram, false);
    this->GetOrCreateNormalIOManagerIndex(this->mem___circular_buffer_bram.C2H_ControlDeviceFilename(), &i_c2h_circular_buffer_bram, false);
    {
        std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
        /* Bind devices */
        bind_status &= this->dev__axi_dma.BindFPGAFileManager(this->io_manager_list_dev[i_axi_dma]);
        bind_status &= this->dev__adc_controller.BindFPGAFileManager(this->io_manager_list_dev[i_adc_controller]);
        bind_status &= this->dev__circular_buffer.BindFPGAFileManager(this->io_manager_list_dev[i_circular_buffer]);
        bind_status &= this->dev__fir_filter_bank.BindFPGAFileManager(this->io_manager_list_dev[i_fir_filter_bank]);
        bind_status &= this->dev__predelay_unit.BindFPGAFileManager(this->io_manager_list_dev[i_predelay_unit]);

        bind_status &= this->dev__axi_dma.BindFPGAFileManager_Interrupt(this->io_manager_list_irq[i_axi_dma_irq]);
        /* Bind memories */
        bind_status &= this->mem__ddr.BindFPGAFileManager(this->io_manager_list_mem[i_h2c_ddr], this->io_manager_list_mem[i_c2h_ddr]);
        bind_status &= this->mem__fir_bram.BindFPGAFileManager(this->io_manager_list_mem[i_h2c_fir_bram], this->io_manager_list_mem[i_c2h_fir_bram]);
        bind_status &= this->mem__sg_bram.BindFPGAFileManager(this->io_manager_list_mem[i_h2c_sg_bram], this->io_manager_list_mem[i_c2h_sg_bram]);
        bind_status &= this->mem___circular_buffer_bram.BindFPGAFileManager(this->io_manager_list_mem[i_h2c_circular_buffer_bram], this->io_manager_list_mem[i_c2h_circular_buffer_bram]);
    }
    return bind_status;
}

bool vuprs::FPGAController::ConfigDown() const
{
    return this->config_done;
}
