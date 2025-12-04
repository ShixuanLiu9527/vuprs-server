#include "fpga_control.h"
#include "string_parse.h"
#include "fpga_data_parse.h"
#include "signal_data.h"

#define DMA_LENGTH_EXPAND_BYTES 4096U

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
    vuprs::AlignedBufferDMA buffer;
    vuprs::DMATransferConfig dmaTransferConfig;
    vuprs::SignalData signalData;

    uint32_t readValue, writeValue = 0;
    uint64_t sf = 0, sp = 0, clockIncrement, lengthInBytes;
    double frequency = 0.0;
    std::vector<std::string> args;
    bool parseStatus = false;

    /* Parse command */

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

    /* FPGA Control */

    config.LoadFPGAConfigFromJson("./fpga_config.json");
    controller.LoadFPGAConfig(config);

    lengthInBytes = (ADC_FRAME_WORD_LENGTH * 4) * sf * sp;  /* in bytes */

    if ((lengthInBytes & 0x3FFFFFF) != lengthInBytes)
    {
        throw std::runtime_error("length too long.");
    }

    /* Reset DMA & ADC Controller */

    printf("[ reset ADC controller, write \033[92m1\033[0m to \033[33mADC_RST\033[0m ]\n");

    controller.AXILite_WriteRegister(AXI_LITE_REGISTER__ADC__RST, 1);
    do 
    {
        controller.AXILite_ReadRegister(AXI_LITE_REGISTER__ADC__STR, &readValue);
        usleep(1000);
    } 
    while (!(readValue & 0x00000001));

    printf("[ reset DMA controller ]\n");

    controller.AXILite_ReadRegister(AXI_LITE_REGISTER__DMA__S2MM_DMACR, &readValue);

    writeValue = readValue | 0x00000004;

    controller.AXILite_WriteRegister(AXI_LITE_REGISTER__DMA__S2MM_DMACR, writeValue);

    do 
    {
        controller.AXILite_ReadRegister(AXI_LITE_REGISTER__DMA__S2MM_DMASR, &readValue);
        usleep(1000);
    } 
    while (!(readValue & 0x00000001));

    /* ADC configuration (not startup) */

    printf("[ write \033[92m%s\033[0m to ADC controller register: \033[33mADC_SF\033[0m ]\n", vuprs::Number2HexString(sf).c_str());
    
    controller.AXILite_WriteRegister(AXI_LITE_REGISTER__ADC__SF, sf);

    controller.AXILite_ReadRegister(AXI_LITE_REGISTER__ADC__SF, &readValue);
    printf("    readback back from \033[33mADC_SF\033[0m: \033[92m%s\033[0m\n", vuprs::Number2HexString(readValue).c_str());

    printf("[ write \033[92m%s\033[0m to ADC controller register: \033[33mADC_SP\033[0m ]\n", vuprs::Number2HexString(sp).c_str());
    
    controller.AXILite_WriteRegister(AXI_LITE_REGISTER__ADC__SP, sp);
    
    controller.AXILite_ReadRegister(AXI_LITE_REGISTER__ADC__SP, &readValue);
    printf("    readback back from \033[33mADC_SP\033[0m: \033[92m%s\033[0m\n", vuprs::Number2HexString(readValue).c_str());

    printf("[ write \033[92m%s\033[0m to ADC controller register: \033[33mADC_SCI\033[0m ]\n", vuprs::Number2HexString(clockIncrement).c_str());
    
    controller.AXILite_WriteRegister(AXI_LITE_REGISTER__ADC__SCI, clockIncrement);

    controller.AXILite_ReadRegister(AXI_LITE_REGISTER__ADC__SCI, &readValue);
    printf("    readback back from \033[33mADC_SCI\033[0m: \033[92m%s\033[0m\n", vuprs::Number2HexString(readValue).c_str());

    /* DMA configuration */
    
    /* Read S2MM_DMACR */

    printf("[ set \033[33mS2MM_DMACR.RS\033[0m to \033[92m1\033[0m ]\n");
    
    controller.AXILite_ReadRegister(AXI_LITE_REGISTER__DMA__S2MM_DMACR, &readValue);
    printf("    read \033[33mS2MM_DMACR\033[0m: \033[92m%s\033[0m\n", vuprs::Number2HexString(readValue).c_str());

    /* set S2MM_DMACR.RS = 1 */

    writeValue = readValue;
    writeValue |= 0x00000001;

    printf("    write \033[92m%s\033[0m to \033[33mS2MM_DMACR\033[0m\n", vuprs::Number2HexString(writeValue).c_str());
    
    controller.AXILite_WriteRegister(AXI_LITE_REGISTER__DMA__S2MM_DMACR, writeValue);

    controller.AXILite_ReadRegister(AXI_LITE_REGISTER__DMA__S2MM_DMACR, &readValue);
    printf("    readback back from \033[33mS2MM_DMACR\033[0m: \033[92m%s\033[0m\n", vuprs::Number2HexString(readValue).c_str());

    /* Check DMA start */

    printf("[ waiting for DMA start ... ]\n");
    
    do 
    {
        controller.AXILite_ReadRegister(AXI_LITE_REGISTER__DMA__S2MM_DMASR, &readValue);
        usleep(1000);
    } 
    while (readValue & 0x00000001);  /* Check S2MM_DMASR.halted */

    /* Set destination address DDR (0x00000000) */

    uint32_t destination_addr = 0x00000000;

    printf("[ write \033[92m%s\033[0m to DMA controller register: \033[33mS2MM_DA\033[0m ]\n", vuprs::Number2HexString(destination_addr).c_str());

    controller.AXILite_WriteRegister(AXI_LITE_REGISTER__DMA__S2MM_DA, destination_addr);

    controller.AXILite_ReadRegister(AXI_LITE_REGISTER__DMA__S2MM_DA, &readValue);
    printf("    readback back from \033[33mS2MM_DA\033[0m: \033[92m%s\033[0m\n", vuprs::Number2HexString(readValue).c_str());

    /* Set DMA transfer size, and start DMA */

    printf("[ write \033[92m%s\033[0m to DMA controller register: \033[33mS2MM_LENGTH\033[0m ]\n", vuprs::Number2HexString(lengthInBytes + DMA_LENGTH_EXPAND_BYTES).c_str());
    
    controller.AXILite_WriteRegister(AXI_LITE_REGISTER__DMA__S2MM_LENGTH, lengthInBytes + DMA_LENGTH_EXPAND_BYTES);

    controller.AXILite_ReadRegister(AXI_LITE_REGISTER__DMA__S2MM_LENGTH, &readValue);
    printf("    readback back from \033[33mS2MM_LENGTH\033[0m: \033[92m%s\033[0m\n", vuprs::Number2HexString(readValue).c_str());

    usleep(1000);

    /* Start ADC */

    printf("[ write \033[92m1\033[0m to ADC controller register: \033[33mADC_STR\033[0m ]\n");
    
    controller.AXILite_WriteRegister(AXI_LITE_REGISTER__ADC__STR, 1);

    controller.AXILite_ReadRegister(AXI_LITE_REGISTER__ADC__STR, &readValue);
    printf("    readback back from \033[33mADC_STR\033[0m: \033[92m%s\033[0m\n", vuprs::Number2HexString(readValue).c_str());

    /* Wait for sampling complete */

    printf("[ waiting for sampling complete (about %f ms) ... ]\n", (double)sp * (double)sf * 1000.0 / frequency);

    do 
    {
        controller.AXILite_ReadRegister(AXI_LITE_REGISTER__ADC__STR, &readValue);
        usleep(1000);
    } 
    while ((readValue & 0x00000001) == 0);

    printf("[ sampling complete! ]\n");

    std::string outputFile = "./adc_output.bin", outputCSV = "./adc_output.csv";

    std::cout << "Save raw sampling data to file: " << outputFile << std::endl;

    vuprs::SetDMATransferConfigToDefault(&dmaTransferConfig);
    dmaTransferConfig.dmaChannel = 0;
    dmaTransferConfig.offset = 0;
    dmaTransferConfig.transferByteSize = lengthInBytes + DMA_LENGTH_EXPAND_BYTES;
    dmaTransferConfig.transferDirectionSelection = DMA_TRANSFER_DIRECTION__FPGA_TO_HOST;
    dmaTransferConfig.transferMemorySelection = DMA_TRANSFER_MEMORY_SELECTION__DDR;

    if (controller.AXIFull_BufferTransfer(dmaTransferConfig, &buffer))
    {
        buffer.to_file(outputFile);
        printf("save success.\n");
    }
    else
    {
        printf("save failed.\n");
    }

    std::cout << "Save CSV data to file: " << outputCSV << std::endl;

    bool status = false;

    signalData = vuprs::BufferData2ADCChannels(&buffer, config.fpgaConfig.hardwareConfig.hardwareConfigADC, frequency, &status);

    if (status)
    {
        signalData.ToCSV(outputCSV);
        printf("save success.\n");
    }
    else
    {
        printf("save failed.\n");
    }

    return 0;
}
