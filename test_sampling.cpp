#include "fpga_control.h"
#include "string_parse.h"

void TEST_SAMPLING__PrintHelp();
void TEST_SAMPLING__ShowNeedHelp();

void TEST_SAMPLING__PrintHelp()
{
printf("\n");
printf(" |========================== [ TEST SAMPLING ] ==========================|\n");
printf(" |                                                                       |\n");
printf(" | ----- [ 1. For Help ] ----------------------------------------------- |\n");
printf(" |                                                                       |\n");
printf(" | [ \033[92mCOMMAND\033[0m ]                                                           |\n");
printf(" |                                                                       |\n");
printf(" | test_sampling -h                                                      |\n");
printf(" | test_sampling --help                                                  |\n");
printf(" |                                                                       |\n");
printf(" | ----- [ 2. Sampling ] ----------------------------------------------- |\n");
printf(" |                                                                       |\n");
printf(" | [ \033[92mCOMMAND\033[0m ]                                                           |\n");
printf(" |                                                                       |\n");
printf(" | test_sampling --frame <frame> --points <points> -f <frequency>        |\n");
printf(" |                                                                       |\n");
printf(" |=======================================================================|\n");
printf("\n");
}

void TEST_SAMPLING__ShowNeedHelp()
{
std::cout << " \033[31mcommand/value error\033[0m\n" << std::endl;
printf(" see help: test_sampling --help\n");
printf("           test_sampling -h\n\n");
}

int main(int argc, char *argv[])
{
    vuprs::FPGAConfigManager config;
    vuprs::FPGAController controller;
    uint32_t readValue, writeValue = 0;
    uint64_t sf = 0, sp = 0, clockIncrement, length;
    double frequency = 0.0;
    std::vector<std::string> args;
    bool parseStatus = false;

    args.resize(argc);
    for (int i = 0; i < argc; i++)
    {
        args[i] = std::string(argv[i]);
    }

    if (argc == 2)
    {
        if (args[1] == "-h" || args[1] == "--help")
        {
            TEST_SAMPLING__PrintHelp();
            return 0;
        }
        else
        {
            TEST_SAMPLING__ShowNeedHelp();
            return 0;
        }
    }
    else if (argc == 7)
    {
        if (args[1] == "--frame" && args[3] == "--points" && args[5] == "-f")
        {
            sf = vuprs::ParseNumberFromString(args[2], &parseStatus);
            if (!parseStatus)
            {
                TEST_SAMPLING__ShowNeedHelp();
                return 0;
            }
            sp = vuprs::ParseNumberFromString(args[4], &parseStatus);
            if (!parseStatus)
            {
                TEST_SAMPLING__ShowNeedHelp();
                return 0;
            }
            frequency = vuprs::ParseDoubleFromString(args[6], &parseStatus);
            if (!parseStatus)
            {
                TEST_SAMPLING__ShowNeedHelp();
                return 0;
            }
            clockIncrement = vuprs::GetOptimalValueSCI(frequency);
        }
        else
        {
            TEST_SAMPLING__ShowNeedHelp();
            return 0;
        }
    }
    else
    {
        TEST_SAMPLING__ShowNeedHelp();
        return 0;
    }

    config.LoadFPGAConfigFromJson("./fpga_config.json");
    controller.LoadFPGAConfig(config);

    length = (18 * 4) * sf * sp;  /* in bytes */

    if ((length & 0x3FFFFFF) != length)
    {
        throw std::runtime_error("length to long.");
    }

    /* Reset DMA & ADC Controller */

    std::cout << "--- Reset ADC ---" << std::endl;

    controller.AXILite_WriteRegister(AXI_LITE_REGISTER__ADC__RST, 1);
    do {controller.AXILite_ReadRegister(AXI_LITE_REGISTER__ADC__STR, &readValue);} 
    while (!(readValue & 0x00000001));

    std::cout << "--- Reset DMA ---" << std::endl;

    controller.AXILite_ReadRegister(AXI_LITE_REGISTER__DMA__S2MM_DMACR, &readValue);

    writeValue = readValue | 0x00000004;

    controller.AXILite_WriteRegister(AXI_LITE_REGISTER__DMA__S2MM_DMACR, writeValue);

    do {controller.AXILite_ReadRegister(AXI_LITE_REGISTER__DMA__S2MM_DMASR, &readValue);} 
    while (!(readValue & 0x00000001));

    /* ADC configuration (not startup) */

    std::cout << "--- write ADC_SF ---" << std::endl;

    controller.AXILite_WriteRegister(AXI_LITE_REGISTER__ADC__SF, sf);

    controller.AXILite_ReadRegister(AXI_LITE_REGISTER__ADC__SF, &readValue);
    std::cout << "readback: " << vuprs::Number2HexString(readValue) << std::endl;

    std::cout << "--- write ADC_SP ---" << std::endl;

    controller.AXILite_WriteRegister(AXI_LITE_REGISTER__ADC__SP, sp);
    
    controller.AXILite_ReadRegister(AXI_LITE_REGISTER__ADC__SP, &readValue);
    std::cout << "readback: " << vuprs::Number2HexString(readValue) << std::endl;

    std::cout << "--- write ADC_SCI ---" << std::endl;

    controller.AXILite_WriteRegister(AXI_LITE_REGISTER__ADC__SCI, clockIncrement);

    controller.AXILite_ReadRegister(AXI_LITE_REGISTER__ADC__SCI, &readValue);
    std::cout << "readback: " << vuprs::Number2HexString(readValue) << std::endl;

    /* DMA configuration */
    
    /* Read S2MM_DMACR */

    std::cout << "--- read S2MM_DMACR ---" << std::endl;
    
    controller.AXILite_ReadRegister(AXI_LITE_REGISTER__DMA__S2MM_DMACR, &readValue);

    /* set S2MM_DMACR.RS = 1 */

    std::cout << "read S2MM_DMACR: " << vuprs::Number2HexString(readValue) << std::endl;

    writeValue = readValue;
    writeValue |= 0x00000001;

    std::cout << "--- write S2MM_DMACR ---" << std::endl;

    std::cout << "write S2MM_DMACR: " << vuprs::Number2HexString(writeValue) << std::endl;

    controller.AXILite_WriteRegister(AXI_LITE_REGISTER__DMA__S2MM_DMACR, writeValue);

    controller.AXILite_ReadRegister(AXI_LITE_REGISTER__DMA__S2MM_DMACR, &readValue);
    std::cout << "readback: " << vuprs::Number2HexString(readValue) << std::endl;

    /* Check DMA start */

    std::cout << "waiting for DMA start ..." << std::endl;

    do {controller.AXILite_ReadRegister(AXI_LITE_REGISTER__DMA__S2MM_DMASR, &readValue);} 
    while (readValue & 0x00000001);  /* Check S2MM_DMASR.halted */

    /* Set destination address DDR (0x00000000) */

    uint32_t destination_addr = 0x00000000;

    std::cout << "--- write S2MM_DA ---" << std::endl;

    controller.AXILite_WriteRegister(AXI_LITE_REGISTER__DMA__S2MM_DA, destination_addr);

    controller.AXILite_ReadRegister(AXI_LITE_REGISTER__DMA__S2MM_DA, &readValue);
    std::cout << "readback: " << vuprs::Number2HexString(readValue) << std::endl;

    /* Set DMA transfer size, and start DMA */

    std::cout << "--- write S2MM_LENGTH ---" << std::endl;

    controller.AXILite_WriteRegister(AXI_LITE_REGISTER__DMA__S2MM_LENGTH, length);

    controller.AXILite_ReadRegister(AXI_LITE_REGISTER__DMA__S2MM_LENGTH, &readValue);
    std::cout << "readback: " << vuprs::Number2HexString(readValue) << std::endl;

    usleep(1000);

    /* Start ADC */

    std::cout << "--- write ADC_STR ---" << std::endl;

    controller.AXILite_WriteRegister(AXI_LITE_REGISTER__ADC__STR, 1);

    controller.AXILite_ReadRegister(AXI_LITE_REGISTER__ADC__STR, &readValue);
    std::cout << "readback: " << vuprs::Number2HexString(readValue) << std::endl;

    return 0;
}
