#include <iostream>

#include "fpga_config.h"
#include "fpga_control.h"
#include "aligned_data_structure.h"

#define FPGA_TOOL__OPERATE__READ_AXI_LITE           0U
#define FPGA_TOOL__OPERATE__WRITE_AXI_LITE          1U
#define FPGA_TOOL__OPERATE__READ_AXI_FULL           2U
#define FPGA_TOOL__OPERATE__WRITE_AXI_FULL          4U
#define FPGA_TOOL__OPERATE__ERROR                   5U
#define FPGA_TOOL__OPERATE__FOR_HELP                6U

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

/* Parse operation */

#define IS__FPGA_TOOL__OPERATE__READ_AXI_LITE(STR_LIST) \
(STR_LIST[2] == "R" && STR_LIST[4] == "LITE")

#define IS__FPGA_TOOL__OPERATE__WRITE_AXI_LITE(STR_LIST) \
(STR_LIST[2] == "W" && STR_LIST[4] == "LITE")

#define IS__FPGA_TOOL__OPERATE__READ_AXI_FULL(STR_LIST) \
(STR_LIST[2] == "R" && STR_LIST[4] == "FULL")

#define IS__FPGA_TOOL__OPERATE__WRITE_AXI_FULL(STR_LIST) \
(STR_LIST[2] == "W" && STR_LIST[4] == "FULL")

typedef struct FPGA_TOOL_AXIParameters
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
printf(" | fpga-tool --rw <rw> --bus <b> --cfg <cfg> --base <ba>                 |\n");
printf(" |           --offset <of> --bytes <by> --io <io>                        |\n");
printf(" |                                                                       |\n");
printf(" | [ \033[92mPARAMETERS\033[0m ]                                                        |\n");
printf(" |                                                                       |\n");
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
printf(" | fpga-tool --rw \033[33mr\033[0m --bus \033[33mlite\033[0m --cfg \033[33m./cfg.json\033[0m --base \033[33m0\033[0m --offset \033[33m0x04\033[0m   |\n");
printf(" |                                                                       |\n");
printf(" | fpga-tool --rw \033[33mw\033[0m --bus \033[33mlite\033[0m --cfg \033[33m./cfg.json\033[0m --base \033[33m0\033[0m --offset \033[33m0x0C\033[0m   |\n");
printf(" |           --io \033[33m0XFF\033[0m                                                   |\n");
printf(" |                                                                       |\n");
printf(" | fpga-tool --rw \033[33mr\033[0m --bus \033[33mfull\033[0m --cfg \033[33m./cfg.json\033[0m --offset \033[33m0\033[0m --bytes \033[33m1024\033[0m  |\n");
printf(" |           --io \033[33m./r_data.bin\033[0m                                           |\n");
printf(" |                                                                       |\n");
printf(" | fpga-tool --rw \033[33mw\033[0m --bus \033[33mfull\033[0m --cfg \033[33m./cfg.json\033[0m --offset \033[33m0\033[0m --bytes \033[33m1024\033[0m  |\n");
printf(" |           --io \033[33m./w_data.bin\033[0m                                           |\n");
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
        else if (IS__FPGA_TOOL__OPERATE__READ_AXI_FULL_CMD(cmdListUpper) && IS__FPGA_TOOL__OPERATE__READ_AXI_FULL(cmdListUpper))
        {
            retParameters.operate = FPGA_TOOL__OPERATE__READ_AXI_FULL;

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
        else if (IS__FPGA_TOOL__OPERATE__WRITE_AXI_FULL_CMD(cmdListUpper) && IS__FPGA_TOOL__OPERATE__WRITE_AXI_FULL(cmdListUpper))
        {
            retParameters.operate = FPGA_TOOL__OPERATE__WRITE_AXI_FULL;

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

        /* FPGA I/O */

        case FPGA_TOOL__OPERATE__READ_AXI_LITE:
        {
            try
            {
                if(fpgaController.AXILite_Read(fpgaConfigParam.base, fpgaConfigParam.offset, &rValue))
                {
printf(" | --------------------------------------------------------------------- |\n");
printf("                           [\033[92mREAD AXI-LITE SUCCESS\033[0m]\n");
printf("\n");
printf("   <address>    \033[33m0x%X\033[0m\n", fpgaConfigParam.base + fpgaConfigParam.offset);
printf("   <value>      \033[33m0x%X\033[0m\n", rValue);
printf("\n");
printf(" | --------------------------------------------------------------------- |\n");
                }
                else
                {
printf(" | --------------------------------------------------------------------- |\n");
printf("                           [\033[31mREAD AXI-LITE FAILED\033[0m]\n");
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
        case FPGA_TOOL__OPERATE__WRITE_AXI_LITE:
        {
            try
            {
                if(fpgaController.AXILite_Write(fpgaConfigParam.base, fpgaConfigParam.offset, fpgaConfigParam.writeValue))
                {
                    if (fpgaController.AXILite_Read(fpgaConfigParam.base, fpgaConfigParam.offset, &rValue))
                    {
printf(" | --------------------------------------------------------------------- |\n");
printf("                           [\033[92mWRITE AXI-LITE SUCCESS\033[0m]\n");
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
printf("                           [\033[31mWRITE AXI-LITE FAILED\033[0m]\n");
printf(" | --------------------------------------------------------------------- |\n");
                    }
                }
                else
                {
printf(" | --------------------------------------------------------------------- |\n");
printf("                           [\033[31mWRITE AXI-LITE FAILED\033[0m]\n");
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
        case FPGA_TOOL__OPERATE__READ_AXI_FULL:
        {
            try
            {
                dmaTransferConfig.ddrOffset = fpgaConfigParam.offset;
                dmaTransferConfig.transferByteSize = fpgaConfigParam.transferBytes;
                dmaTransferConfig.transferDirectionSelection = DMA_TRANSFER_DIRECTION__FPGA_TO_HOST;
                dmaTransferConfig.transferDmaChannel = 0;

                if(fpgaController.AXIFull_IO(dmaTransferConfig, &buffer))
                {
printf(" | --------------------------------------------------------------------- |\n");
printf("                           [\033[92mREAD AXI-FULL SUCCESS\033[0m]\n");
                    if(buffer.to_file(fpgaConfigParam.datafileName, 0, fpgaConfigParam.transferBytes))
                    {
std::cout << "   Successfully save <" << fpgaConfigParam.transferBytes << "> bytes to file: " << fpgaConfigParam.datafileName;
                    }
                    else
                    {
printf("   Failed to save data to file.");
                    }
                }
                else
                {
printf("                           [\033[31mREAD AXI-FULL FAILED\033[0m]\n");
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
        case FPGA_TOOL__OPERATE__WRITE_AXI_FULL:
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
                    if(fpgaController.AXIFull_IO(dmaTransferConfig, &buffer))
                    {
printf("                         [\033[92mWRITE AXI-FULL SUCCESS\033[0m]\n");
                    }
                    else
                    {
printf("                         [\033[31mWRITE AXI-FULL FAILED\033[0m]\n");
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

        default: 
        {
            break;
        }
    }

    buffer.release();
    return 0;
}
