#ifndef FPGA_MEMORY_H
#define FPGA_MEMORY_H

#include "fpga/fpga_module_template.h"

namespace vuprs
{
    class FPGA_Memory__DDR : public FPGAMemoryTemplate
    {
    public:
        bool LoadFromJsonObj(const nlohmann::json &obj) override;
    };

    class FPGA_Memory__FIRBram : public FPGAMemoryTemplate
    {
    public:
        bool LoadFromJsonObj(const nlohmann::json &obj) override;
    };

    class FPGA_Memory__SGBram : public FPGAMemoryTemplate
    {
    public:
        bool LoadFromJsonObj(const nlohmann::json &obj) override;
    };

    class FPGA_Memory__CircularBufferBram : public FPGAMemoryTemplate
    {
    public:
        bool LoadFromJsonObj(const nlohmann::json &obj) override;
    };
}

#endif
