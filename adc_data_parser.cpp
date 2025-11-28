#include "fpga_data_parse.h"
#include "fpga_control.h"
#include "aligned_buffer.h"

void ADC_DATA_PARSER__PrintHelp();
void ADC_DATA_PARSER__ShowNeedHelp();

void ADC_DATA_PARSER__ShowNeedHelp()
{
std::cout << " \033[31mcommand/value error\033[0m\n" << std::endl;
printf(" see help: test_sampling --help\n");
printf("           test_sampling -h\n\n");
}

void ADC_DATA_PARSER__PrintHelp()
{
printf("\n");
printf(" |========================== [ ADC DATA PARSER ] ========================|\n");
printf(" |                                                                       |\n");
printf(" | ----- [ 1. For Help ] ----------------------------------------------- |\n");
printf(" |                                                                       |\n");
printf(" | [ \033[92mCOMMAND\033[0m ]                                                           |\n");
printf(" |                                                                       |\n");
printf(" | adc_data_parser -h                                                    |\n");
printf(" | adc_data_parser --help                                                |\n");
printf(" |                                                                       |\n");
printf(" | ----- [ 2. Sampling ] ----------------------------------------------- |\n");
printf(" |                                                                       |\n");
printf(" | [ \033[92mCOMMAND\033[0m ]                                                           |\n");
printf(" |                                                                       |\n");
printf(" | adc_data_parser -f <input file> -o <output file name>                 |\n");
printf(" |                                                                       |\n");
printf(" | [ \033[92mEXAMPLE\033[0m ]                                                           |\n");
printf(" |                                                                       |\n");
printf(" | ./adc_data_parser -f ./input.bin -o ./output.csv                      |\n");
printf(" |                                                                       |\n");
printf(" |=======================================================================|\n");
printf("\n");
}

int main(int argc, char *argv[])
{
    std::vector<std::string> args;
    std::string inputFilename, outputFilename;
    vuprs::FPGAConfigManager fpgaConfig;
    vuprs::SignalData signalData;
    vuprs::AlignedBufferServer buffer;
    args.resize(argc);

    args.resize(argc);
    for (int i = 0; i < argc; i++)
    {
        args[i] = std::string(argv[i]);
    }

    if (argc == 2)
    {
        if (args[1] == "-h" || args[1] == "--help")
        {
            ADC_DATA_PARSER__PrintHelp();
            return 0;
        }
        else
        {
            ADC_DATA_PARSER__ShowNeedHelp();
            return 0;
        }
    }
    else if (argc == 5)
    {
        if (args[1] == "-f" && args[3] == "-o")
        {
            inputFilename = args[2];
            outputFilename = args[4];
        }
        else
        {
            ADC_DATA_PARSER__ShowNeedHelp();
            return 0;
        }
    }
    else
    {
        ADC_DATA_PARSER__ShowNeedHelp();
        return 0;
    }

    /* Read input file */

    try
    {
        fpgaConfig.LoadFPGAConfigFromJson("./fpga_config.json");
        buffer.from_file(inputFilename);
        signalData = vuprs::BufferData2ADCChannels(&buffer, fpgaConfig.fpgaConfig.hardwareConfig.hardwareConfigADC);
        signalData.ToCSV(outputFilename);
    }
    catch(const std::exception& e)
    {
        printf("Error occurred when reading.\n");
    }
    printf("Done!\n");
}
