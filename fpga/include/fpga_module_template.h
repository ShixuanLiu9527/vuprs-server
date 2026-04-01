#ifndef FPGA_MODULE_TEMPLATE_H
#define FPGA_MODULE_TEMPLATE_H

#include "nlohmann/json.hpp"
#include "string_parse.h"
#include "aligned_buffer.h"
#include "fpga_io_manager.h"

#define _IS_CODING_MODE false  /* should be false before compilering */
#if _IS_CODING_MODE
    #error "In coding mode."
#endif

#define __MEGABYTES__                             (1024U * 1024U);
#define __KILOBYTES__                             1024U

#define __LINUX_DMA_MAX_TRANSFER_BYTES__          0x7ffff000  /* Maximum transfer size in Linux-32bit & Linux-64bit */

#define FPGA_REG_BIT(REG, BIT) (((REG) >> (BIT)) & 1U)
#define FPGA_CLEAR_REG_BIT(REG, BIT) ((uint32_t)((REG) & ~(1U << (BIT))))
#define FPGA_SET_REG_BIT(REG, BIT) ((uint32_t)((REG) | (1U << (BIT))))

namespace vuprs
{
    enum class FPGABus
    {
        AXI_LITE,
        AXI_FULL,
        AXI_STREAM
    };

    template<typename REG_SEL_ENUM>
    struct _DeviceRegisterConfig
    {
        uint32_t *offsetVal;
        std::string name;
        REG_SEL_ENUM enumVal;
    };

#if _IS_CODING_MODE
    enum class REG_SEL_ENUM  /* for coding */
    {
        REG_A, REG_B, REG_C
    };
#endif

    /**
     * @brief FPGA Device Template (Abstract Base Class).
     * 
     * @note TODO in subclass
     * @note 1. Define enum REG_SEL_ENUM;
     * @note 2. Add all register offset attribute (uint32_t);
     * @note 3. Complete method GenerateRegisterTable();
     * @note 4. Complete method LoadFromJsonObj();
     * @note 5. Call GenerateRegisterTable() and SetRegisterOffsetDefault() in constructor of subclass.
     * 
     * @note USAGE in subclass:
     * @note 1. Generate object;
     * @note 2. Call method LoadFromJsonObj();
     * @note 3. Call method BindFPGAFileManager();
     * @note 4. Read/Write registers.
     * 
     * @note Thread safety.
     */
#if !_IS_CODING_MODE
    template<typename REG_SEL_ENUM>
#endif
    class FPGADeviceTemplate
    {
        private:

            std::atomic<bool> isIOManagerBind;
            std::atomic<bool> isIOManagerBind_Interrupt;

            std::weak_ptr<vuprs::FPGA_IOManagerForDevice> bindIOManager_Device;
            std::weak_ptr<vuprs::FPGA_IOManagerForInterrput> bindIOManager_Interrput;
            
            std::string controlDeviceFilename;
            std::string eventDeviceFilename;

            bool RegisterIO(uint32_t registerAddress, uint32_t* ioValue, bool isRead)
            {
                /* Security Check */

                if (!this->isIOManagerBind) 
                {
                    throw std::runtime_error("in [FPGADeviceTemplate::RegisterIO] FPGA file manager is NULL.");
                }

                std::shared_ptr<vuprs::FPGA_IOManagerForDevice> manager;

                {
                    std::unique_lock<std::mutex> lock(this->mut_dev);  /* LOCK */
                    manager = this->bindIOManager_Device.lock();
                }

                if (!manager) return false;

                return manager->RegisterIO(ioValue, registerAddress, isRead);
            }

            bool RegisterIO(const std::vector<REG_SEL_ENUM> &mulRegisterSelection, std::vector<uint32_t> *ioValue, bool isRead)
            {
                /* Security Check */

                int registerNumber = mulRegisterSelection.size();

                if (!this->isIOManagerBind) 
                {
                    throw std::runtime_error("in [FPGADeviceTemplate::RegisterIO] FPGA file manager is NULL."); 
                }
                if (registerNumber <= 0)
                {
                    throw std::runtime_error("in [FPGADeviceTemplate::RegisterIO] No registers read or written."); 
                }
                if (!isRead && ioValue->size() != registerNumber) 
                {
                    throw std::runtime_error("in [FPGADeviceTemplate::RegisterIO] mulReadValue.size() != register count to read.");
                }

                std::shared_ptr<vuprs::FPGA_IOManagerForDevice> manager;

                {
                    std::unique_lock<std::mutex> lock(this->mut_dev);  /* LOCK */
                    manager = this->bindIOManager_Device.lock();
                }

                if (!manager) return false;
                
                /* Operation */

                std::vector<uint32_t> multiRegisterAddressOffset;

                multiRegisterAddressOffset.resize(registerNumber);

                for (int i = 0; i < registerNumber; i++)
                {
                    multiRegisterAddressOffset[i] = this->GetRegisterAbsoluteAddress(mulRegisterSelection[i]);
                }

                return manager->RegisterListIO(ioValue, multiRegisterAddressOffset, isRead);
            }

            bool EventIO(uint32_t *readValue)
            {
                if (!this->isIOManagerBind_Interrupt) 
                {
                    throw std::runtime_error("in [FPGADeviceTemplate::EventIO] FPGA event file manager is NULL.");
                }

                std::shared_ptr<vuprs::FPGA_IOManagerForInterrput> manager;
                
                {
                    std::unique_lock<std::mutex> lock(this->mut_event);  /* LOCK */
                    manager = this->bindIOManager_Interrput.lock();
                }

                if (!manager) return false;

                return manager->ReadEvent(readValue);
            }
            
        protected:

            FPGABus controlBus;
            FPGABus dataBus;
            
            std::atomic<uint32_t> fpgaAddress;
            std::atomic<uint32_t> barOffset;

            std::vector<vuprs::_DeviceRegisterConfig<REG_SEL_ENUM>> registerTable;

            mutable std::mutex mut;  /* Global lock */
            mutable std::mutex mut_dev;  /* Device IO manager lock */
            mutable std::mutex mut_event;  /* Event IO manager lock */

            std::atomic<bool> configdone;

            virtual void GenerateRegisterTable() = 0;

            /**
             * @brief Set all register offset to 0.
             */
            void SetRegisterOffsetDefault()
            {
                std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */

                int registerNumber = this->registerTable.size();
                for (int i = 0; i < registerNumber; i++)
                {
                    *(this->registerTable[i].offsetVal) = 0;
                }
            }

            bool LoadMainInfoFromJsonObj(const nlohmann::json &obj)
            {
                /* Security Check */

                if (!obj.contains("register-offset")) 
                {
                    throw std::runtime_error("in [FPGADeviceTemplate::LoadMainInfoFromJsonObj] register-offset not found.");
                }
                
                /* Operation */

                {
                    std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */

                    int registerNumber = this->registerTable.size();
                
                    auto registers = obj["register-offset"];
                    for (int i = 0; i < registerNumber; i++)
                    {
                        vuprs::__JsonStringParseINT<uint32_t>(this->registerTable[i].offsetVal, registers, this->registerTable[i].name, true);
                    }
                
                    /* Base Address */

                    uint32_t fpgaAddress, barOffset;
                
                    vuprs::__JsonStringParseINT<uint32_t>(&fpgaAddress, obj, "fpga-address", true);
                    vuprs::__JsonStringParseINT<uint32_t>(&barOffset, obj, "bar-offset", true);

                    this->fpgaAddress = fpgaAddress;
                    this->barOffset = barOffset;

                    vuprs::__JsonParseString(&this->controlDeviceFilename, obj, "control-device-file", true);
                    vuprs::__JsonParseString(&this->eventDeviceFilename, obj, "event-device-file", false);  /* event is not required for all devices */
                }

                return true;
            }

        public:

            FPGADeviceTemplate(const FPGADeviceTemplate&) = delete;
            FPGADeviceTemplate(FPGADeviceTemplate&&) = delete;

            FPGADeviceTemplate& operator=(const FPGADeviceTemplate&) = delete;
            FPGADeviceTemplate& operator=(FPGADeviceTemplate&&) = delete;

            FPGADeviceTemplate()
            {
                {
                    std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */

                    this->controlBus = vuprs::FPGABus::AXI_LITE;
                    this->dataBus = vuprs::FPGABus::AXI_STREAM;
                    this->fpgaAddress = 0;
                    this->barOffset = 0;
                    this->configdone = false;
                    this->registerTable.clear();

                    this->isIOManagerBind = false;
                    this->isIOManagerBind_Interrupt = false;
                }
                {
                    std::unique_lock<std::mutex> lock(this->mut_dev);  /* LOCK */
                    this->bindIOManager_Device.reset();
                }
                {
                    std::unique_lock<std::mutex> lock(this->mut_event);  /* LOCK */
                    this->bindIOManager_Interrput.reset();
                }
            }

            virtual ~FPGADeviceTemplate() {}
            virtual bool LoadFromJsonObj(const nlohmann::json &obj) = 0;

            std::string ControlDeviceFilename() const
            {
                std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */
                return this->controlDeviceFilename;
            }

            std::string EventDeviceFilename() const
            {
                std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */
                return this->eventDeviceFilename;
            }

            /**
             * @brief Bind FPGA file manager.
             */
            bool BindFPGAFileManager(std::shared_ptr<vuprs::FPGA_IOManagerForDevice> ioManager)
            {
                if (!ioManager) 
                {
                    return false;
                }

                std::shared_ptr<vuprs::FPGA_IOManagerForDevice> manager;

                {
                    std::unique_lock<std::mutex> lock(this->mut_dev);  /* LOCK */
                    manager = this->bindIOManager_Device.lock();
                }

                if (manager && manager != ioManager) 
                {
                    this->UnbindFileManager();
                }
                this->isIOManagerBind = true;

                {
                    std::unique_lock<std::mutex> lock(this->mut_dev);  /* LOCK */
                    this->bindIOManager_Device = ioManager;
                }

                return true;
            }

            /**
             * @brief Bind FPGA file manager (for interrupt).
             */
            bool BindFPGAFileManager_Interrupt(std::shared_ptr<vuprs::FPGA_IOManagerForInterrput> ioManager_interrupt)
            {
                if (!ioManager_interrupt) 
                {
                    return false;
                }

                std::shared_ptr<vuprs::FPGA_IOManagerForInterrput> manager;

                {
                    std::unique_lock<std::mutex> lock(this->mut_event);  /* LOCK */
                    manager = this->bindIOManager_Interrput.lock();
                }

                if (manager && manager != ioManager_interrupt) 
                {
                    this->UnbindFileManager();
                }
                this->isIOManagerBind_Interrupt = true;

                {
                    std::unique_lock<std::mutex> lock(this->mut_event);  /* LOCK */
                    this->bindIOManager_Interrput = ioManager_interrupt;
                }

                return true;
            }
    
            /**
             * @brief Unbind FPGA file manager.
             */
            void UnbindFileManager()
            {
                {
                    std::unique_lock<std::mutex> lock(this->mut_dev);  /* LOCK */
                    this->bindIOManager_Device.reset();
                }
                this->isIOManagerBind = false;
            }

            /**
             * @brief Unbind FPGA file manager (for interrupt).
             */
            void UnbindFileManager_Interrupt()
            {
                {
                    std::unique_lock<std::mutex> lock(this->mut_event);  /* LOCK */
                    this->bindIOManager_Interrput.reset();
                }
                this->isIOManagerBind_Interrupt = false;
            }

            /**
             * @brief Get absolute address of register (= device bar address + register offset).
             */
            uint32_t GetRegisterAbsoluteAddress(REG_SEL_ENUM registerSelection)
            {
                /* Security Check */

                if (!this->configdone) 
                {
                    throw std::runtime_error("in [FPGADeviceTemplate::GetRegisterAbsoluteAddress] Config not complete.");
                }

                /* Operation */

                uint32_t registerOffset = 0;
                bool registerFound = false;
                int registerNumber;

                {
                    std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */

                    registerNumber = this->registerTable.size();
                    for (int i = 0; i < registerNumber; i++)
                    {
                        if (registerSelection == this->registerTable[i].enumVal)
                        {
                            registerOffset = *(this->registerTable[i].offsetVal);
                            registerFound = true;
                            break;
                        }
                    }
                }

                if (!registerFound) 
                {
                    throw std::runtime_error("in [FPGADeviceTemplate::GetRegisterAbsoluteAddress] Invalid register selection.");
                }

                return registerOffset + this->barOffset;
            }

            /* ------------------------------ Single Register IO ------------------------------ */

            /**
             * @brief Read single register (use register selection).
             * 
             * @param registerSelection register to read.
             * @param readValue read value pointer.
             * 
             * @retval true: success, false: failed.
             */
            bool ReadSingleRegister(REG_SEL_ENUM registerSelection, uint32_t *readValue)
            {
                uint32_t registerAddress = this->GetRegisterAbsoluteAddress(registerSelection);
                return this->RegisterIO(registerAddress, readValue, true);
            }

            /**
             * @brief Read single register (use offset).
             * 
             * @param offset register offset (must aligned to 4 bytes).
             * @param readValue read value pointer.
             * 
             * @retval true: success, false: failed.
             */
            bool ReadSingleRegister(uint32_t offset, uint32_t *readValue)
            {
                if (offset % sizeof(uint32_t) != 0)
                {
                    throw std::runtime_error("in [FPGADeviceTemplate::ReadSingleRegister] Offset must aligned to 4 bytes.");
                }
                uint32_t registerAddress = offset + this->barOffset;
                return this->RegisterIO(registerAddress, readValue, true);
            }

            /**
             * @brief Write single register (use register selection).
             * 
             * @param registerSelection register to write.
             * @param writeValue write value.
             * 
             * @retval true: success, false: failed.
             */
            bool WriteSingleRegister(REG_SEL_ENUM registerSelection, uint32_t writeValue)
            {
                uint32_t registerAddress = this->GetRegisterAbsoluteAddress(registerSelection);
                return this->RegisterIO(registerAddress, &writeValue, false);
            }

            /**
             * @brief Write single register (use offset).
             * 
             * @param offset register offset (must aligned to 4 bytes).
             * @param writeValue write value.
             * 
             * @retval true: success, false: failed.
             */
            bool WriteSingleRegister(uint32_t offset, uint32_t writeValue)
            {
                if (offset % sizeof(uint32_t) != 0)
                {
                    throw std::runtime_error("in [FPGADeviceTemplate::WriteSingleRegister] Offset must aligned to 4 bytes.");
                }
                uint32_t registerAddress = offset + this->barOffset;
                return this->RegisterIO(registerAddress, &writeValue, false);
            }

            /**
             * @brief Read one bit of certain register.
             * 
             * @param registerSelection register to operate.
             * @param bit bit select (valid value: 0, 1, ..., 31).
             * @param value read result.
             * 
             * @retval true: success, false: failed.
             */
            bool ReadSingleRegisterBIT(REG_SEL_ENUM registerSelection, uint32_t bit, uint32_t *value)
            {
                uint32_t registerAddress = this->GetRegisterAbsoluteAddress(registerSelection);
                return this->ReadSingleRegisterBIT(registerAddress, bit, value);
            }

            /**
             * @brief Read one bit of certain register.
             * 
             * @param registerAddress register address.
             * @param bit bit select (valid value: 0, 1, ..., 31).
             * @param value read result.
             * 
             * @retval true: success, false: failed.
             */
            bool ReadSingleRegisterBIT(uint32_t registerAddress, uint32_t bit, uint32_t *value)
            {
                if (bit > 31)
                {
                    throw std::runtime_error("in [FPGADeviceTemplate::ReadSingleRegisterBIT] Invalid Bit position (valid <= 31).");
                }

                bool operateStatus = true;
                uint32_t r_val;
                
                operateStatus &= this->RegisterIO(registerAddress, &r_val, true);  /* read */
                
                *value = FPGA_REG_BIT(r_val, bit);

                return operateStatus;
            }

            /**
             * @brief Set/Clear one bit of certain register.
             * 
             * @param registerSelection register to operate.
             * @param bit bit select (valid value: 0, 1, ..., 31).
             * @param isSet true: set this bit to 1, false: set this bit to 0.
             * 
             * @retval true: success, false: failed.
             */
            bool WriteSingleRegisterBIT(REG_SEL_ENUM registerSelection, uint32_t bit, bool isSet)
            {
                if (bit > 31)
                {
                    throw std::runtime_error("in [FPGADeviceTemplate::WriteSingleRegisterBIT] Invalid Bit position (valid <= 31).");
                }

                bool operateStatus = true;
                uint32_t r_val, w_val;
                uint32_t registerAddress = this->GetRegisterAbsoluteAddress(registerSelection);

                operateStatus &= this->RegisterIO(registerAddress, &r_val, true);  /* read */

                if (isSet)
                {
                    w_val = FPGA_SET_REG_BIT(r_val, bit);
                }
                else
                {
                    w_val = FPGA_CLEAR_REG_BIT(r_val, bit);
                }

                operateStatus &= this->RegisterIO(registerAddress, &w_val, false);

                return operateStatus;
            }

            /**
             * @brief Wait for register bit to certain value.
             * 
             * @param registerSelection register to operate.
             * @param bit bit select (valid value: 0, 1, ..., 31).
             * @param waitForValue 0 or 1.
             * @param timeout_us timeout (unit: us).
             * 
             * @retval true: success, false: failed.
             */
            bool WaitForRegisterBIT(REG_SEL_ENUM registerSelection, uint32_t bit, uint32_t waitForValue, uint32_t timeout_us = 1000)
            {
                if (bit > 31)
                {
                    throw std::runtime_error("in [FPGADeviceTemplate::WaitForRegisterBIT] Invalid Bit position (valid <= 31).");
                }

                uint32_t waitTime = 0;
                uint32_t r_val;
                uint32_t registerOffset = this->GetRegisterAbsoluteAddress(registerSelection);
                bool operateStatus = true;

                if (timeout_us <= 0)
                {
                    timeout_us = 100;
                }
                do
                {
                    operateStatus &= this->ReadSingleRegisterBIT(registerOffset, bit, &r_val);
                    if (r_val == waitForValue) break;
                    if (waitTime > timeout_us) break;
                    usleep(100);
                    waitTime += 100;
                }
                while (r_val != waitForValue);

                return operateStatus & (waitTime <= timeout_us);
            }

            /**
             * @brief Read value to bits of certain register.
             * 
             * @param registerSelection register to operate.
             * @param lower lower boundary of written.
             * @param upper upper boundary of written.
             * @param value read value.
             * 
             * @note e.g. lower = 1, upper = 4, value = 0x10
             * @note then, region [1:4] (contains bit-1 and bit-4) of the register will be read.
             * @note [0:0], [1:1], ..., [31:31] are also supported.
             * @note lower <= upper.
             * 
             * @retval true: success, false: failed.
             */
            bool ReadSingleRegisterBITRegion(REG_SEL_ENUM registerSelection, uint32_t lower, uint32_t upper, uint32_t *value)
            {
                if (lower > 31 || upper > 31)
                {
                    throw std::runtime_error("in [FPGADeviceTemplate::ReadSingleRegisterBITRegion] Invalid Bit position (valid <= 31).");
                }
                if (lower == upper)
                {
                    return this->ReadSingleRegisterBIT(registerSelection, lower, value);
                }

                bool operateStatus = true;
                uint32_t r_val;
                uint32_t registerAddress = this->GetRegisterAbsoluteAddress(registerSelection);

                operateStatus &= this->RegisterIO(registerAddress, &r_val, true);  /* read */

                uint32_t bitCount = upper - lower + 1;
                uint32_t mask = ((1 << bitCount) - 1) << lower;

                *value = (r_val & mask) >> lower;

                return operateStatus;
            }

            /**
             * @brief Write value to bits of certain register.
             * 
             * @param registerSelection register to operate.
             * @param lower lower boundary of written.
             * @param upper upper boundary of written.
             * @param value write value.
             * 
             * @note e.g. lower = 1, upper = 4, value = 0x10
             * @note then, region [1:4] (contains bit-1 and bit-4) of the register will be set with 0x10.
             * @note [0:0], [1:1], ..., [31:31] are also supported.
             * @note lower <= upper.
             * 
             * @retval true: success, false: failed.
             */
            bool WriteSingleRegisterBITRegion(REG_SEL_ENUM registerSelection, uint32_t lower, uint32_t upper, uint32_t value)
            {
                if (lower > 31 || upper > 31)
                {
                    throw std::runtime_error("in [FPGADeviceTemplate::WriteSingleRegisterBITRegion] Invalid Bit position (valid <= 31).");
                }
                if (lower == upper)
                {
                    return this->WriteSingleRegisterBIT(registerSelection, lower, (bool)(value & 0x01));
                }

                bool operateStatus = true;
                uint32_t r_val, w_val;
                uint32_t registerAddress = this->GetRegisterAbsoluteAddress(registerSelection);

                operateStatus &= this->RegisterIO(registerAddress, &r_val, true);  /* read */

                uint32_t bitCount = upper - lower + 1;
                uint32_t mask = ((1 << bitCount) - 1) << lower;

                uint32_t maxValue = (1 << bitCount) - 1;
                uint32_t safeValue = value & maxValue;

                w_val = (r_val & ~mask) | (safeValue << lower);

                operateStatus &= this->RegisterIO(registerAddress, &w_val, false);

                return operateStatus;
            }

            /* ----------------------------- Multiple Register IO ----------------------------- */

            /**
             * @brief Read multiple register.
             * 
             * @param registerSelection register to read.
             * @param readValue read value pointer.
             * 
             * @retval true: success, false: failed.
             */
            bool ReadMultipleRegister(const std::vector<REG_SEL_ENUM> &mulRegisterSelection, std::vector<uint32_t> *mulReadValue)
            {
                return this->RegisterIO(mulRegisterSelection, mulReadValue, true);
            }

            /**
             * @brief Write multiple register.
             * 
             * @param registerSelection register to write.
             * @param writeValue write value.
             * 
             * @retval true: success, false: failed.
             */
            bool WriteMultipleRegister(const std::vector<REG_SEL_ENUM> &mulRegisterSelection, const std::vector<uint32_t> &mulWriteValue)
            {
                std::vector<uint32_t> writeValueList = mulWriteValue;
                return this->RegisterIO(mulRegisterSelection, &writeValueList, false);
            }

            /**
             * @brief Read all register info from device.
             * 
             * @param registerName output register name.
             * @param registerOffset output register offset.
             * @param readValue output register value.
             * 
             * @retval true: success.
             * @retval false: failed.
             */
            bool ReadAllRegisters(std::vector<std::string> *registerName, std::vector<uint32_t> *registerOffset, std::vector<uint32_t> *readValue)
            {
                std::vector<REG_SEL_ENUM> mulRegisterSelection;
                int registerNumber;

                {
                    std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */

                    registerNumber = this->registerTable.size();
                    registerName->resize(registerNumber);
                    registerOffset->resize(registerNumber);

                    mulRegisterSelection.resize(registerNumber);

                    for (int i = 0; i < registerNumber; i++)
                    {
                        mulRegisterSelection[i] = this->registerTable[i].enumVal;
                        (*registerName)[i] = this->registerTable[i].name;
                        (*registerOffset)[i] = *this->registerTable[i].offsetVal;
                    }
                }

                this->ReadMultipleRegister(mulRegisterSelection, readValue);

                return true;
            }

            /* ----------------------------- Hardware Interrupt IO ----------------------------- */

            /**
             * @brief Read hardware interrupt.
             * 
             * @param readValue read value (1: interrupt detected, 0: no interrupt).
             * 
             * @retval true: success.
             * @retval false: failed.
             */
            bool ReadEvent(uint32_t *readValue)
            {
                return this->EventIO(readValue);
            }

            bool ConfigDone() const
            {
                return this->configdone;
            }
    };

    /**
     * @brief FPGA Memory Template (Abstract Base Class).
     * 
     * @note TODO in subclass
     * @note 1. Complete method LoadFromJsonObj();
     * 
     * @note USAGE in subclass:
     * @note 1. Generate object;
     * @note 2. Call method LoadFromJsonObj();
     * @note 3. Call method BindFPGAFileManager();
     * @note 4. Read/Write memory.
     * 
     * @note Thread safety.
     */
    class FPGAMemoryTemplate
    {
        private:

            std::atomic<bool> isIOManagerBind;
            std::weak_ptr<vuprs::FPGA_IOManagerForMemory> bindIOManager_h2c;
            std::weak_ptr<vuprs::FPGA_IOManagerForMemory> bindIOManager_c2h;
            std::string h2c_controlDeviceFilename, c2h_controlDeviceFilename;

            bool BufferIO(uint32_t offset, vuprs::AlignedBufferDMA *buffer, uint64_t transferByteSize, bool isRead)
            {
                /* Security Check */

                if (!this->isIOManagerBind) 
                {
                    throw std::runtime_error("in [FPGAMemoryTemplate::BufferIO] FPGA file manager is NULL.");
                }
                if (transferByteSize > __LINUX_DMA_MAX_TRANSFER_BYTES__) 
                {
                    throw std::runtime_error("in [FPGAMemoryTemplate::BufferIO] Too big transfer size.");
                }
                if ((transferByteSize + offset) > static_cast<uint32_t>(this->maxCapacityKB * __KILOBYTES__ - 1))
                {
                    throw std::runtime_error("in [FPGAMemoryTemplate::BufferIO] Invalid transfer size (valid: <= " + std::to_string(this->maxCapacityKB * __KILOBYTES__ - offset) + ")");
                }
                if (transferByteSize <= 0) 
                {
                    return true;
                }
                if (buffer == nullptr) 
                {
                    throw std::runtime_error("in [FPGAMemoryTemplate::BufferIO] *Buffer is nullptr.");
                }

                /* Operation */
                
                uint32_t targetOffset = offset + this->fpgaAddress;

                if (isRead)
                {
                    buffer->malloc(transferByteSize);
                    std::shared_ptr<vuprs::FPGA_IOManagerForMemory> c2h_manager;
                    {
                        std::unique_lock<std::mutex> lock(this->mut_c2h);  /* LOCK */
                        c2h_manager = this->bindIOManager_c2h.lock();
                    }
                    return c2h_manager->BufferIO(buffer->data(), targetOffset, transferByteSize, true);
                }
                else
                {
                    std::shared_ptr<vuprs::FPGA_IOManagerForMemory> h2c_manager;
                    {
                        std::unique_lock<std::mutex> lock(this->mut_h2c);  /* LOCK */
                        h2c_manager = this->bindIOManager_h2c.lock();
                    }
                    return h2c_manager->BufferIO(buffer->data(), targetOffset, transferByteSize, false);
                }
            }

            bool WordIO(uint32_t offset, uint32_t *ioValue, bool isRead)
            {
                /* Security Check */

                if (!this->isIOManagerBind) 
                {
                    throw std::runtime_error("in [FPGAMemoryTemplate::WordIO] FPGA file manager is NULL.");
                }
                if (offset > static_cast<uint32_t>(this->maxCapacityKB * __KILOBYTES__ - 1)) 
                {
                    throw std::runtime_error("in [FPGAMemoryTemplate::WordIO] Invalid offset (valid: <= " + std::to_string(this->maxCapacityKB * __KILOBYTES__ - 1) + ")");
                }
                if (ioValue == nullptr) 
                {
                    throw std::runtime_error("in [FPGAMemoryTemplate::WordIO] *readValue is nullptr.");
                }

                /* Operation */

                uint32_t targetOffset = offset + this->fpgaAddress;

                if (isRead) 
                {
                    std::shared_ptr<vuprs::FPGA_IOManagerForMemory> c2h_manager;
                    {
                        std::unique_lock<std::mutex> lock(this->mut_c2h);  /* LOCK */
                        c2h_manager = this->bindIOManager_c2h.lock();
                    }
                    return c2h_manager->BufferIO(ioValue, targetOffset, sizeof(uint32_t), true);
                }
                else 
                {
                    std::shared_ptr<vuprs::FPGA_IOManagerForMemory> h2c_manager;
                    {
                        std::unique_lock<std::mutex> lock(this->mut_h2c);  /* LOCK */
                        h2c_manager = this->bindIOManager_h2c.lock();
                    }
                    return h2c_manager->BufferIO(ioValue, targetOffset, sizeof(uint32_t), false);
                }
            }

        protected:

            FPGABus dataBus;
            std::atomic<uint32_t> fpgaAddress;
            std::atomic<uint32_t> maxCapacityKB;

            mutable std::mutex mut;  /* Global mutex lock */
            mutable std::mutex mut_c2h;  /* C2H mutex lock */
            mutable std::mutex mut_h2c;  /* H2C mutex lock */

            std::atomic<bool> configdone;

            bool LoadMainInfoFromJsonObj(const nlohmann::json &obj)
            {
                std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */

                uint32_t fpgaAddress, maxCapacityKB;

                vuprs::__JsonStringParseINT<uint32_t>(&fpgaAddress, obj, "fpga-address", true);
                vuprs::__JsonStringParseINT<uint32_t>(&maxCapacityKB, obj, "memory-capacity-kilobytes", true);
                vuprs::__JsonParseString(&this->h2c_controlDeviceFilename, obj, "h2c-device-file", true);
                vuprs::__JsonParseString(&this->c2h_controlDeviceFilename, obj, "c2h-device-file", true);

                this->fpgaAddress = fpgaAddress;
                this->maxCapacityKB = maxCapacityKB;

                return true;
            }

        public:

            FPGAMemoryTemplate()
            {
                {
                    std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */

                    this->dataBus = vuprs::FPGABus::AXI_FULL;
                    this->fpgaAddress = 0;
                    this->maxCapacityKB = 1;

                    this->configdone = false;

                    this->isIOManagerBind = false;
                }
                {
                    std::unique_lock<std::mutex> lock(this->mut_c2h);  /* LOCK */
                    this->bindIOManager_c2h.reset();
                }
                {
                    std::unique_lock<std::mutex> lock(this->mut_h2c);  /* LOCK */
                    this->bindIOManager_h2c.reset();
                }
            }

            virtual ~FPGAMemoryTemplate() {}
            virtual bool LoadFromJsonObj(const nlohmann::json &obj) = 0;

            std::string H2C_ControlDeviceFilename() const
            {
                std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */
                return this->h2c_controlDeviceFilename;
            }

            std::string C2H_ControlDeviceFilename() const
            {
                std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */
                return this->c2h_controlDeviceFilename;
            }

            /**
             * @brief Memory capacity in bytes.
             */
            uint32_t MaxSizeBytes() const
            {
                std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */
                return this->maxCapacityKB * __KILOBYTES__;
            }

            /**
             * @brief Bind FPGA file manager.
             */
            bool BindFPGAFileManager(std::shared_ptr<vuprs::FPGA_IOManagerForMemory> ioManager_h2c,
                                     std::shared_ptr<vuprs::FPGA_IOManagerForMemory> ioManager_c2h)
            {
        
                if (ioManager_h2c == nullptr || ioManager_c2h == nullptr)
                {
                    return false;
                }
                
                std::shared_ptr<vuprs::FPGA_IOManagerForMemory> current_h2c;
                std::shared_ptr<vuprs::FPGA_IOManagerForMemory> current_c2h;
                
                {
                    std::unique_lock<std::mutex> lock(this->mut_h2c);  /* LOCK */
                    current_h2c = this->bindIOManager_h2c.lock();
                }
                {
                    std::unique_lock<std::mutex> lock(this->mut_c2h);  /* LOCK */
                    current_c2h = this->bindIOManager_c2h.lock();
                }
                
                if (this->isIOManagerBind) 
                {
                    if ((current_h2c && current_h2c != ioManager_h2c) || (current_c2h && current_c2h != ioManager_c2h)) 
                    {
                        this->UnbindFileManager();
                    }
                }

                {
                    std::unique_lock<std::mutex> lock(this->mut_h2c);  /* LOCK */
                    this->bindIOManager_h2c = ioManager_h2c;
                }
                {
                    std::unique_lock<std::mutex> lock(this->mut_c2h);  /* LOCK */
                    this->bindIOManager_c2h = ioManager_c2h;
                }
                
                this->isIOManagerBind = true;

                return true;
            }

            /**
             * @brief Unbind FPGA file manager.
             */
            void UnbindFileManager()
            {
                {
                    std::unique_lock<std::mutex> lock(this->mut_h2c);  /* LOCK */
                    this->bindIOManager_h2c.reset();
                }
                {
                    std::unique_lock<std::mutex> lock(this->mut_c2h);  /* LOCK */
                    this->bindIOManager_c2h.reset();
                }
                this->isIOManagerBind = false;
            }

            /**
             * @brief Read data from memory to buffer.
             * 
             * @note The function can automaticaly allocate the buffer.
             * 
             * @param buffer the buffer to store data.
             * @param offset offset from base address of the memory.
             * @param transferByteSize transfer size in bytes.
             * 
             * @retval true: success, false: failed.
             * 
             * @throw std::runtime_error
             */
            bool ReadMemory(vuprs::AlignedBufferDMA *buffer, uint32_t offset, uint64_t transferByteSize)
            {
                return this->BufferIO(offset, buffer, transferByteSize, true);
            }

            /**
             * @brief Write buffer data to memory.
             * 
             * @note The buffer must be allocated in advance.
             * 
             * @param buffer data buffer.
             * @param offset offset from base address of the memory.
             * @param transferByteSize transfer size in bytes.
             * 
             * @retval true: success, false: failed.
             * 
             * @throw std::runtime_error
             */
            bool WriteMemory(vuprs::AlignedBufferDMA *buffer, uint32_t offset, uint64_t transferByteSize)
            {
                return this->BufferIO(offset, buffer, transferByteSize, false);
            }

            /**
             * @brief Read 4 bytes data from memory.
             * 
             * @param offset offset from base address of the memory.
             * @param readValue read value.
             * 
             * @retval true: success, false: failed.
             * 
             * @throw std::runtime_error
             */
            bool ReadMemory(uint32_t offset, uint32_t *readValue)
            {
                return this->WordIO(offset, readValue, true);
            }

            /**
             * @brief Write 4 bytes data to memory.
             * 
             * @param offset offset from base address of the memory.
             * @param writeValue write value.
             * 
             * @retval true: success, false: failed.
             * 
             * @throw std::runtime_error
             */
            bool WriteMemory(uint32_t offset, uint32_t writeValue)
            {
                return this->WordIO(offset, &writeValue, false);
            }

            bool ConfigDone() const
            {
                return this->configdone;
            }

            uint32_t FPGAAddress() const
            {
                return this->fpgaAddress;
            }
    };
}

#endif
