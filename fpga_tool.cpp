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
void FPGA_TOOL__PrintError();
void FPGA_TOOL__FileOpenError(const std::string &filename);
void FPGA_TOOL__ShowReadValue(const uint32_t &r_value, const uint64_t &base, const uint64_t &offset);
void FPGA_TOOL__ShowReadFile(const std::string &filename);
void FPGA_TOOL__ShowReadFailed();
FPGA_TOOL_AXIParameters FPGA_TOOL__ParseCommandParameters(const std::vector<std::string> &cmdList);

void FPGA_TOOL__PrintError()
{
printf("\n");
printf(" FPGA-TOOL: ERROR COMMAND! \n");
printf("\n");
}

void FPGA_TOOL__ShowReadFailed()
{
printf("\n");
printf(" [ READ FAILED ]\n");
printf("\n");
}

void FPGA_TOOL__FileOpenError(const std::string &filename)
{
printf("\n");
std::cout << " FPGA-TOOL: Cannot open: " << filename << std::endl;
printf("\n");
}

void FPGA_TOOL__ShowReadFile(const std::string &filename)
{
printf("\n");
printf(" [ READ SUCCESS ]\n");
printf("\n");
printf((" Data have be written to: " + filename + "\n").c_str());
printf("\n");
}

void FPGA_TOOL__ShowReadValue(const uint32_t &r_value, const uint64_t &base, const uint64_t &offset)
{
printf("\n");
printf(" [ READ SUCCESS ]\n");
printf("\n");
printf(" addr:  %lx\n", base + offset);
printf(" val:   %lx\n", r_value);
printf("\n");
}

void FPGA_TOOL__PrintHelp()
{
printf("\n");
printf(" |=============================== [ FPGA TOOL HELP ] ====================|\n");
printf(" |                                                                       |\n");
printf(" | ----- [ 1. For Help ] ----------------------------------------------- |\n");
printf(" |                                                                       |\n");
printf(" | [ COMMAND ]                                                           |\n");
printf(" |                                                                       |\n");
printf(" | fpga-tool -h                                                          |\n");
printf(" | fpga-tool --help                                                      |\n");
printf(" |                                                                       |\n");
printf(" | ----- [ 2. Access AXI-Lite & AXI-Full Bus ] ------------------------- |\n");
printf(" |                                                                       |\n");
printf(" | [ COMMAND ]                                                           |\n");
printf(" |                                                                       |\n");
printf(" | fpga-tool --rw <rw> --bus <b> --cfg <cfg> --base <ba>                 |\n");
printf(" |           --offset <of> --bytes <by> --io <io>                        |\n");
printf(" |                                                                       |\n");
printf(" | [ PARAMETERS ]                                                        |\n");
printf(" |                                                                       |\n");
printf(" | <rw> r = read from FPGA, w = write to FPGA;                           |\n");
printf(" | <b>  bus selection, lite = AXI-Lite, full = AXI-Full;                 |\n");
printf(" | <cf> config JSON file;                                                |\n");
printf(" | <ba> base address of the address space (AXI-Lite only, AXI-Full = 0); |\n");
printf(" | <of> register offset (AXI-Lite) or address offset (AXI-Full);         |\n");
printf(" | <by> read/write bytes                  (AXI-Full only, AXI-Lite = 4); |\n");
printf(" | <io> input value (AXI-Lite) or intput/output filename (AXI-Full);     |\n");
printf(" |                                                                       |\n");
printf(" | [ EXAMPLE ]                                                           |\n");
printf(" |                                                                       |\n");
printf(" | fpga-tool --rw r --bus lite --cfg ./cfg.json --base 0 --offset 0x04   |\n");
printf(" |                                                                       |\n");
printf(" | fpga-tool --rw w --bus lite --cfg ./cfg.json --base 0 --offset 0x0C   |\n");
printf(" |           --io 0XFF                                                   |\n");
printf(" |                                                                       |\n");
printf(" | fpga-tool --rw r --bus full --cfg ./cfg.json --offset 0 --bytes 1024  |\n");
printf(" |           --io ./r_data.bin                                           |\n");
printf(" |                                                                       |\n");
printf(" | fpga-tool --rw w --bus full --cfg ./cfg.json --offset 0 --bytes 1024  |\n");
printf(" |           --io ./w_data.bin                                           |\n");
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

    for (uint64_t i = 0; i < cmdSize; i++)
    {
        std::transform(cmdListUpper[i].begin(), cmdListUpper[i].end(), cmdListUpper[i].begin(), ::toupper);  /* Upper */
    }

    /* Check --rw & --bus */

    if (cmdSize == 2)
    {
        if (cmdListUpper[1] == "-h" || cmdListUpper[1] == "--help")
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

            parseValue = vuprs::ParseNumberFromString(cmdListUpper[8]);
            if (parseStatus)retParameters.base = parseValue;
            else cmdError = true;
           
            parseValue = vuprs::ParseNumberFromString(cmdListUpper[10]);
            if (parseStatus)retParameters.offset = parseValue;
            else cmdError = true;
            
        }
    }
    else if (cmdSize = 13)
    {
        if (IS__FPGA_TOOL__OPERATE__WRITE_AXI_LITE_CMD(cmdListUpper) && IS__FPGA_TOOL__OPERATE__WRITE_AXI_LITE(cmdListUpper))
        {
            retParameters.operate = FPGA_TOOL__OPERATE__WRITE_AXI_LITE;

            /* Parse user value */

            if (!cmdList[6].empty())retParameters.configFileName = cmdList[6];
            else cmdError = true;

            parseValue = vuprs::ParseNumberFromString(cmdListUpper[8]);
            if (parseStatus)retParameters.base = parseValue;
            else cmdError = true;
           
            parseValue = vuprs::ParseNumberFromString(cmdListUpper[10]);
            if (parseStatus)retParameters.offset = parseValue;
            else cmdError = true;

            parseValue = vuprs::ParseNumberFromString(cmdListUpper[12]);
            if (parseStatus)retParameters.writeValue = static_cast<uint32_t>(parseValue);
            else cmdError = true;
        }
        else if (IS__FPGA_TOOL__OPERATE__READ_AXI_FULL_CMD(cmdListUpper) && IS__FPGA_TOOL__OPERATE__READ_AXI_FULL(cmdListUpper))
        {
            retParameters.operate = FPGA_TOOL__OPERATE__READ_AXI_FULL;

            /* Parse user value */

            if (!cmdList[6].empty())retParameters.configFileName = cmdList[6];
            else cmdError = true;

            parseValue = vuprs::ParseNumberFromString(cmdListUpper[8]);
            if (parseStatus)retParameters.offset = parseValue;
            else cmdError = true;
           
            parseValue = vuprs::ParseNumberFromString(cmdListUpper[10]);
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

            parseValue = vuprs::ParseNumberFromString(cmdListUpper[8]);
            if (parseStatus)retParameters.offset = parseValue;
            else cmdError = true;
           
            parseValue = vuprs::ParseNumberFromString(cmdListUpper[10]);
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

    if (fpgaConfigParam.operate != FPGA_TOOL__OPERATE__ERROR || fpgaConfigParam.operate != FPGA_TOOL__OPERATE__FOR_HELP)
    {
        if (!fpgaConfigManager.LoadFPGAConfigFromJson(fpgaConfigParam.configFileName))
        {
            FPGA_TOOL__FileOpenError(fpgaConfigParam.configFileName);
            return 0;
        }
        else
        {
            if(!fpgaController.LoadFPGAConfig(fpgaConfigManager))
            {
printf("\n");
printf(" FPGA-TOOL: Error occurred when loading config.\n");
printf("\n");
                return 0;
            }
        }
    }

    switch (fpgaConfigParam.operate)
    {
        case FPGA_TOOL__OPERATE__FOR_HELP:
        {
            FPGA_TOOL__PrintHelp();
        }
        case FPGA_TOOL__OPERATE__ERROR: 
        {
            FPGA_TOOL__PrintError();
            FPGA_TOOL__PrintHelp();
        }

        /* FPGA I/O */

        case FPGA_TOOL__OPERATE__READ_AXI_LITE:
        {
            try
            {
                if(fpgaController.AXILite_Read(fpgaConfigParam.base, fpgaConfigParam.offset, &rValue))
                {
                    FPGA_TOOL__ShowReadValue(rValue, fpgaConfigParam.base, fpgaConfigParam.offset);
                }
                else
                {
                    FPGA_TOOL__ShowReadFailed();
                }
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
            }
        }
        case FPGA_TOOL__OPERATE__WRITE_AXI_LITE:
        {
            try
            {
                if(fpgaController.AXILite_Write(fpgaConfigParam.base, fpgaConfigParam.offset, fpgaConfigParam.writeValue))
                {
                    printf(" DONE.");
                }
                else
                {
                    printf(" FAILED.");
                }
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
            }
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
                    if(buffer.to_file(fpgaConfigParam.datafileName, 0, fpgaConfigParam.transferBytes))
                    {
                        FPGA_TOOL__ShowReadFile(fpgaConfigParam.datafileName);
                    }
                    else
                    {
                        printf(" FAILED.");
                    }
                }
                else
                {
                    printf(" FAILED.");
                }
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
            }
        }
    }
}
