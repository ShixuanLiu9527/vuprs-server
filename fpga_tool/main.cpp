#include "fpga_tool/include/fpga_tool.h"

int main(int argc, char *argv[])
{
    std::vector<std::string> args, args_lower, register_name_list;
    std::vector<uint32_t> register_offset_list, register_value_list;
    vuprs::FPGAController controller;
    vuprs::AlignedBufferDMA buffer;
    tool::_FPGA_TOOL_CommandParseResult cmd;
    uint32_t read_value = 0;
    uint32_t memory_transfer_size = 0;
    args.resize(argc);
    args_lower.resize(argc);
    for (int i = 0; i < argc; i++)
    {
        args[i] = std::string(argv[i]);
        args_lower[i] = args[i];
        std::transform(args[i].begin(), args[i].end(), args[i].begin(), ::toupper); /* to upper */
    }
    /* Simplify command */
    tool::FPGA_TOOL_RestoreCommand(&args);
    /* Parse command */
    tool::FPGA_TOOL_ParseCommand(args, args_lower, &cmd);
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
    catch (const std::exception &e)
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
            controller.dev__adc_controller.ReadAllRegisters(&register_name_list, &register_offset_list, &register_value_list);
            tool::FPGA_TOOL_PrintDeviceRegisters(register_name_list, register_offset_list, register_value_list);
            break;
        }
        case tool::_FPGA_OPERATION::LIST_DEVICE__DMA:
        {
            controller.dev__axi_dma.ReadAllRegisters(&register_name_list, &register_offset_list, &register_value_list);
            tool::FPGA_TOOL_PrintDeviceRegisters(register_name_list, register_offset_list, register_value_list);
            break;
        }
        case tool::_FPGA_OPERATION::LIST_DEVICE__CBUF:
        {
            controller.dev__circular_buffer.ReadAllRegisters(&register_name_list, &register_offset_list, &register_value_list);
            tool::FPGA_TOOL_PrintDeviceRegisters(register_name_list, register_offset_list, register_value_list);
            break;
        }
        case tool::_FPGA_OPERATION::LIST_DEVICE__FIR:
        {
            controller.dev__fir_filter_bank.ReadAllRegisters(&register_name_list, &register_offset_list, &register_value_list);
            tool::FPGA_TOOL_PrintDeviceRegisters(register_name_list, register_offset_list, register_value_list);
            break;
        }
        case tool::_FPGA_OPERATION::LIST_DEVICE__PDLY:
        {
            controller.dev__predelay_unit.ReadAllRegisters(&register_name_list, &register_offset_list, &register_value_list);
            tool::FPGA_TOOL_PrintDeviceRegisters(register_name_list, register_offset_list, register_value_list);
            break;
        }
            /* Device control */
        case tool::_FPGA_OPERATION::DEVICE_REGISTER_OPERATION__ADC:
        {
            if (cmd.isread)
            {
                controller.dev__adc_controller.ReadSingleRegister(cmd.offset, &read_value);
                tool::FPGA_TOOL_PrintValue(cmd.offset, read_value);
            }
            else
            {
                controller.dev__adc_controller.WriteSingleRegister(cmd.offset, cmd.value);
            }
            break;
        }
        case tool::_FPGA_OPERATION::DEVICE_REGISTER_OPERATION__DMA:
        {
            if (cmd.isread)
            {
                controller.dev__axi_dma.ReadSingleRegister(cmd.offset, &read_value);
                tool::FPGA_TOOL_PrintValue(cmd.offset, read_value);
            }
            else
            {
                controller.dev__axi_dma.WriteSingleRegister(cmd.offset, cmd.value);
            }
            break;
        }
        case tool::_FPGA_OPERATION::DEVICE_REGISTER_OPERATION__CBUF:
        {
            if (cmd.isread)
            {
                controller.dev__circular_buffer.ReadSingleRegister(cmd.offset, &read_value);
                tool::FPGA_TOOL_PrintValue(cmd.offset, read_value);
            }
            else
            {
                controller.dev__circular_buffer.WriteSingleRegister(cmd.offset, cmd.value);
            }
            break;
        }
        case tool::_FPGA_OPERATION::DEVICE_REGISTER_OPERATION__FIR:
        {
            if (cmd.isread)
            {
                controller.dev__fir_filter_bank.ReadSingleRegister(cmd.offset, &read_value);
                tool::FPGA_TOOL_PrintValue(cmd.offset, read_value);
            }
            else
            {
                controller.dev__fir_filter_bank.WriteSingleRegister(cmd.offset, cmd.value);
            }
            break;
        }
        case tool::_FPGA_OPERATION::DEVICE_REGISTER_OPERATION__PDLY:
        {
            if (cmd.isread)
            {
                controller.dev__predelay_unit.ReadSingleRegister(cmd.offset, &read_value);
                tool::FPGA_TOOL_PrintValue(cmd.offset, read_value);
            }
            else
            {
                controller.dev__predelay_unit.WriteSingleRegister(cmd.offset, cmd.value);
            }
            break;
        }
            /* Memory control */
        case tool::_FPGA_OPERATION::MEMORY_OPERATION__DDR:
        {
            if (cmd.isread)
            {
                buffer.malloc(cmd.transfer_size);
                controller.mem__ddr.ReadMemory(&buffer, cmd.offset, cmd.transfer_size);
                buffer.to_file(cmd.file);
            }
            else
            {
                buffer.from_file(cmd.file);
                if (cmd.transfer_size_if_file_size)
                    memory_transfer_size = buffer.size();
                else
                    memory_transfer_size = cmd.transfer_size;
                controller.mem__ddr.WriteMemory(&buffer, cmd.offset, memory_transfer_size);
            }
            break;
        }
        case tool::_FPGA_OPERATION::MEMORY_OPERATION__FIR_BRAM:
        {
            if (cmd.isread)
            {
                buffer.malloc(cmd.transfer_size);
                controller.mem__fir_bram.ReadMemory(&buffer, cmd.offset, cmd.transfer_size);
                buffer.to_file(cmd.file);
            }
            else
            {
                buffer.from_file(cmd.file);
                if (cmd.transfer_size_if_file_size)
                    memory_transfer_size = buffer.size();
                else
                    memory_transfer_size = cmd.transfer_size;
                controller.mem__fir_bram.WriteMemory(&buffer, cmd.offset, memory_transfer_size);
            }
            break;
        }
        case tool::_FPGA_OPERATION::MEMORY_OPERATION__SG_BRAM:
        {
            if (cmd.isread)
            {
                buffer.malloc(cmd.transfer_size);
                controller.mem__sg_bram.ReadMemory(&buffer, cmd.offset, cmd.transfer_size);
                buffer.to_file(cmd.file);
            }
            else
            {
                buffer.from_file(cmd.file);
                if (cmd.transfer_size_if_file_size)
                    memory_transfer_size = buffer.size();
                else
                    memory_transfer_size = cmd.transfer_size;
                controller.mem__sg_bram.WriteMemory(&buffer, cmd.offset, memory_transfer_size);
            }
            break;
        }
        case tool::_FPGA_OPERATION::MEMORY_OPERATION__CBUF_BRAM:
        {
            if (cmd.isread)
            {
                buffer.malloc(cmd.transfer_size);
                controller.mem___circular_buffer_bram.ReadMemory(&buffer, cmd.offset, cmd.transfer_size);
                buffer.to_file(cmd.file);
            }
            else
            {
                buffer.from_file(cmd.file);
                if (cmd.transfer_size_if_file_size)
                    memory_transfer_size = buffer.size();
                else
                    memory_transfer_size = cmd.transfer_size;
                controller.mem___circular_buffer_bram.WriteMemory(&buffer, cmd.offset, memory_transfer_size);
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
