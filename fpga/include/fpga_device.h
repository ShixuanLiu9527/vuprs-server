#ifndef FPGA_DEVICE_H
#define FPGA_DEVICE_H

#include "fpga_module_template.h"

namespace vuprs
{

    /* ---------------------------------------------------------------------- */
    /* -------------------------------- AXI DMA ----------------------------- */
    /* ---------------------------------------------------------------------- */

    #pragma pack(push, 1)

    /**
     * @brief AXI DMA Scatter/Gather Descriptor.
     */
    struct AXI_DMA_ScatterGatherDescriptor
    {
        uint32_t NXTDESC;  /* 00h: Next Descriptor Pointer (16-word aligned), for S2MM, [5:0]: Reserved (should be 0), [31:6]: Indicates the lower order pointer pointing to the first word of the next descriptor. */
        uint32_t NXTDESC_MSB;  /* 04h: Upper 32 bits of Next Descriptor Pointer (16-word aligned), [31:0]: Indicates the MSB 32 bits of the pointer pointing to the first word of the next descriptor. */
        uint32_t BUFFER_ADDRESS;  /* 08h: Buffer Address, [31:0]: Provides the location of the buffer space available to store data transferred from Stream to Memory Map. */
        uint32_t BUFFER_ADDRESS_MSB;  /* 0Ch: Upper 32 bits of Buffer Address, [31:0]: Provides the MSB 32 bits of the location of the buffer space available to store data transferred from Stream to Memory Map. */
        uint32_t RESERVED_0;  /* 10h: N/A, [31:0]: Reserved (should be 0) */
        uint32_t RESERVED_1;  /* 14h: N/A, [31:0]: Reserved (should be 0) */
        uint32_t CONTROL;  /* 18h: Control, [31:28]: Reserved (should be 0), [27]: RXSOF, [26]: Receive End Of Frame, [25:0]: Buffer Length */
        uint32_t STATUS;  /* 1Ch: Status, [25:0]: Transferred Bytes, [26]: RXEOF, [27]: RXSOF, [28]: DMAIntErr, [29]: DMASlvErr, [30]: DMADecErr, [31]: Cmplt */

        uint32_t APP0;  /* 20h: User Application Field 0 */
        uint32_t APP1;  /* 24h: User Application Field 1 */
        uint32_t APP2;  /* 28h: User Application Field 2 */
        uint32_t APP3;  /* 2Ch: User Application Field 3 */
        uint32_t APP4;  /* 30h: User Application Field 4 */

        uint32_t ALIGNMENT_0_CURRENT_ADDR;  /* Address of this descriptor, to aligned to 16-word */
        uint32_t ALIGNMENT_1;  /* to aligned to 16-word */
        uint32_t ALIGNMENT_2;  /* to aligned to 16-word */
    };

    #pragma pack(pop)

    constexpr uint32_t DMA_DESCRIPTOR_ALIGNMENT_16_WORD = (uint32_t)(16 * sizeof(uint32_t));  /* 16-word alignment */
    constexpr uint32_t DMA_BUFFER_ALIGNMENT_1_WORD = (uint32_t)(1 * sizeof(uint32_t));  /* 1-word alignment */
    
    constexpr uint32_t DMA_MAX_BUFFER_LENGTH = 0x3FF'FFFF;
    constexpr uint32_t DMA_BUFFER_LENGTH_MASK = 0x3FF'FFFF;

    static_assert(sizeof(AXI_DMA_ScatterGatherDescriptor) == 64, "AXI DMA Descriptor must be 52 bytes");

    /**
     * @brief Set AXI DMA Scatter/Gather Descriptor to default value (0).
     */
    void AXI_DMA_ScatterGatherDescriptor_ToDefault(vuprs::AXI_DMA_ScatterGatherDescriptor *descriptor);
    
    /**
     * @brief Create AXI DMA Scatter/Gather Descriptor chain.
     * 
     * @note Address of descriptors will be 16-word aligned.
     * @note Address of buffer will be 1-word aligned.
     * 
     * @param descriptorList target descriptor list.
     * @param bufferSize buffer size of each descriptor (should be aligned to 1-word).
     * @param bufferCount buffer count (descriptor count).
     * @param ddrBaseAddr DDR base address in FPGA AXI-Full network.
     * @param isCyclicMode true: cyclic DMA mode, false: normal mode.
     * 
     * @throw std::runtime_error
     */
    void CreateDMAScatterGatherDescriptorChain(std::vector<vuprs::AXI_DMA_ScatterGatherDescriptor> *descriptorList, 
        uint32_t bufferSize, uint32_t bufferCount, uint32_t ddrBaseAddr, bool isCyclicMode = false);

    enum class AXI_DMA__Registers
    {
        SG_CTL,  /* [3:0]: SG_CACHE, [7:4]: Reserved, [11:8]: SG_USER, [31:12]: Reserved */
        S2MM_DMACR,  /* [0]: RS, [1]: Reserved, [2]: Reset, [3]: Keyhole, [4]: Cyclic BD Enable, [11:5]: Reserved, [31:12]: Interrupt flags */
        S2MM_DMASR,  /* [0]: Halted, [1]: Idle, [2]: Reserved, [3]: SGIncld, [31:4]: Interrupt & error flags */
        S2MM_CURDESC,  /* [5:0]: Reserved, [31:6]: Current Descriptor Pointer (26 bits) */
        S2MM_CURDESC_MSB,  /* [31:0]: Current Descriptor Pointer */
        S2MM_TAILDESC,  /* [5:0]: Reserved, [31:6]: Tail Descriptor Pointer (26 bits) */
        S2MM_TAILDESC_MSB,  /* [31:0]: Tail Descriptor Pointer */
        S2MM_DA,  /* [31:0]: Destination Address */
        S2MM_DA_MSB,  /* [31:0]: Destination Address */
        S2MM_LENGTH  /* [25:0]: Length, [31:26]: Reserved */
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
             * @brief Convert SCI value to sampling frequency.
             */
            double SCI2FS(uint32_t SCI) const;
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
