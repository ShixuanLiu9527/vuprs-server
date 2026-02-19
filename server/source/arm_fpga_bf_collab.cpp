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

bool vuprs::ARM_FPGA_CollaborationBeamfomer::ResetHardwareBeamformer()
{
    bool retval = true;

    retval &= vuprs::FPGA_API__ADC__ResetADC(&this->controller);  /* Reset ADC controller */
    retval &= vuprs::FPGA_API__CBUF__ResetCircularBuffer(&this->controller);  /* Reset Circular Buffer */
    retval &= vuprs::FPGA_API__FIR__ResetFIR(&this->controller);  /* Reset FIR Filter Bank */
    retval &= vuprs::FPGA_API__DMA__ResetDMA(&this->controller);  /* Reset AXI DMA */

    return retval;
}

bool vuprs::ARM_FPGA_CollaborationBeamfomer::StartBeamforming(const ARM_FPGA_BF_Config &config)
{
    bool retval = true;

    /* Generate descriptors */
}
