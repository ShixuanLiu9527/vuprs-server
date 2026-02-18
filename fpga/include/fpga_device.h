#ifndef FPGA_DEVICE_H
#define FPGA_DEVICE_H

#include "fpga_module_template.h"

namespace vuprs
{

    /* ---------------------------------------------------------------------- */
    /* -------------------------------- AXI DMA ----------------------------- */
    /* ---------------------------------------------------------------------- */

    enum class AXI_DMA__Registers
    {
        SG_CTL,
        S2MM_DMACR,
        S2MM_DMASR,
        S2MM_CURDESC,
        S2MM_CURDESC_MSB,
        S2MM_TAILDESC,
        S2MM_TAILDESC_MSB,
        S2MM_DA,
        S2MM_DA_MSB,
        S2MM_LENGTH
    };

    class FPGA_Device__AXIDirectMemoryAccess: public FPGADeviceTemplate<AXI_DMA__Registers>
    {
        private:

            uint32_t offset_SG_CTL;
            uint32_t offset_S2MM_DMACR;
            uint32_t offset_S2MM_DMASR;
            uint32_t offset_S2MM_CURDESC;
            uint32_t offset_S2MM_CURDESC_MSB;
            uint32_t offset_S2MM_TAILDESC;
            uint32_t offset_S2MM_TAILDESC_MSB;
            uint32_t offset_S2MM_DA;
            uint32_t offset_S2MM_DA_MSB;
            uint32_t offset_S2MM_LENGTH;

        protected:

            void GenerateRegisterTable() override;

        public:

            FPGA_Device__AXIDirectMemoryAccess();
            bool LoadFromJsonObj(const nlohmann::json &obj) override;
    };

    /* ---------------------------------------------------------------------- */
    /* ---------------------------- ADC Controller -------------------------- */
    /* ---------------------------------------------------------------------- */

    enum class ADC_Controller__Registers
    {
        ADC_SCI,
        ADC_SP,
        ADC_SF,
        ADC_STR,
        ADC_NGF,
        ADC_ERR,
        ADC_RST,
        ADC_CS
    };

    class FPGA_Device__ADCController: public FPGADeviceTemplate<ADC_Controller__Registers>
    {
        private:

            double maxSamplingFrequencyHz;
            double voltageRangeRadiusV;
            double workClockFrequencyHz;

            double currentSamplingFrequency;
            uint32_t currentSCI;

            uint32_t offset_ADC_SCI;
            uint32_t offset_ADC_SP;
            uint32_t offset_ADC_SF;
            uint32_t offset_ADC_STR;
            uint32_t offset_ADC_NGF;
            uint32_t offset_ADC_ERR;
            uint32_t offset_ADC_RST;
            uint32_t offset_ADC_CS;

        protected:

            void GenerateRegisterTable() override;

        public:

            FPGA_Device__ADCController();
            bool LoadFromJsonObj(const nlohmann::json &obj) override;

            /**
             * @brief Get SCI value for certain sampling frequency.
             */
            uint32_t GetSCIValueForSamplingFrequency(double fs) const;

            /**
             * @brief Maximum sampling frequency.
             */
            double MaxSamplingFrequency() const;

            /**
             * @brief ADC work voltage (5.0 V or 10.0 V for AD7606)
             */
            double VoltageRangeRadius() const;

            /**
             * @brief Work frequency (e.g. 50000000 Hz)
             */
            double WorkFrequency() const;

            /**
             * @brief Current sampling frequency.
             */
            double CurrentSamplingFrequency() const;

            /**
             * @brief Convert SCI value to sampling frequency.
             */
            double SCI2FS(uint32_t SCI) const;

            /**
             * @brief Set SCI value.
             */
            void SetSCI(uint32_t SCI);
    };

    /* ---------------------------------------------------------------------- */
    /* --------------------------- Circular Buffer -------------------------- */
    /* ---------------------------------------------------------------------- */

    enum class Circular_Buffer__Registers
    {
        CBUF_FREEZE,
        CBUF_RST,
        CBUF_RS,
        CBUF_CBP
    };

    class FPGA_Device__CircularBuffer: public FPGADeviceTemplate<Circular_Buffer__Registers>
    {
        private:

            uint32_t signalPoints;

            uint32_t offset_CBUF_FREEZE;
            uint32_t offset_CBUF_RST;
            uint32_t offset_CBUF_RS;
            uint32_t offset_CBUF_CBP;

        protected:

            void GenerateRegisterTable() override;

        public:

            FPGA_Device__CircularBuffer();
            bool LoadFromJsonObj(const nlohmann::json &obj) override;

            uint32_t SignalPoints() const;
    };

    /* ---------------------------------------------------------------------- */
    /* --------------------------- FIR Filter Bank -------------------------- */
    /* ---------------------------------------------------------------------- */

    enum class FIR_Filter_Bank__Registers
    {
        FIR_RST,
        FIR_U_FIR_LEN,
        FIR_U_FIR_COEF,
        FIR_LEN,
        FIR_COEF_SCALE,
        FIR_RSC,
        FIR_RS,
        FIR_MAX_LEN
    };

    class FPGA_Device__FIRFilterBank: public FPGADeviceTemplate<FIR_Filter_Bank__Registers>
    {
        private:

        uint32_t offset_FIR_RST;
        uint32_t offset_FIR_U_FIR_LEN;
        uint32_t offset_FIR_U_FIR_COEF;
        uint32_t offset_FIR_LEN;
        uint32_t offset_FIR_COEF_SCALE;
        uint32_t offset_FIR_RSC;
        uint32_t offset_FIR_RS;
        uint32_t offset_FIR_MAX_LEN;

        protected:

            void GenerateRegisterTable() override;

        public:

            FPGA_Device__FIRFilterBank();
            bool LoadFromJsonObj(const nlohmann::json &obj) override;
    };

    /* ---------------------------------------------------------------------- */
    /* ---------------------------- Pre-delay Unit -------------------------- */
    /* ---------------------------------------------------------------------- */

    enum class PreDelay_Unit__Registers
    {
        PREDLY_CH1_CH2,
        PREDLY_CH3_CH4,
        PREDLY_CH5_CH6,
        PREDLY_CH7_CH8,
        PREDLY_CH9_CH10,
        PREDLY_CH11_CH12,
        PREDLY_CH13_CH14,
        PREDLY_CH15_CH16,
        PREDLY_FREEZE,
        PREDLY_RST,
        PREDLY_RS,
        PREDLY_MAX_DLY
    };

    class FPGA_Device__PreDelayUnit: public FPGADeviceTemplate<PreDelay_Unit__Registers>
    {
        private:

        uint32_t offset_PREDLY_CH1_CH2;
        uint32_t offset_PREDLY_CH3_CH4;
        uint32_t offset_PREDLY_CH5_CH6;
        uint32_t offset_PREDLY_CH7_CH8;
        uint32_t offset_PREDLY_CH9_CH10;
        uint32_t offset_PREDLY_CH11_CH12;
        uint32_t offset_PREDLY_CH13_CH14;
        uint32_t offset_PREDLY_CH15_CH16;
        uint32_t offset_PREDLY_FREEZE;
        uint32_t offset_PREDLY_RST;
        uint32_t offset_PREDLY_RS;
        uint32_t offset_PREDLY_MAX_DLY;

        protected:

            void GenerateRegisterTable() override;

        public:

            FPGA_Device__PreDelayUnit();
            bool LoadFromJsonObj(const nlohmann::json &obj) override;
    };
}

#endif
