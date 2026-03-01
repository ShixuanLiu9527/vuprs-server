#include "fpga_tool.h"

int main(int argc, char *argv[])
{
    std::vector<std::string> args, registerNameList;
    std::vector<uint32_t> registerOffsetList, registerValueList;
    vuprs::FPGAController controller;
    vuprs::AlignedBufferDMA buffer;
    tool::_FPGA_TOOL_CommandParseResult cmd;

    uint32_t readValue = 0;
    uint32_t memoryTransferSize = 0;

    args.resize(argc);

    for (int i = 0; i < argc; i++)
    {
        args[i] = std::string(argv[i]);
        std::transform(args[i].begin(), args[i].end(), args[i].begin(), ::toupper);  /* to upper */
    }
    
    /* Simplify command */

    tool::FPGA_TOOL_RestoreCommand(&args);

    /* Parse command */

    tool::FPGA_TOOL_ParseCommand(args, &cmd);

    if (cmd.operation == tool::_FPGA_OPERATION::OPERATION_HELP)
    {
        tool::FPGA_TOOL_PrintHelp();
        return 0;
    }
    else if (cmd.operation == tool::_FPGA_OPERATION::OPERATION_ERR)
    {
        tool::FPGA_TOOL_PrintErrorInfo();
        return 0;
    }

    /* Open device files */

    try
    {
        controller.ConfigFPGAFromJson(DEFAULT_FPGA_CONFIG_JSON_FILENAME);
    }
    catch(const std::exception& e)
    {
        std::cout << e.what() << std::endl;
    }

    /* Response command */

    try
    {
        switch (cmd.operation)
        {
            /* List device */

            case tool::_FPGA_OPERATION::LIST_DEVICE__ADC:
            {
                controller.dev__ADC_Controller.ReadAllRegisters(&registerNameList, &registerOffsetList, &registerValueList);
                tool::FPGA_TOOL_PrintDeviceRegisters(registerNameList, registerOffsetList, registerValueList);
                break;
            }
            case tool::_FPGA_OPERATION::LIST_DEVICE__DMA:
            {
                controller.dev__AXI_DMA.ReadAllRegisters(&registerNameList, &registerOffsetList, &registerValueList);
                tool::FPGA_TOOL_PrintDeviceRegisters(registerNameList, registerOffsetList, registerValueList);
                break;
            }
            case tool::_FPGA_OPERATION::LIST_DEVICE__CBUF:
            {
                controller.dev__Circular_Buffer.ReadAllRegisters(&registerNameList, &registerOffsetList, &registerValueList);
                tool::FPGA_TOOL_PrintDeviceRegisters(registerNameList, registerOffsetList, registerValueList);
                break;
            }
            case tool::_FPGA_OPERATION::LIST_DEVICE__FIR:
            {
                controller.dev__FIR_Filter_Bank.ReadAllRegisters(&registerNameList, &registerOffsetList, &registerValueList);
                tool::FPGA_TOOL_PrintDeviceRegisters(registerNameList, registerOffsetList, registerValueList);
                break;
            }
            case tool::_FPGA_OPERATION::LIST_DEVICE__PDLY:
            {
                controller.dev__PreDelay_Unit.ReadAllRegisters(&registerNameList, &registerOffsetList, &registerValueList);
                tool::FPGA_TOOL_PrintDeviceRegisters(registerNameList, registerOffsetList, registerValueList);
                break;
            }

            /* Device control */

            case tool::_FPGA_OPERATION::DEVICE_REGISTER_OPERATION__ADC:
            {
                if (cmd.isread) 
                {
                    controller.dev__ADC_Controller.ReadSingleRegister(cmd.offset, &readValue);
                    tool::FPGA_TOOL_PrintValue(cmd.offset, readValue);
                }
                else 
                {
                    controller.dev__ADC_Controller.WriteSingleRegister(cmd.offset, cmd.value);
                }
                break;
            }
            case tool::_FPGA_OPERATION::DEVICE_REGISTER_OPERATION__DMA:
            {
                if (cmd.isread) 
                {
                    controller.dev__AXI_DMA.ReadSingleRegister(cmd.offset, &readValue);
                    tool::FPGA_TOOL_PrintValue(cmd.offset, readValue);
                }
                else 
                {
                    controller.dev__AXI_DMA.WriteSingleRegister(cmd.offset, cmd.value);
                }
                break;
            }
            case tool::_FPGA_OPERATION::DEVICE_REGISTER_OPERATION__CBUF:
            {
                if (cmd.isread) 
                {
                    controller.dev__Circular_Buffer.ReadSingleRegister(cmd.offset, &readValue);
                    tool::FPGA_TOOL_PrintValue(cmd.offset, readValue);
                }
                else 
                {
                    controller.dev__Circular_Buffer.WriteSingleRegister(cmd.offset, cmd.value);
                }
                break;
            }
            case tool::_FPGA_OPERATION::DEVICE_REGISTER_OPERATION__FIR:
            {
                if (cmd.isread) 
                {
                    controller.dev__FIR_Filter_Bank.ReadSingleRegister(cmd.offset, &readValue);
                    tool::FPGA_TOOL_PrintValue(cmd.offset, readValue);
                }
                else 
                {
                    controller.dev__FIR_Filter_Bank.WriteSingleRegister(cmd.offset, cmd.value);
                }
                break;
            }
            case tool::_FPGA_OPERATION::DEVICE_REGISTER_OPERATION__PDLY:
            {
                if (cmd.isread) 
                {
                    controller.dev__PreDelay_Unit.ReadSingleRegister(cmd.offset, &readValue);
                    tool::FPGA_TOOL_PrintValue(cmd.offset, readValue);
                }
                else 
                {
                    controller.dev__PreDelay_Unit.WriteSingleRegister(cmd.offset, cmd.value);
                }
                break;
            }

            /* Memory control */

            case tool::_FPGA_OPERATION::MEMORY_OPERATION__DDR:
            {
                if (cmd.isread) 
                {
                    buffer.malloc(cmd.transfersize);
                    controller.mem__DDR.ReadMemory(&buffer, cmd.offset, cmd.transfersize);
                    buffer.to_file(cmd.file);
                }
                else
                {
                    buffer.from_file(cmd.file);
                    if (cmd.transferSizeIfFilesize) memoryTransferSize = buffer.size();
                    else memoryTransferSize = cmd.transfersize;
                    controller.mem__DDR.WriteMemory(&buffer, cmd.offset, memoryTransferSize);
                }
                break;
            }
            case tool::_FPGA_OPERATION::MEMORY_OPERATION__FIR_BRAM:
            {
                if (cmd.isread) 
                {
                    buffer.malloc(cmd.transfersize);
                    controller.mem__FIR_BRAM.ReadMemory(&buffer, cmd.offset, cmd.transfersize);
                    buffer.to_file(cmd.file);
                }
                else
                {
                    buffer.from_file(cmd.file);
                    if (cmd.transferSizeIfFilesize) memoryTransferSize = buffer.size();
                    else memoryTransferSize = cmd.transfersize;
                    controller.mem__FIR_BRAM.WriteMemory(&buffer, cmd.offset, memoryTransferSize);
                }
                break;
            }
            case tool::_FPGA_OPERATION::MEMORY_OPERATION__SG_BRAM:
            {
                if (cmd.isread) 
                {
                    buffer.malloc(cmd.transfersize);
                    controller.mem__SG_BRAM.ReadMemory(&buffer, cmd.offset, cmd.transfersize);
                    buffer.to_file(cmd.file);
                }
                else
                {
                    buffer.from_file(cmd.file);
                    if (cmd.transferSizeIfFilesize) memoryTransferSize = buffer.size();
                    else memoryTransferSize = cmd.transfersize;
                    controller.mem__SG_BRAM.WriteMemory(&buffer, cmd.offset, memoryTransferSize);
                }
                break;
            }
            case tool::_FPGA_OPERATION::MEMORY_OPERATION__CBUF_BRAM:
            {
                if (cmd.isread) 
                {
                    buffer.malloc(cmd.transfersize);
                    controller.mem__Circular_Buffer_BRAM.ReadMemory(&buffer, cmd.offset, cmd.transfersize);
                    buffer.to_file(cmd.file);
                }
                else
                {
                    buffer.from_file(cmd.file);
                    if (cmd.transferSizeIfFilesize) memoryTransferSize = buffer.size();
                    else memoryTransferSize = cmd.transfersize;
                    controller.mem__Circular_Buffer_BRAM.WriteMemory(&buffer, cmd.offset, memoryTransferSize);
                }
                break;
            }

            default: 
            {
                break;
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cout << e.what() << std::endl;
    }
    return 0;
}
