#include <iostream>

#include "fpga_config.h"
#include "fpga_control.h"
#include "aligned_data_structure.h"

#define FPGA_TOOL__OPERATE__READ_AXI_LITE           0U
#define FPGA_TOOL__OPERATE__WRITE_AXI_LITE          1U

#define FPGA_TOOL__OPERATE__READ_AXI_FULL_BUFFER    2U
#define FPGA_TOOL__OPERATE__WRITE_AXI_FULL_BUFFER   3U

#define FPGA_TOOL__OPERATE__READ_AXI_FULL_REG       4U
#define FPGA_TOOL__OPERATE__WRITE_AXI_FULL_REG      5U

#define FPGA_TOOL__OPERATE__READ_CONTROL            6U
#define FPGA_TOOL__OPERATE__WRITE_CONTROL           7U

#define FPGA_TOOL__OPERATE__ERROR                   8U

#define FPGA_TOOL__OPERATE__FOR_HELP                9U

/* Check command */

#define IS__FPGA_TOOL__OPERATE__READ_AXI_LITE_CMD(STR_LIST) \
(STR_LIST[1] == "--RW" && STR_LIST[3] == "--BUS" && \
 STR_LIST[5] == "--CFG" && STR_LIST[7] == "--BASE" && \
 STR_LIST[9] == "--OFFSET")

#define IS__FPGA_TOOL__OPERATE__WRITE_AXI_LITE_CMD(STR_LIST) \
(STR_LIST[1] == "--RW" && STR_LIST[3] == "--BUS" && \
 STR_LIST[5] == "--CFG" && STR_LIST[7] == "--BASE" && \
 STR_LIST[9] == "--OFFSET" && STR_LIST[11] == "--IO")

#define IS__FPGA_TOOL__OPERATE__READ_AXI_FULL_CMD(STR_LIST) \
(STR_LIST[1] == "--RW" && STR_LIST[3] == "--BUS" && \
 STR_LIST[5] == "--CFG" && STR_LIST[7] == "--OFFSET" && \
 STR_LIST[9] == "--BYTES" && STR_LIST[11] == "--IO")

#define IS__FPGA_TOOL__OPERATE__WRITE_AXI_FULL_CMD(STR_LIST) \
(IS__FPGA_TOOL__OPERATE__READ_AXI_FULL_CMD(STR_LIST))

#define IS__FPGA_TOOL__OPERATE__DEFAULT_READ_AXI_LITE_CMD(STR_LIST) \
(STR_LIST[1] == "--DEF-OPT" && STR_LIST[3] == "--BASE" && STR_LIST[5] == "--OFFSET")
#define IS__FPGA_TOOL__OPERATE__DEFAULT_WRITE_AXI_LITE_CMD(STR_LIST) \
(STR_LIST[1] == "--DEF-OPT" && STR_LIST[3] == "--BASE" && STR_LIST[5] == "--OFFSET" && STR_LIST[7] == "--IO")

#define IS__FPGA_TOOL__OPERATE__DEFAULT_READ_AXI_FULL_BUFFER_CMD(STR_LIST) \
(STR_LIST[1] == "--DEF-OPT" && STR_LIST[3] == "--OFFSET" && STR_LIST[5] == "--BYTES" && STR_LIST[7] == "--IO")
#define IS__FPGA_TOOL__OPERATE__DEFAULT_WRITE_AXI_FULL_BUFFER_CMD(STR_LIST) \
(IS__FPGA_TOOL__OPERATE__DEFAULT_READ_AXI_FULL_BUFFER_CMD(STR_LIST))

#define IS__FPGA_TOOL__OPERATE__DEFAULT_READ_AXI_FULL_REG_CMD(STR_LIST) \
(STR_LIST[1] == "--DEF-OPT" && STR_LIST[3] == "--OFFSET")
#define IS__FPGA_TOOL__OPERATE__DEFAULT_WRITE_AXI_FULL_REG_CMD(STR_LIST) \
(STR_LIST[1] == "--DEF-OPT" && STR_LIST[3] == "--OFFSET" && STR_LIST[5] == "--IO")

#define IS__FPGA_TOOL__OPERATE__DEFAULT_READ_CONTROL_REG_CMD(STR_LIST) \
(STR_LIST[1] == "--DEF-OPT" && STR_LIST[3] == "--OFFSET")
#define IS__FPGA_TOOL__OPERATE__DEFAULT_WRITE_CONTROL_REG_CMD(STR_LIST) \
(STR_LIST[1] == "--DEF-OPT" && STR_LIST[3] == "--OFFSET" && STR_LIST[5] == "--IO")

/* Parse operation */

#define IS__FPGA_TOOL__OPERATE__READ_AXI_LITE(STR_LIST) \
(STR_LIST[2] == "R" && STR_LIST[4] == "LITE")
#define IS__FPGA_TOOL__OPERATE__WRITE_AXI_LITE(STR_LIST) \
(STR_LIST[2] == "W" && STR_LIST[4] == "LITE")

#define IS__FPGA_TOOL__OPERATE__READ_AXI_FULL(STR_LIST) \
(STR_LIST[2] == "R" && STR_LIST[4] == "FULL")
#define IS__FPGA_TOOL__OPERATE__WRITE_AXI_FULL(STR_LIST) \
(STR_LIST[2] == "W" && STR_LIST[4] == "FULL")

#define IS__FPGA_TOOL__OPERATE__DEFAULT_READ_AXI_LITE(STR_LIST) \
(STR_LIST[2] == "R-LITE")
#define IS__FPGA_TOOL__OPERATE__DEFAULT_WRITE_AXI_LITE(STR_LIST) \
(STR_LIST[2] == "W-LITE")

#define IS__FPGA_TOOL__OPERATE__DEFAULT_READ_AXI_FULL_BUFFER(STR_LIST) \
(STR_LIST[2] == "R-FULL-BUF")
#define IS__FPGA_TOOL__OPERATE__DEFAULT_WRITE_AXI_FULL_BUFFER(STR_LIST) \
(STR_LIST[2] == "W-FULL-BUF")

#define IS__FPGA_TOOL__OPERATE__DEFAULT_READ_AXI_FULL_REG(STR_LIST) \
(STR_LIST[2] == "R-FULL-REG")
#define IS__FPGA_TOOL__OPERATE__DEFAULT_WRITE_AXI_FULL_REG(STR_LIST) \
(STR_LIST[2] == "W-FULL-REG")

#define IS__FPGA_TOOL__OPERATE__DEFAULT_READ_CONTROL_REG(STR_LIST) \
(STR_LIST[2] == "R-CTRL")
#define IS__FPGA_TOOL__OPERATE__DEFAULT_WRITE_CONTROL_REG(STR_LIST) \
(STR_LIST[2] == "W-CTRL")

/* Config file */

#define FPGA_TOOL__DEFAULT_CONFIG_JSON_FILENAME     "./fpga_config.json"

struct FPGA_TOOL_AXIParameters
{
    uint8_t operate;    /* Operate selection */
    
    uint64_t base;      /* Memory space base address (AXI-Lite only, AXI-Full = 0) */
    uint64_t offset;    /* Register offset address (AXI-Lite) & DDR offset (AXI-Full) */
    uint32_t writeValue;  /* Value to write (AXI-Lite only) */
    uint64_t transferBytes;  /* Transfer byte size (AXI-Full only) */

    std::string configFileName;  /* Config JSON file name */
    std::string datafileName;  /* Source data file (AXI-Full only) */
};

void FPGA_TOOL__PrintHelp();
FPGA_TOOL_AXIParameters FPGA_TOOL__ParseCommandParameters(const std::vector<std::string> &cmdList);

void FPGA_TOOL__PrintHelp()
{
printf("\n");
printf(" |========================= [ FPGA TOOL HELP ] ==========================|\n");
printf(" |                                                                       |\n");
printf(" | ----- [ 1. For Help ] ----------------------------------------------- |\n");
printf(" |                                                                       |\n");
printf(" | [ \033[92mCOMMAND\033[0m ]                                                           |\n");
printf(" |                                                                       |\n");
printf(" | fpga-tool -h                                                          |\n");
printf(" | fpga-tool --help                                                      |\n");
printf(" |                                                                       |\n");
printf(" | ----- [ 2. Access AXI-Lite & AXI-Full Bus ] ------------------------- |\n");
printf(" |                                                                       |\n");
printf(" | [ \033[92mCOMMAND\033[0m ]                                                           |\n");
printf(" |                                                                       |\n");
printf(" |  \033[33mCommand Method 1\033[0m:                                                    |\n");
printf(" |                                                                       |\n");
printf(" | fpga_tool --rw <rw> --bus <b> --cfg <cfg> --base <ba>                 |\n");
printf(" |           --offset <of> --bytes <by> --io <io>                        |\n");
printf(" |                                                                       |\n");
printf(" |  \033[33mCommand Method 2 (use default parameters)\033[0m:                           |\n");
printf(" |                                                                       |\n");
printf(" | fpga_tool --def-opt <opt> --base <b> --offset <of> --bytes <by>       |\n");
printf(" |           --io <io>                                                   |\n");
printf(" |                                                                       |\n");
printf(" | [ \033[92mPARAMETERS\033[0m ]                                                        |\n");
printf(" |                                                                       |\n");
printf(" | <opt> read/write & bus option.                                        |\n");
printf(" |       r-lite = read AXI-Lite;                                         |\n");
printf(" |       w-lite = write AXI-Lite;                                        |\n");
printf(" |       r-full-reg = read AXI-Full (memory map method);                 |\n");
printf(" |       w-full-reg = write AXI-Full (memory map method);                |\n");
printf(" |       r-full-buf = read AXI-Full (DMA method);                        |\n");
printf(" |       w-full-buf = write AXI-Full (DMA method);                       |\n");
printf(" |       r-ctrl = read XDMA control register;                            |\n");
printf(" |       w-ctrl = write XDMA control register;                           |\n");
printf(" |       in this method, config json file = ./fpga_config.json;          |\n");
printf(" | <rw> r = read from FPGA, w = write to FPGA;                           |\n");
printf(" | <b>  bus selection, lite = AXI-Lite, full = AXI-Full;                 |\n");
printf(" | <cf> config JSON file;                                                |\n");
printf(" | <ba> base address of the address space (AXI-Lite only, AXI-Full = 0); |\n");
printf(" | <of> register offset (AXI-Lite) or address offset (AXI-Full);         |\n");
printf(" | <by> read/write bytes (AXI-Full only, AXI-Lite = 4);                  |\n");
printf(" | <io> input value (AXI-Lite) or intput/output filename (AXI-Full);     |\n");
printf(" |                                                                       |\n");
printf(" | [ \033[92mEXAMPLE\033[0m ]                                                           |\n");
printf(" |                                                                       |\n");
printf(" | fpga_tool --rw \033[33mr\033[0m --bus \033[33mlite\033[0m --cfg \033[33m./cfg.json\033[0m --base \033[33m0\033[0m --offset \033[33m0x04\033[0m   |\n");
printf(" |                                                                       |\n");
printf(" | fpga_tool --rw \033[33mw\033[0m --bus \033[33mlite\033[0m --cfg \033[33m./cfg.json\033[0m --base \033[33m0\033[0m --offset \033[33m0x0C\033[0m   |\n");
printf(" |           --io \033[33m0XFF\033[0m                                                   |\n");
printf(" |                                                                       |\n");
printf(" | fpga_tool --rw \033[33mr\033[0m --bus \033[33mfull\033[0m --cfg \033[33m./cfg.json\033[0m --offset \033[33m0\033[0m --bytes \033[33m1024\033[0m  |\n");
printf(" |           --io \033[33m./r_data.bin\033[0m                                           |\n");
printf(" |                                                                       |\n");
printf(" | fpga_tool --rw \033[33mw\033[0m --bus \033[33mfull\033[0m --cfg \033[33m./cfg.json\033[0m --offset \033[33m0\033[0m --bytes \033[33m1024\033[0m  |\n");
printf(" |           --io \033[33m./w_data.bin\033[0m                                           |\n");
printf(" |                                                                       |\n");
printf(" | fpga_tool --def-opt \033[33mr-lite\033[0m --base \033[33m0\033[0m --offset \033[33m0x04\033[0m                     |\n");
printf(" | fpga_tool --def-opt \033[33mw-lite\033[0m --base \033[33m0\033[0m --offset \033[33m0x0C\033[0m --io \033[33m0x12\033[0m           |\n");
printf(" |                                                                       |\n");
printf(" | fpga_tool --def-opt \033[33mr-full-buf\033[0m --offset \033[33m0\033[0m --bytes \033[33m65536\033[0m --io \033[33m./r.bin\033[0m  |\n");
printf(" | fpga_tool --def-opt \033[33mw-full-buf\033[0m --offset \033[33m0\033[0m --bytes \033[33m2048\033[0m --io \033[33m./w.bin\033[0m   |\n");
printf(" |                                                                       |\n");
printf(" | fpga_tool --def-opt \033[33mr-full-reg\033[0m --offset \033[33m0\033[0m                             |\n");
printf(" | fpga_tool --def-opt \033[33mw-full-reg\033[0m --offset \033[33m0\033[0m --io \033[33m0x1234\033[0m                 |\n");
printf(" |                                                                       |\n");
printf(" | fpga_tool --def-opt \033[33mr-ctrl\033[0m --offset \033[33m0\033[0m                                 |\n");
printf(" | fpga_tool --def-opt \033[33mw-ctrl\033[0m --offset \033[33m0\033[0m --io \033[33m0x1234\033[0m                     |\n");
printf(" |                                                                       |\n");
printf(" |=======================================================================|\n");
printf("\n");
}

FPGA_TOOL_AXIParameters FPGA_TOOL__ParseCommandParameters(const std::vector<std::string> &cmdList)
{
    FPGA_TOOL_AXIParameters retParameters;
    std::vector<std::string> cmdListUpper = cmdList;
    bool parseStatus = false, cmdError = false;
    uint64_t cmdSize = cmdList.size(), parseValue;

    /* String to upper */

    printf(" Processing commands: \n");

    for (uint64_t i = 0; i < cmdSize; i++)
    {
        std::transform(cmdListUpper[i].begin(), cmdListUpper[i].end(), cmdListUpper[i].begin(), ::toupper);  /* Upper */
        std::cout << cmdListUpper[i] << " ";
    }

    std::cout << std::endl;

    /* Check --rw & --bus */

    if (cmdSize == 2)
    {
        if (cmdListUpper[1] == "-H" || cmdListUpper[1] == "--HELP")
        {
            retParameters.operate = FPGA_TOOL__OPERATE__FOR_HELP;
        }
        else
        {
            cmdError = true;
        }
    }
    else if (cmdSize == 5)
    {
        if ((IS__FPGA_TOOL__OPERATE__DEFAULT_READ_AXI_FULL_REG_CMD(cmdListUpper) && IS__FPGA_TOOL__OPERATE__DEFAULT_READ_AXI_FULL_REG(cmdListUpper)))
        {
            retParameters.operate = FPGA_TOOL__OPERATE__READ_AXI_FULL_REG;

            /* Parse user value */

            retParameters.configFileName = FPGA_TOOL__DEFAULT_CONFIG_JSON_FILENAME;

            parseValue = vuprs::ParseNumberFromString(cmdList[4], &parseStatus);
            if (parseStatus)retParameters.offset = parseValue;
            else cmdError = true;
        }
        else if (IS__FPGA_TOOL__OPERATE__DEFAULT_READ_CONTROL_REG_CMD(cmdListUpper) && IS__FPGA_TOOL__OPERATE__DEFAULT_READ_CONTROL_REG(cmdListUpper))
        {
            retParameters.operate = FPGA_TOOL__OPERATE__READ_CONTROL;

            /* Parse user value */

            retParameters.configFileName = FPGA_TOOL__DEFAULT_CONFIG_JSON_FILENAME;

            parseValue = vuprs::ParseNumberFromString(cmdList[4], &parseStatus);
            if (parseStatus)retParameters.offset = parseValue;
            else cmdError = true;
        }
        else
        {
            cmdError = true;
        }
    }
    else if (cmdSize == 7)  /* use default parameters */
    {
        if (IS__FPGA_TOOL__OPERATE__DEFAULT_READ_AXI_LITE_CMD(cmdListUpper) && IS__FPGA_TOOL__OPERATE__DEFAULT_READ_AXI_LITE(cmdListUpper))
        {
            retParameters.operate = FPGA_TOOL__OPERATE__READ_AXI_LITE;

            /* Parse user value */

            retParameters.configFileName = FPGA_TOOL__DEFAULT_CONFIG_JSON_FILENAME;

            parseValue = vuprs::ParseNumberFromString(cmdList[4], &parseStatus);
            if (parseStatus)retParameters.base = parseValue;
            else cmdError = true;

            parseValue = vuprs::ParseNumberFromString(cmdList[6], &parseStatus);
            if (parseStatus)retParameters.offset = parseValue;
            else cmdError = true;
        }
        else if ((IS__FPGA_TOOL__OPERATE__DEFAULT_WRITE_AXI_FULL_REG_CMD(cmdListUpper) && IS__FPGA_TOOL__OPERATE__DEFAULT_WRITE_AXI_FULL_REG(cmdListUpper)))
        {
            retParameters.operate = FPGA_TOOL__OPERATE__WRITE_AXI_FULL_REG;

            /* Parse user value */

            retParameters.configFileName = FPGA_TOOL__DEFAULT_CONFIG_JSON_FILENAME;

            parseValue = vuprs::ParseNumberFromString(cmdList[4], &parseStatus);
            if (parseStatus)retParameters.offset = parseValue;
            else cmdError = true;

            parseValue = vuprs::ParseNumberFromString(cmdList[6], &parseStatus);
            if (parseStatus)retParameters.writeValue = static_cast<uint32_t>(parseValue);
            else cmdError = true;
        }
        else if (IS__FPGA_TOOL__OPERATE__DEFAULT_WRITE_CONTROL_REG_CMD(cmdListUpper) && IS__FPGA_TOOL__OPERATE__DEFAULT_WRITE_CONTROL_REG(cmdListUpper))
        {
            retParameters.operate = FPGA_TOOL__OPERATE__WRITE_CONTROL;

            /* Parse user value */

            retParameters.configFileName = FPGA_TOOL__DEFAULT_CONFIG_JSON_FILENAME;

            parseValue = vuprs::ParseNumberFromString(cmdList[4], &parseStatus);
            if (parseStatus)retParameters.offset = parseValue;
            else cmdError = true;

            parseValue = vuprs::ParseNumberFromString(cmdList[6], &parseStatus);
            if (parseStatus)retParameters.writeValue = static_cast<uint32_t>(parseValue);
            else cmdError = true;
        }
        else
        {
            cmdError = true;
        }
    }
    else if (cmdSize == 9)
    {
        if (IS__FPGA_TOOL__OPERATE__DEFAULT_WRITE_AXI_LITE_CMD(cmdListUpper) && IS__FPGA_TOOL__OPERATE__DEFAULT_WRITE_AXI_LITE(cmdListUpper))
        {
            retParameters.operate = FPGA_TOOL__OPERATE__WRITE_AXI_LITE;

            /* Parse user value */

            retParameters.configFileName = FPGA_TOOL__DEFAULT_CONFIG_JSON_FILENAME;

            parseValue = vuprs::ParseNumberFromString(cmdList[4], &parseStatus);
            if (parseStatus)retParameters.base = parseValue;
            else cmdError = true;

            parseValue = vuprs::ParseNumberFromString(cmdList[6], &parseStatus);
            if (parseStatus)retParameters.offset = parseValue;
            else cmdError = true;

            parseValue = vuprs::ParseNumberFromString(cmdList[8], &parseStatus);
            if (parseStatus)retParameters.writeValue = static_cast<uint32_t>(parseValue);
            else cmdError = true;
        }

        /* AXI-Full buffer transfer */

        else if ((IS__FPGA_TOOL__OPERATE__DEFAULT_READ_AXI_FULL_BUFFER_CMD(cmdListUpper) && IS__FPGA_TOOL__OPERATE__DEFAULT_READ_AXI_FULL_BUFFER(cmdListUpper)) || 
                 (IS__FPGA_TOOL__OPERATE__DEFAULT_WRITE_AXI_FULL_BUFFER_CMD(cmdListUpper) && IS__FPGA_TOOL__OPERATE__DEFAULT_WRITE_AXI_FULL_BUFFER(cmdListUpper)))
        {
            if (IS__FPGA_TOOL__OPERATE__DEFAULT_READ_AXI_FULL_BUFFER_CMD(cmdListUpper) && IS__FPGA_TOOL__OPERATE__DEFAULT_READ_AXI_FULL_BUFFER(cmdListUpper))
            {
                retParameters.operate = FPGA_TOOL__OPERATE__READ_AXI_FULL_BUFFER;
            }
            else
            {
                retParameters.operate = FPGA_TOOL__OPERATE__WRITE_AXI_FULL_BUFFER;
            }

            /* Parse user value */

            retParameters.configFileName = FPGA_TOOL__DEFAULT_CONFIG_JSON_FILENAME;

            parseValue = vuprs::ParseNumberFromString(cmdList[4], &parseStatus);
            if (parseStatus)retParameters.offset = parseValue;
            else cmdError = true;
           
            parseValue = vuprs::ParseNumberFromString(cmdList[6], &parseStatus);
            if (parseStatus)retParameters.transferBytes = parseValue;
            else cmdError = true;

            if (!cmdList[8].empty())retParameters.datafileName = cmdList[8];
            else cmdError = true;
        }
        else
        {
            cmdError = true;
        }
        
    }
    else if (cmdSize == 11)
    {
        if (IS__FPGA_TOOL__OPERATE__READ_AXI_LITE_CMD(cmdListUpper) && IS__FPGA_TOOL__OPERATE__READ_AXI_LITE(cmdListUpper))
        {
            retParameters.operate = FPGA_TOOL__OPERATE__READ_AXI_LITE;

            /* Parse user value */

            if (!cmdList[6].empty())retParameters.configFileName = cmdList[6];
            else cmdError = true;

            parseValue = vuprs::ParseNumberFromString(cmdList[8], &parseStatus);
            if (parseStatus)retParameters.base = parseValue;
            else cmdError = true;

            parseValue = vuprs::ParseNumberFromString(cmdList[10], &parseStatus);
            if (parseStatus)retParameters.offset = parseValue;
            else cmdError = true;
        }
        else
        {
            cmdError = true;
        }
    }
    else if (cmdSize == 13)
    {
        if (IS__FPGA_TOOL__OPERATE__WRITE_AXI_LITE_CMD(cmdListUpper) && IS__FPGA_TOOL__OPERATE__WRITE_AXI_LITE(cmdListUpper))
        {
            retParameters.operate = FPGA_TOOL__OPERATE__WRITE_AXI_LITE;

            /* Parse user value */

            if (!cmdList[6].empty())retParameters.configFileName = cmdList[6];
            else cmdError = true;

            parseValue = vuprs::ParseNumberFromString(cmdList[8], &parseStatus);
            if (parseStatus)retParameters.base = parseValue;
            else cmdError = true;
           
            parseValue = vuprs::ParseNumberFromString(cmdList[10], &parseStatus);
            if (parseStatus)retParameters.offset = parseValue;
            else cmdError = true;

            parseValue = vuprs::ParseNumberFromString(cmdList[12], &parseStatus);
            if (parseStatus)retParameters.writeValue = static_cast<uint32_t>(parseValue);
            else cmdError = true;
        }
        else if ((IS__FPGA_TOOL__OPERATE__READ_AXI_FULL_CMD(cmdListUpper) && IS__FPGA_TOOL__OPERATE__READ_AXI_FULL(cmdListUpper)) || 
                 (IS__FPGA_TOOL__OPERATE__WRITE_AXI_FULL_CMD(cmdListUpper) && IS__FPGA_TOOL__OPERATE__WRITE_AXI_FULL(cmdListUpper)))
        {
            if (IS__FPGA_TOOL__OPERATE__READ_AXI_FULL_CMD(cmdListUpper) && IS__FPGA_TOOL__OPERATE__READ_AXI_FULL(cmdListUpper))
            {
                retParameters.operate = FPGA_TOOL__OPERATE__READ_AXI_FULL_BUFFER;
            }
            else
            {
                retParameters.operate = FPGA_TOOL__OPERATE__WRITE_AXI_FULL_BUFFER;
            }

            /* Parse user value */

            if (!cmdList[6].empty())retParameters.configFileName = cmdList[6];
            else cmdError = true;

            parseValue = vuprs::ParseNumberFromString(cmdList[8], &parseStatus);
            if (parseStatus)retParameters.offset = parseValue;
            else cmdError = true;
           
            parseValue = vuprs::ParseNumberFromString(cmdList[10], &parseStatus);
            if (parseStatus)retParameters.transferBytes = parseValue;
            else cmdError = true;

            if (!cmdList[12].empty())retParameters.datafileName = cmdList[12];
            else cmdError = true;
        }
        else
        {
            cmdError = true;
        }
    }
    else
    {
        cmdError = true;
    }

    if (cmdError)
    {
        retParameters.operate = FPGA_TOOL__OPERATE__ERROR;
    }

    return retParameters;
}

int main(int argc, char *argv[])
{

    std::vector<std::string> args;
    FPGA_TOOL_AXIParameters fpgaConfigParam;

    vuprs::FPGAConfigManager fpgaConfigManager;
    vuprs::FPGAController fpgaController;
    vuprs::AlignedBufferDMA buffer;
    vuprs::DMATransferConfig dmaTransferConfig;

    uint32_t rValue;

    args.resize(argc);

    for (int i = 0; i < argc; i++)
    {
        args[i] = std::string(argv[i]);
    }

    fpgaConfigParam = FPGA_TOOL__ParseCommandParameters(args);

    if (fpgaConfigParam.operate != FPGA_TOOL__OPERATE__ERROR && fpgaConfigParam.operate != FPGA_TOOL__OPERATE__FOR_HELP)
    {
        if (!fpgaConfigManager.LoadFPGAConfigFromJson(fpgaConfigParam.configFileName))
        {
std::cout << " \033[31mFPGA-TOOL ERR: Cannot load config data from: " << fpgaConfigParam.configFileName << "\033[0m" << std::endl;
            buffer.release();
            return 0;
        }
        else
        {
            if(!fpgaController.LoadFPGAConfig(fpgaConfigManager))
            {
printf(" \033[31mFPGA-TOOL ERR: Error occurred when loading config.\033[0m\n");
                buffer.release();
                return 0;
            }
        }

std::cout << " \033[92mSuccessfully load configuration from\033[0m: \033[34m" << fpgaConfigParam.configFileName << "\033[0m" << std::endl;
    }

    switch (fpgaConfigParam.operate)
    {
        case FPGA_TOOL__OPERATE__FOR_HELP:
        {
            FPGA_TOOL__PrintHelp();
            break;
        }
        case FPGA_TOOL__OPERATE__ERROR: 
        {
printf(" \033[31mFPGA-TOOL: ERROR COMMAND!\033[0m Check the command below:  \n");
            FPGA_TOOL__PrintHelp();
            break;
        }

        /* Read AXI-Lite bus */

        case FPGA_TOOL__OPERATE__READ_AXI_LITE:
        {
            try
            {
                if(fpgaController.AXILite_Read(fpgaConfigParam.base, fpgaConfigParam.offset, &rValue))
                {
printf(" | --------------------------------------------------------------------- |\n");
printf("                           [\033[92m READ AXI-LITE SUCCESS \033[0m]\n");
printf("\n");
printf("   <address>    \033[33m0x%X\033[0m\n", fpgaConfigParam.base + fpgaConfigParam.offset);
printf("   <value>      \033[33m0x%X\033[0m\n", rValue);
printf("\n");
printf(" | --------------------------------------------------------------------- |\n");
                }
                else
                {
printf(" | --------------------------------------------------------------------- |\n");
printf("                           [\033[31m READ AXI-LITE FAILED \033[0m]\n");
printf(" | --------------------------------------------------------------------- |\n");
                }
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
                buffer.release();
                return 0;
            }
            break;
        }

        /* Write AXI-Lite bus */

        case FPGA_TOOL__OPERATE__WRITE_AXI_LITE:
        {
            try
            {
                if(fpgaController.AXILite_Write(fpgaConfigParam.base, fpgaConfigParam.offset, fpgaConfigParam.writeValue))
                {
                    if (fpgaController.AXILite_Read(fpgaConfigParam.base, fpgaConfigParam.offset, &rValue))
                    {
printf(" | --------------------------------------------------------------------- |\n");
printf("                           [\033[92m WRITE AXI-LITE SUCCESS \033[0m]\n");
printf("\n");
printf("   <address>      \033[33m0x%X\033[0m\n", fpgaConfigParam.base + fpgaConfigParam.offset);
printf("   <write value>  \033[33m0x%X\033[0m\n", fpgaConfigParam.writeValue);
printf("   <read back>    \033[33m0x%X\033[0m\n", rValue);
printf("\n");
printf(" | --------------------------------------------------------------------- |\n");
                    }
                    else
                    {
printf(" | --------------------------------------------------------------------- |\n");
printf("                           [\033[31m WRITE AXI-LITE FAILED \033[0m]\n");
printf(" | --------------------------------------------------------------------- |\n");
                    }
                }
                else
                {
printf(" | --------------------------------------------------------------------- |\n");
printf("                           [\033[31m WRITE AXI-LITE FAILED \033[0m]\n");
printf(" | --------------------------------------------------------------------- |\n");
                }
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
                buffer.release();
                return 0;
            }
            break;
        }

        /* Read AXI-Full bus (DMA transfer method) */

        case FPGA_TOOL__OPERATE__READ_AXI_FULL_BUFFER:
        {
            try
            {
                dmaTransferConfig.ddrOffset = fpgaConfigParam.offset;
                dmaTransferConfig.transferByteSize = fpgaConfigParam.transferBytes;
                dmaTransferConfig.transferDirectionSelection = DMA_TRANSFER_DIRECTION__FPGA_TO_HOST;
                dmaTransferConfig.transferDmaChannel = 0;

                if(fpgaController.AXIFull_BufferTransfer(dmaTransferConfig, &buffer))
                {
printf(" | --------------------------------------------------------------------- |\n");
printf("                           [\033[92m READ AXI-FULL SUCCESS \033[0m]\n");
                    if(buffer.to_file(fpgaConfigParam.datafileName, 0, fpgaConfigParam.transferBytes))
                    {
std::cout << "   Successfully save <\033[33m" << fpgaConfigParam.transferBytes << "\033[0m> bytes to file: " << fpgaConfigParam.datafileName << std::endl;
                    }
                    else
                    {
printf("   Failed to save data to file.\n");
                    }
                }
                else
                {
printf("                           [\033[31m READ AXI-FULL FAILED \033[0m]\n");
printf(" | --------------------------------------------------------------------- |\n");
                }
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
                buffer.release();
                return 0;
            }
            break;
        }

        /* Write AXI-Full bus (DMA transfer method) */

        case FPGA_TOOL__OPERATE__WRITE_AXI_FULL_BUFFER:
        {
            try
            {
                dmaTransferConfig.ddrOffset = fpgaConfigParam.offset;
                dmaTransferConfig.transferByteSize = fpgaConfigParam.transferBytes;
                dmaTransferConfig.transferDirectionSelection = DMA_TRANSFER_DIRECTION__HOST_TO_FPGA;
                dmaTransferConfig.transferDmaChannel = 0;

                buffer.release();
printf(" | --------------------------------------------------------------------- |\n");
                if (buffer.from_file(fpgaConfigParam.datafileName, 0, fpgaConfigParam.transferBytes))
                {
                    if(fpgaController.AXIFull_BufferTransfer(dmaTransferConfig, &buffer))
                    {
printf("                         [\033[92m WRITE AXI-FULL SUCCESS \033[0m]\n");
                    }
                    else
                    {
printf("                         [\033[31m WRITE AXI-FULL FAILED \033[0m]\n");
                    }
                }
                else
                {
std::cout << "   Failed to load data from: " << fpgaConfigParam.datafileName << std::endl;
                }
printf(" | --------------------------------------------------------------------- |\n");
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
                buffer.release();
                return 0;
            }
            break;
        }

        /* Read AXI-Full bus (memory map method) */

        case FPGA_TOOL__OPERATE__READ_AXI_FULL_REG:
        {
            try
            {
                if(fpgaController.AXIFull_Read(0, fpgaConfigParam.offset, &rValue))
                {
printf(" | --------------------------------------------------------------------- |\n");
printf("                           [\033[92m READ AXI-FULL SUCCESS \033[0m]\n");
printf("\n");
printf("   <address>    \033[33m0x%X\033[0m\n", fpgaConfigParam.offset);
printf("   <value>      \033[33m0x%X\033[0m\n", rValue);
printf("\n");
printf(" | --------------------------------------------------------------------- |\n");
                }
                else
                {
printf(" | --------------------------------------------------------------------- |\n");
printf("                           [\033[31m READ AXI-FULL FAILED \033[0m]\n");
printf(" | --------------------------------------------------------------------- |\n");
                }
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
                return 0;
            }
            break;
        }

        /* Write AXI-Full bus (memory map method) */

        case FPGA_TOOL__OPERATE__WRITE_AXI_FULL_REG:
        {
            try
            {
                if(fpgaController.AXIFull_Write(0, fpgaConfigParam.offset, fpgaConfigParam.writeValue))
                {
                    if (fpgaController.AXIFull_Read(0, fpgaConfigParam.offset, &rValue))
                    {
printf(" | --------------------------------------------------------------------- |\n");
printf("                           [\033[92m WRITE AXI-FULL SUCCESS \033[0m]\n");
printf("\n");
printf("   <address>      \033[33m0x%X\033[0m\n", fpgaConfigParam.offset);
printf("   <write value>  \033[33m0x%X\033[0m\n", fpgaConfigParam.writeValue);
printf("   <read back>    \033[33m0x%X\033[0m\n", rValue);
printf("\n");
printf(" | --------------------------------------------------------------------- |\n");
                    }
                    else
                    {
printf(" | --------------------------------------------------------------------- |\n");
printf("                           [\033[31m WRITE AXI-FULL FAILED \033[0m]\n");
printf(" | --------------------------------------------------------------------- |\n");
                    }
                }
                else
                {
printf(" | --------------------------------------------------------------------- |\n");
printf("                           [\033[31m WRITE AXI-FULL FAILED \033[0m]\n");
printf(" | --------------------------------------------------------------------- |\n");
                }
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
                return 0;
            }
            break;
        }
        case FPGA_TOOL__OPERATE__READ_CONTROL:
        {
            try
            {
                if(fpgaController.XDMA_Read(fpgaConfigParam.offset, &rValue))
                {
printf(" | --------------------------------------------------------------------- |\n");
printf("                     [\033[92m READ XDMA CONTROL SUCCESS \033[0m]\n");
printf("\n");
printf("   <address>    \033[33m0x%X\033[0m\n", fpgaConfigParam.offset);
printf("   <value>      \033[33m0x%X\033[0m\n", rValue);
printf("\n");
printf(" | --------------------------------------------------------------------- |\n");
                }
                else
                {
printf(" | --------------------------------------------------------------------- |\n");
printf("                     [\033[31m READ XDMA CONTROL FAILED \033[0m]\n");
printf(" | --------------------------------------------------------------------- |\n");
                }
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
                return 0;
            }
            break;
        }
        case FPGA_TOOL__OPERATE__WRITE_CONTROL:
        {
            try
            {
                if(fpgaController.XDMA_Write(fpgaConfigParam.offset, fpgaConfigParam.writeValue))
                {
                    if (fpgaController.XDMA_Read(fpgaConfigParam.offset, &rValue))
                    {
printf(" | --------------------------------------------------------------------- |\n");
printf("                     [\033[92m WRITE XDMA CONTROL SUCCESS \033[0m]\n");
printf("\n");
printf("   <address>      \033[33m0x%X\033[0m\n", fpgaConfigParam.offset);
printf("   <write value>  \033[33m0x%X\033[0m\n", fpgaConfigParam.writeValue);
printf("   <read back>    \033[33m0x%X\033[0m\n", rValue);
printf("\n");
printf(" | --------------------------------------------------------------------- |\n");
                    }
                    else
                    {
printf(" | --------------------------------------------------------------------- |\n");
printf("                     [\033[31m WRITE XDMA CONTROL FAILED \033[0m]\n");
printf(" | --------------------------------------------------------------------- |\n");
                    }
                }
                else
                {
printf(" | --------------------------------------------------------------------- |\n");
printf("                     [\033[31m WRITE XDMA CONTROL FAILED \033[0m]\n");
printf(" | --------------------------------------------------------------------- |\n");
                }
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
                return 0;
            }
            break;
        }

        default: 
        {
            break;
        }
    }

    buffer.release();
    return 0;
}
