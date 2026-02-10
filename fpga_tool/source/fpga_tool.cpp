#include "fpga_tool.h"

void tool::_FPGA_TOOL_CommandParseResult_ToDefault(_FPGA_TOOL_CommandParseResult *result)
{
    result->operation = _FPGA_OPERATION::OPERATION_ERR;
    result->isread = -1;
    result->offset = 0;
    result->value = 0;
    result->transfersize = 0;
    result->transferSizeIfFilesize = false;
    result->file = "";
}

void tool::FPGA_TOOL_PrintHelp()
{
    printf("\n");
    printf(" |================= [ FPGA CONTROLLER TOOL HELP ] ===============|\n");
    printf(" |                                                               |\n");
    printf(" | ----- [ 1. Device Register Operations ] --------------------- |\n");
    printf(" |                                                               |\n");
    printf(" | (1) Read register:                                            |\n");
    printf(" |                                                               |\n");
    printf(" |     \033[33mcontroller\033[0m \033[34m-p\033[0m r \033[34m-m\033[0m <\033[32mmod\033[0m> \033[34m-f\033[0m <\033[32moff\033[0m>                         |\n");
    printf(" |                                                               |\n");
    printf(" |     <\033[32mmod\033[0m> module name: adc, dma, cbuf, fir, pdly              |\n");
    printf(" |     <\033[32moff\033[0m> register offset (hex or decimal, 4-byte aligned)    |\n");
    printf(" |                                                               |\n");
    printf(" | (2) Write register:                                           |\n");
    printf(" |                                                               |\n");
    printf(" |     \033[33mcontroller\033[0m \033[34m-p\033[0m w \033[34m-m\033[0m <\033[32mmod\033[0m> \033[34m-f\033[0m <\033[32moff\033[0m> \033[34m-v\033[0m <\033[32mval\033[0m>                |\n");
    printf(" |                                                               |\n");
    printf(" |     <\033[32mval\033[0m> value to write (hex or decimal, 32-bit unsigned)    |\n");
    printf(" |                                                               |\n");
    printf(" | (3) List all registers:                                       |\n");
    printf(" |                                                               |\n");
    printf(" |     \033[33mcontroller\033[0m \033[34m-p\033[0m l \033[34m-m\033[0m <\033[32mmod\033[0m>                                  |\n");
    printf(" |     \033[33mcontroller\033[0m \033[34m-p\033[0m r \033[34m-m\033[0m <\033[32mmod\033[0m>                                  |\n");
    printf(" |                                                               |\n");
    printf(" | ----- [ 2. Memory Data Operations ] ------------------------- |\n");
    printf(" |                                                               |\n");
    printf(" | (1) Read data to file:                                        |\n");
    printf(" |                                                               |\n");
    printf(" |     \033[33mcontroller\033[0m \033[34m-p\033[0m r \033[34m-m\033[0m <\033[32mmod\033[0m> \033[34m-f\033[0m <\033[32moff\033[0m> \033[34m-s\033[0m <\033[32msize\033[0m> \033[34m-o\033[0m <\033[32mfile\033[0m>     |\n");
    printf(" |                                                               |\n");
    printf(" |     if offset = 0:                                            |\n");
    printf(" |     \033[33mcontroller\033[0m \033[34m-p\033[0m r \033[34m-m\033[0m <\033[32mmod\033[0m> \033[34m-s\033[0m <\033[32msize\033[0m> \033[34m-o\033[0m <\033[32mfile\033[0m>              |\n");
    printf(" |                                                               |\n");
    printf(" |     <\033[32mmod\033[0m> module name: ddr, fir-bram, sg-bram, cbuf-bram      |\n");
    printf(" |     <\033[32moff\033[0m> memory offset (hex or decimal, 4-byte aligned)      |\n");
    printf(" |     <\033[32msize\033[0m> transfer size in bytes                             |\n");
    printf(" |     <\033[32mfile\033[0m> output filename                                    |\n");
    printf(" |                                                               |\n");
    printf(" | (2) Write data from file:                                     |\n");
    printf(" |                                                               |\n");
    printf(" |     \033[33mcontroller\033[0m \033[34m-p\033[0m w \033[34m-m\033[0m <\033[32mmod\033[0m> \033[34m-f\033[0m <\033[32moff\033[0m> \033[34m-s\033[0m <\033[32msize\033[0m> \033[34m-i\033[0m <\033[32mfile\033[0m>     |\n");
    printf(" |                                                               |\n");
    printf(" |     if offset = 0:                                            |\n");
    printf(" |     \033[33mcontroller\033[0m \033[34m-p\033[0m w \033[34m-m\033[0m <\033[32mmod\033[0m> \033[34m-s\033[0m <\033[32msize\033[0m> \033[34m-i\033[0m <\033[32mfile\033[0m>              |\n");
    printf(" |                                                               |\n");
    printf(" |     if offset = 0, size = file size:                          |\n");
    printf(" |     \033[33mcontroller\033[0m \033[34m-p\033[0m w \033[34m-m\033[0m <\033[32mmod\033[0m> \033[34m-i\033[0m <\033[32mfile\033[0m>                        |\n");
    printf(" |                                                               |\n");
    printf(" |     <\033[32mfile\033[0m> input filename                                     |\n");
    printf(" |                                                               |\n");
    printf(" |===============================================================|\n");
    printf("\n");
}

void tool::FPGA_TOOL_PrintErrorInfo()
{
    printf("   \033[31mCommand error.\033[0m See help: controller -h");
}

void tool::FPGA_TOOL_PrintValue(uint32_t offset, uint32_t val)
{
printf(" | --------------------------- Read Value ------------------------------ |\n");
printf("   <address>    \033[33m0x%X\033[0m\n", offset);
printf("   <value>      \033[33m0x%X\033[0m\n", val);
printf(" | --------------------------------------------------------------------- |\n");
}

void tool::FPGA_TOOL_PrintDeviceRegisters(const std::vector<std::string> &name, const std::vector<uint32_t> &offset, const std::vector<uint32_t> &val)
{
    int len = name.size();

    if (len == 0) 
    {
        throw std::runtime_error("No registers to display.");
    }
    
    printf(" | ------------------------ List Registers ----------------------------- |\n");
    
    size_t maxNameLength = 0;
    for (int i = 0; i < len; i++) 
    {
        if (name[i].length() > maxNameLength) 
        {
            maxNameLength = name[i].length();
        }
    }
    
    int maxOffsetHexLength = 0;
    for (int i = 0; i < len; i++) 
    {
        uint32_t temp = offset[i];
        int hexLength = 0;
        if (temp == 0) 
        {
            hexLength = 1;
        } 
        else 
        {
            while (temp > 0) 
            {
                temp >>= 4;
                hexLength++;
            }
        }
        if (hexLength > maxOffsetHexLength) 
        {
            maxOffsetHexLength = hexLength;
        }
    }
    
    if (maxOffsetHexLength < 2) maxOffsetHexLength = 2;
    if (maxOffsetHexLength % 2 != 0) maxOffsetHexLength++;
    
    for (int i = 0; i < len; i++) 
    {
        printf("   ");
        printf("(0x");
        uint32_t temp = offset[i];
        int hexLength = 0;
        if (temp == 0) 
        {
            hexLength = 1;
        } 
        else 
        {
            uint32_t t = temp;
            while (t > 0) 
            {
                t >>= 4;
                hexLength++;
            }
        }

        for (int j = hexLength; j < maxOffsetHexLength; j++) 
        {
            printf("0");
        }
        
        printf("%X)", offset[i]);
        printf(" ");
        printf("%s", name[i].c_str());

        int spacesAfterName = maxNameLength - name[i].length();
        printf(":");
        
        for (int j = 0; j < spacesAfterName + 2; j++) 
        {
            printf(" ");
        }

        printf("\033[33m0x%08X\033[0m\n", val[i]);
    }
    
    printf(" | --------------------------------------------------------------------- |\n");
}

void tool::FPGA_TOOL_ParseCommand(const std::vector<std::string> &args, _FPGA_TOOL_CommandParseResult *result)
{
    bool offset_parseStatus = false, value_parseStatus = false, transfersize_parseStatus = false;

    tool::_FPGA_TOOL_CommandParseResult_ToDefault(result);

    if (args.size() == 2)
    {
        if (IS_FOR_HELP(args)) result->operation = _FPGA_OPERATION::OPERATION_HELP;
    }
    if (args.size() == 5)
    {
        if (IS_LIST_DEVICE_REGISTERS(args, DEVICE_NAME__ADC_CONTROLLER)) result->operation = _FPGA_OPERATION::LIST_DEVICE__ADC;
        else if (IS_LIST_DEVICE_REGISTERS(args, DEVICE_NAME__AXI_DMA)) result->operation = _FPGA_OPERATION::LIST_DEVICE__DMA;
        else if (IS_LIST_DEVICE_REGISTERS(args, DEVICE_NAME__CIRCULAR_BUFFER)) result->operation = _FPGA_OPERATION::LIST_DEVICE__CBUF;
        else if (IS_LIST_DEVICE_REGISTERS(args, DEVICE_NAME__FIR_FILTER_BANK)) result->operation = _FPGA_OPERATION::LIST_DEVICE__FIR;
        else if (IS_LIST_DEVICE_REGISTERS(args, DEVICE_NAME__PRE_DELAY_UNIT)) result->operation = _FPGA_OPERATION::LIST_DEVICE__PDLY;
    }
    if (args.size() == 7)
    {
        if (IS_READ_DEVICE_REGISTER(args, DEVICE_NAME__ADC_CONTROLLER)) result->operation = _FPGA_OPERATION::DEVICE_REGISTER_OPERATION__ADC;
        else if (IS_READ_DEVICE_REGISTER(args, DEVICE_NAME__AXI_DMA)) result->operation = _FPGA_OPERATION::DEVICE_REGISTER_OPERATION__DMA;
        else if (IS_READ_DEVICE_REGISTER(args, DEVICE_NAME__CIRCULAR_BUFFER)) result->operation = _FPGA_OPERATION::DEVICE_REGISTER_OPERATION__CBUF;
        else if (IS_READ_DEVICE_REGISTER(args, DEVICE_NAME__FIR_FILTER_BANK)) result->operation = _FPGA_OPERATION::DEVICE_REGISTER_OPERATION__FIR;
        else if (IS_READ_DEVICE_REGISTER(args, DEVICE_NAME__PRE_DELAY_UNIT)) result->operation = _FPGA_OPERATION::DEVICE_REGISTER_OPERATION__PDLY;

        result->isread = true;
        result->offset = vuprs::ParseNumberFromString(args[6], &offset_parseStatus);

        if (!offset_parseStatus) result->operation = _FPGA_OPERATION::OPERATION_ERR;
    }
    else if (args.size() == 9)
    {
        if (IS_WRITE_DEVICE_REGISTER(args, DEVICE_NAME__ADC_CONTROLLER)) result->operation = _FPGA_OPERATION::DEVICE_REGISTER_OPERATION__ADC;
        else if (IS_WRITE_DEVICE_REGISTER(args, DEVICE_NAME__AXI_DMA)) result->operation = _FPGA_OPERATION::DEVICE_REGISTER_OPERATION__DMA;
        else if (IS_WRITE_DEVICE_REGISTER(args, DEVICE_NAME__CIRCULAR_BUFFER)) result->operation = _FPGA_OPERATION::DEVICE_REGISTER_OPERATION__CBUF;
        else if (IS_WRITE_DEVICE_REGISTER(args, DEVICE_NAME__FIR_FILTER_BANK)) result->operation = _FPGA_OPERATION::DEVICE_REGISTER_OPERATION__FIR;
        else if (IS_WRITE_DEVICE_REGISTER(args, DEVICE_NAME__PRE_DELAY_UNIT)) result->operation = _FPGA_OPERATION::DEVICE_REGISTER_OPERATION__PDLY;

        result->isread = false;
        result->offset = vuprs::ParseNumberFromString(args[6], &offset_parseStatus);
        result->value = vuprs::ParseNumberFromString(args[8], &value_parseStatus);

        if (!offset_parseStatus || !value_parseStatus) result->operation = _FPGA_OPERATION::OPERATION_ERR;
    }
    else if (args.size() == 11)
    {
        if (_IS_MEM_OPERATION(args, MEMORY_NAME__DDR)) result->operation = _FPGA_OPERATION::MEMORY_OPERATION__DDR;
        else if (_IS_MEM_OPERATION(args, MEMORY_NAME__FIR_BRAM)) result->operation = _FPGA_OPERATION::MEMORY_OPERATION__FIR_BRAM;
        else if (_IS_MEM_OPERATION(args, MEMORY_NAME__SG_BRAM)) result->operation = _FPGA_OPERATION::MEMORY_OPERATION__SG_BRAM;
        else if (_IS_MEM_OPERATION(args, MEMORY_NAME__CIRCULAR_BUFFER_BRAM)) result->operation = _FPGA_OPERATION::MEMORY_OPERATION__CBUF_BRAM;

        if (IS_READ_OPERATION(args[2])) result->isread = true;
        else if (IS_WRITE_OPERATION(args[2])) result->isread = false;

        result->offset = vuprs::ParseNumberFromString(args[6], &offset_parseStatus);

        if (IS_FILE_SIZE(args[8])) 
        {
            result->transfersize = 0;
            result->transferSizeIfFilesize = true;
        }
        else 
        {
            result->transfersize = vuprs::ParseNumberFromString(args[8], &transfersize_parseStatus);
            result->transferSizeIfFilesize = false;
        }

        result->file = args[10];

        if (!offset_parseStatus || !transfersize_parseStatus) result->operation = _FPGA_OPERATION::OPERATION_ERR;
    }
}

void tool::FPGA_TOOL_RestoreCommand(std::vector<std::string> *args)
{
    if (args->size() == 7)
    {
        if (IS_WRITE_DATA_TO_MEM_F0_S0((*args))) *args = RESTORE_CMD__WRITE_DATA_TO_MEM_F0_S0((*args));
    }
    else if (args->size() == 9)
    {
        if (IS_READ_DATA_FROM_MEM_F0((*args))) *args = RESTORE_CMD__READ_DATA_FROM_MEM_F0((*args));
        if (IS_WRITE_DATA_TO_MEM_F0((*args))) *args = RESTORE_CMD__WRITE_DATA_TO_MEM_F0((*args));
    }
}
