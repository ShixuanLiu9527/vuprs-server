#ifndef FPGA_TOOL_H
#define FPGA_TOOL_H

#include "fpga/fpga_controller.h"

#define DEFAULT_FPGA_CONFIG_JSON_FILENAME "./fpga_config.json"

/* Module Name */

#define DEVICE_NAME__ADC_CONTROLLER "ADC"
#define DEVICE_NAME__AXI_DMA "DMA"
#define DEVICE_NAME__CIRCULAR_BUFFER "CBUF"
#define DEVICE_NAME__FIR_FILTER_BANK "FIR"
#define DEVICE_NAME__PRE_DELAY_UNIT "PDLY"

#define MEMORY_NAME__DDR "DDR"
#define MEMORY_NAME__FIR_BRAM "FIR-BRAM"
#define MEMORY_NAME__SG_BRAM "SG-BRAM"
#define MEMORY_NAME__CIRCULAR_BUFFER_BRAM "CBUF-BRAM"

#define INVALID_MODULE_NAME "-"

#define IS_READ_OPERATION(VAL) (VAL == "R")
#define IS_WRITE_OPERATION(VAL) (VAL == "W")

#define FILE_SIZE "FILE_SIZE"
#define IS_FILE_SIZE(VAL) (VAL == FILE_SIZE)

#define IS_VALID_OPERATION(VAL) (IS_READ_OPERATION(VAL) || IS_WRITE_OPERATION(VAL))

#define IS_VALID_COMMAND_LEN(VAL) (VAL == 7 || VAL == 9 || VAL == 11)

#define IS_DEVICE_NAME(VAL)                 \
    (VAL == DEVICE_NAME__ADC_CONTROLLER ||  \
     VAL == DEVICE_NAME__AXI_DMA ||         \
     VAL == DEVICE_NAME__CIRCULAR_BUFFER || \
     VAL == DEVICE_NAME__FIR_FILTER_BANK || \
     VAL == DEVICE_NAME__PRE_DELAY_UNIT)

#define IS_MEMORY_NAME(VAL)          \
    (VAL == MEMORY_NAME__DDR ||      \
     VAL == MEMORY_NAME__FIR_BRAM || \
     VAL == MEMORY_NAME__SG_BRAM ||  \
     VAL == MEMORY_NAME__CIRCULAR_BUFFER_BRAM)

/* Command parsing: argc = 2 */

#define IS_FOR_HELP(ARGS) (ARGS[1] == "-H" || ARGS[1] == "--HELP")

/* Command parsing: argc = 5 */

#define IS_LIST_DEVICE_REGISTERS(ARGS, _MODULE) (ARGS[1] == "-P" && (ARGS[2] == "L" || ARGS[2] == "R") && ARGS[3] == "-M" && IS_DEVICE_NAME(ARGS[4]) && ARGS[4] == _MODULE)

/* Command parsing: argc = 7 */

#define IS_READ_DEVICE_REGISTER(ARGS, _MODULE) (ARGS[1] == "-P" && ARGS[2] == "R" && ARGS[3] == "-M" && IS_DEVICE_NAME(ARGS[4]) && ARGS[4] == _MODULE && ARGS[5] == "-F")

/* Command parsing: argc = 9 */

#define IS_WRITE_DEVICE_REGISTER(ARGS, _MODULE) (ARGS[1] == "-P" && ARGS[2] == "W" && ARGS[3] == "-M" && IS_DEVICE_NAME(ARGS[4]) && ARGS[4] == _MODULE && ARGS[5] == "-F" && ARGS[7] == "-V")

/* Command parsing: argc = 11 */

#define _IS_MEM_OPERATION(ARGS, _MODULE) (ARGS[1] == "-P" && ARGS[3] == "-M" && IS_MEMORY_NAME(ARGS[4]) && ARGS[4] == _MODULE && ARGS[5] == "-F" && ARGS[7] == "-S")

#define IS_READ_DATA_FROM_MEM(ARGS, _MODULE) (_IS_MEM_OPERATION(ARGS, _MODULE) && ARGS[9] == "-O")
#define IS_WRITE_DATA_TO_MEM(ARGS, _MODULE) (_IS_MEM_OPERATION(ARGS, _MODULE) && ARGS[9] == "-I")

/* Simplify command parsing */

/* argc = 9 */

#define IS_READ_DATA_FROM_MEM_F0(ARGS) (ARGS[1] == "-P" && ARGS[2] == "R" && ARGS[3] == "-M" && IS_MEMORY_NAME(ARGS[4]) && ARGS[5] == "-S" && ARGS[7] == "-O")
#define RESTORE_CMD__READ_DATA_FROM_MEM_F0(ORIGIN) {ORIGIN[0], ORIGIN[1], ORIGIN[2], ORIGIN[3], ORIGIN[4], "-F", "0", ORIGIN[5], ORIGIN[6], ORIGIN[7], ORIGIN[8]}

#define IS_WRITE_DATA_TO_MEM_F0(ARGS) (ARGS[1] == "-P" && ARGS[2] == "W" && ARGS[3] == "-M" && IS_MEMORY_NAME(ARGS[4]) && ARGS[5] == "-S" && ARGS[7] == "-I")
#define RESTORE_CMD__WRITE_DATA_TO_MEM_F0(ORIGIN) {ORIGIN[0], ORIGIN[1], ORIGIN[2], ORIGIN[3], ORIGIN[4], "-F", "0", ORIGIN[5], ORIGIN[6], ORIGIN[7], ORIGIN[8]}

/* argc = 7 */

#define IS_WRITE_DATA_TO_MEM_F0_S0(ARGS) (ARGS[1] == "-P" && ARGS[2] == "W" && ARGS[3] == "-M" && IS_MEMORY_NAME(ARGS[4]) && ARGS[5] == "-I")
#define RESTORE_CMD__WRITE_DATA_TO_MEM_F0_S0(ORIGIN) {ORIGIN[0], ORIGIN[1], ORIGIN[2], ORIGIN[3], ORIGIN[4], "-F", "0", "-S", FILE_SIZE, ORIGIN[5], ORIGIN[6]}

namespace tool
{
    enum class _FPGA_OPERATION
    {
        LIST_DEVICE__ADC,
        LIST_DEVICE__DMA,
        LIST_DEVICE__CBUF,
        LIST_DEVICE__FIR,
        LIST_DEVICE__PDLY,

        DEVICE_REGISTER_OPERATION__ADC,
        DEVICE_REGISTER_OPERATION__DMA,
        DEVICE_REGISTER_OPERATION__CBUF,
        DEVICE_REGISTER_OPERATION__FIR,
        DEVICE_REGISTER_OPERATION__PDLY,

        MEMORY_OPERATION__DDR,
        MEMORY_OPERATION__FIR_BRAM,
        MEMORY_OPERATION__SG_BRAM,
        MEMORY_OPERATION__CBUF_BRAM,

        OPERATION_ERR,
        OPERATION_HELP
    };

    struct _FPGA_TOOL_CommandParseResult
    {
        _FPGA_OPERATION operation;
        int isread; /* true: is read, false: is write */
        bool transferSizeIfFilesize;
        uint32_t offset;       /* (in dev/memo) offset */
        uint32_t value;        /* (in dev) write value */
        uint32_t transfersize; /* (in mem only) transfer size */
        std::string file;      /* (in mem only) output & input file */
    };

    void _FPGA_TOOL_CommandParseResult_ToDefault(_FPGA_TOOL_CommandParseResult *result);

    void FPGA_TOOL_PrintHelp();
    void FPGA_TOOL_PrintErrorInfo();

    void FPGA_TOOL_PrintValue(uint32_t offset, uint32_t val);
    void FPGA_TOOL_PrintDeviceRegisters(const std::vector<std::string> &name, const std::vector<uint32_t> &offset, const std::vector<uint32_t> &val);

    void FPGA_TOOL_ParseCommand(const std::vector<std::string> &args, const std::vector<std::string> &argsLower, _FPGA_TOOL_CommandParseResult *result);
    void FPGA_TOOL_RestoreCommand(std::vector<std::string> *args);
}

#endif
