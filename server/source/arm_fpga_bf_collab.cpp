#include "arm_fpga_bf_collab.h"

vuprs::ARM_FPGA_CollaborationBeamfomer::ARM_FPGA_CollaborationBeamfomer()
{

}

vuprs::ARM_FPGA_CollaborationBeamfomer::~ARM_FPGA_CollaborationBeamfomer()
{

}

bool vuprs::ARM_FPGA_CollaborationBeamfomer::InitCollaborationBeamfomer(const std::string &fpgaConfigJson, const std::string &bfArrayConfigJson)
{
    bool operateStatus = true;
    try
    {
        operateStatus &= this->controller.ConfigFPGAFromJson(fpgaConfigJson);
        operateStatus &= this->bf_dcrcb.ConfigArrayFromJson(bfArrayConfigJson);
        operateStatus &= this->bf_cbf.ConfigArrayFromJson(bfArrayConfigJson);
    }
    catch(const std::exception& e)
    {
        throw std::runtime_error("Error occurred in initialization.");
    }

    return operateStatus;
}

void vuprs::ARM_FPGA_CollaborationBeamfomer::InitHardwareBeamformer()
{
    vuprs::FPGA_API__ADC__ResetADC(&this->controller);  /* Reset ADC controller */
}

void vuprs::ARM_FPGA_CollaborationBeamfomer::StartBeamforming(double fs)
{

}
