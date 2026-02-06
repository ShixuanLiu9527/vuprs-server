#include "fpga_memory.h"

bool vuprs::FPGA_Memory__DDR::LoadFromJsonObj(const nlohmann::json &obj)
{
    this->LoadMainInfoFromJsonObj(obj);
    return true;
}

bool vuprs::FPGA_Memory__FIRBram::LoadFromJsonObj(const nlohmann::json &obj)
{
    this->LoadMainInfoFromJsonObj(obj);
    return true;
}

bool vuprs::FPGA_Memory__SGBram::LoadFromJsonObj(const nlohmann::json &obj)
{
    this->LoadMainInfoFromJsonObj(obj);
    return true;
}

bool vuprs::FPGA_Memory__CircularBufferBram::LoadFromJsonObj(const nlohmann::json &obj)
{
    this->LoadMainInfoFromJsonObj(obj);
    return true;
}
