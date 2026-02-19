#ifndef FPGA_MODULE_TEMPLATE_H
#define FPGA_MODULE_TEMPLATE_H

#include "nlohmann/json.hpp"
#include "string_parse.h"
#include "aligned_buffer.h"
#include "fpga_io_manager.h"

#define __MEGABYTES__                             (1024U * 1024U);
#define __KILOBYTES__                             1024U

#define __LINUX_DMA_MAX_TRANSFER_BYTES__          0x7ffff000  /* Maximum transfer size in Linux-32bit & Linux-64bit */

#define _IS_CODING_MODE false  /* should be false before compilering */

#define FPGA_REG_BIT(REG, BIT) ((REG) & (uint32_t)((uint32_t)0x00000001 << (BIT)))
#define FPGA_CLEAR_REG_BIT(REG, BIT) (uint32_t)((REG) & ~(uint32_t)((uint32_t)1U << (BIT)))
#define FPGA_SET_REG_BIT(REG, BIT) (uint32_t)((REG) | (uint32_t)((uint32_t)1U << (BIT)))

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
     */
#if !_IS_CODING_MODE
    template<typename REG_SEL_ENUM>
#endif
    class FPGADeviceTemplate
    {
        private:

            bool isIOManagerBind;
            std::weak_ptr<vuprs::FPGA_IOManager> bindIOManager;
            std::string controlDeviceFilename;

            bool RegisterIO(uint32_t registerAddress, uint32_t* ioValue, bool isRead)
            {
                /* Security Check */

                if (!this->isIOManagerBind) 
                {
                    throw std::runtime_error("FPGA file manager is NULL.");
                }

                auto manager = this->bindIOManager.lock();

                if (!manager) return false;

                return manager->RegisterIO(ioValue, registerAddress, isRead);
            }

            bool RegisterIO(const std::vector<REG_SEL_ENUM> &mulRegisterSelection, std::vector<uint32_t> *ioValue, bool isRead)
            {
                /* Security Check */

                int registerNumber = mulRegisterSelection.size();

                if (!this->isIOManagerBind) 
                {
                    throw std::runtime_error("FPGA file manager is NULL."); 
                }
                if (registerNumber <= 0)
                {
                    throw std::runtime_error("No registers read or written."); 
                }
                if (!isRead && ioValue->size() != registerNumber) 
                {
                    throw std::runtime_error("mulReadValue.size() != register count to read.");
                }

                auto manager = this->bindIOManager.lock();

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
            
        protected:

            FPGABus controlBus;
            FPGABus dataBus;
            
            uint32_t fpgaAddress;
            uint32_t barOffset;

            std::vector<vuprs::_DeviceRegisterConfig<REG_SEL_ENUM>> registerTable;

            bool configdone;

            virtual void GenerateRegisterTable() = 0;

            /**
             * @brief Set all register offset to 0.
             */
            void SetRegisterOffsetDefault()
            {
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
                    throw std::runtime_error("register-offset not found.");
                }
                
                /* Operation */

                int registerNumber = this->registerTable.size();
            
                auto registers = obj["register-offset"];
                for (int i = 0; i < registerNumber; i++)
                {
                    vuprs::__JsonStringParseINT<uint32_t>(this->registerTable[i].offsetVal, registers, this->registerTable[i].name, true);
                }
            
                /* Base Address */
            
                vuprs::__JsonStringParseINT<uint32_t>(&this->fpgaAddress, obj, "fpga-address", true);
                vuprs::__JsonStringParseINT<uint32_t>(&this->barOffset, obj, "bar-offset", true);
                vuprs::__JsonParseString(&this->controlDeviceFilename, obj, "control-device-file", true);

                return true;
            }

        public:

            FPGADeviceTemplate(const FPGADeviceTemplate&) = delete;
            FPGADeviceTemplate(FPGADeviceTemplate&&) = delete;

            FPGADeviceTemplate& operator=(const FPGADeviceTemplate&) = delete;
            FPGADeviceTemplate& operator=(FPGADeviceTemplate&&) = delete;

            FPGADeviceTemplate()
            {
                this->controlBus = vuprs::FPGABus::AXI_LITE;
                this->dataBus = vuprs::FPGABus::AXI_STREAM;
                this->fpgaAddress = 0;
                this->barOffset = 0;
                this->configdone = false;
                this->isIOManagerBind = false;
                this->registerTable.clear();
                this->bindIOManager.reset();
            }

            virtual ~FPGADeviceTemplate() {}
            virtual bool LoadFromJsonObj(const nlohmann::json &obj) = 0;

            std::string ControlDeviceFilename() const
            {
                return this->controlDeviceFilename;
            }

            /**
             * @brief Bind FPGA file manager.
             */
            bool BindFPGAFileManager(std::shared_ptr<vuprs::FPGA_IOManager> ioManager)
            {
                if (!ioManager) 
                {
                    return false;
                }

                auto manager = this->bindIOManager.lock();

                if (manager && manager != ioManager) 
                {
                    UnbindFileManager();
                }

                this->isIOManagerBind = true;
                bindIOManager = ioManager;
                return true;
            }
    
            /**
             * @brief Unbind FPGA file manager.
             */
            void UnbindFileManager()
            {
                this->bindIOManager.reset();
                this->isIOManagerBind = false;
            }

            /**
             * @brief Get absolute address of register (= device bar address + register offset).
             */
            uint32_t GetRegisterAbsoluteAddress(REG_SEL_ENUM registerSelection)
            {
                /* Security Check */

                if (!this->configdone) 
                {
                    throw std::runtime_error("Config not complete.");
                }

                /* Operation */

                uint32_t registerOffset = 0;
                bool registerFound = false;
                int registerNumber = this->registerTable.size();

                for (int i = 0; i < registerNumber; i++)
                {
                    if (registerSelection == this->registerTable[i].enumVal)
                    {
                        registerOffset = *(this->registerTable[i].offsetVal);
                        registerFound = true;
                        break;
                    }
                }

                if (!registerFound) 
                {
                    throw std::runtime_error("Invalid register selection.");
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
                    throw std::runtime_error("Offset must aligned to 4 bytes.");
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
                    throw std::runtime_error("Offset must aligned to 4 bytes.");
                }
                uint32_t registerAddress = offset + this->barOffset;
                return this->RegisterIO(registerAddress, &writeValue, false);
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
                    throw std::runtime_error("Invalid Bit position (valid <= 31).");
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
                
                int registerNumber = this->registerTable.size();
                registerName->resize(registerNumber);
                registerOffset->resize(registerNumber);

                mulRegisterSelection.resize(registerNumber);
                
                for (int i = 0; i < registerNumber; i++)
                {
                    mulRegisterSelection[i] = this->registerTable[i].enumVal;
                    (*registerName)[i] = this->registerTable[i].name;
                    (*registerOffset)[i] = *this->registerTable[i].offsetVal;
                }

                this->ReadMultipleRegister(mulRegisterSelection, readValue);

                return true;
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
     */
    class FPGAMemoryTemplate
    {
        private:

            bool isIOManagerBind;
            std::weak_ptr<vuprs::FPGA_IOManager> bindIOManager_h2c, bindIOManager_c2h;
            std::string h2c_controlDeviceFilename, c2h_controlDeviceFilename;

            bool BufferIO(uint32_t offset, vuprs::AlignedBufferDMA *buffer, uint64_t transferByteSize, bool isRead)
            {
                /* Security Check */

                if (!this->isIOManagerBind) 
                {
                    throw std::runtime_error("FPGA file manager is NULL.");
                }
                if (transferByteSize > __LINUX_DMA_MAX_TRANSFER_BYTES__) 
                {
                    throw std::runtime_error("Too big transfer size.");
                }
                if (transferByteSize + offset > this->maxCapacityKB * __KILOBYTES__ - 1)
                {
                    throw std::runtime_error("Invalid transfer size (valid: <= " + std::to_string(this->maxCapacityKB * __KILOBYTES__ - offset) + ")");
                }
                if (transferByteSize <= 0) 
                {
                    return true;
                }
                if (buffer == nullptr) 
                {
                    throw std::runtime_error("*Buffer is nullptr.");
                }

                /* Operation */

                uint32_t targetOffset = offset + this->fpgaAddress;

                if (isRead) 
                {
                    auto c2h_manager = this->bindIOManager_c2h.lock();
                    return c2h_manager->BufferIO(buffer->data(), targetOffset, transferByteSize, true);
                }
                else
                {
                    auto h2c_manager = this->bindIOManager_h2c.lock();
                    return h2c_manager->BufferIO(buffer->data(), targetOffset, transferByteSize, false);
                }
            }

            bool WordIO(uint32_t offset, uint32_t *ioValue, bool isRead)
            {
                /* Security Check */

                if (!this->isIOManagerBind) 
                {
                    throw std::runtime_error("FPGA file manager is NULL.");
                }
                if (offset > this->maxCapacityKB * __KILOBYTES__ - 1) 
                {
                    throw std::runtime_error("Invalid offset (valid: <= " + std::to_string(this->maxCapacityKB * __KILOBYTES__ - 1) + ")");
                }
                if (ioValue == nullptr) 
                {
                    throw std::runtime_error("*readValue is nullptr.");
                }

                /* Operation */

                uint32_t targetOffset = offset + this->fpgaAddress;

                if (isRead) 
                {
                    auto c2h_manager = this->bindIOManager_c2h.lock();
                    return c2h_manager->BufferIO(ioValue, targetOffset, sizeof(uint32_t), true);
                }
                else 
                {
                    auto h2c_manager = this->bindIOManager_h2c.lock();
                    return h2c_manager->BufferIO(ioValue, targetOffset, sizeof(uint32_t), false);
                }
            }

        protected:

            FPGABus dataBus;
            uint32_t fpgaAddress;
            uint32_t maxCapacityKB;

            bool configdone;

            bool LoadMainInfoFromJsonObj(const nlohmann::json &obj)
            {
                vuprs::__JsonStringParseINT<uint32_t>(&this->fpgaAddress, obj, "fpga-address", true);
                vuprs::__JsonStringParseINT<uint32_t>(&this->maxCapacityKB, obj, "memory-capacity-kilobytes", true);
                vuprs::__JsonParseString(&this->h2c_controlDeviceFilename, obj, "h2c-device-file", true);
                vuprs::__JsonParseString(&this->c2h_controlDeviceFilename, obj, "c2h-device-file", true);
                return true;
            }

        public:

            FPGAMemoryTemplate()
            {
                this->dataBus = vuprs::FPGABus::AXI_FULL;
                this->fpgaAddress = 0;
                this->maxCapacityKB = 1;

                this->configdone = false;

                this->isIOManagerBind = false;
                this->bindIOManager_c2h.reset();
                this->bindIOManager_h2c.reset();
            }

            virtual ~FPGAMemoryTemplate() {}
            virtual bool LoadFromJsonObj(const nlohmann::json &obj) = 0;

            std::string H2C_ControlDeviceFilename() const
            {
                return this->h2c_controlDeviceFilename;
            }

            std::string C2H_ControlDeviceFilename() const
            {
                return this->c2h_controlDeviceFilename;
            }

            /**
             * @brief Bind FPGA file manager.
             */
            bool BindFPGAFileManager(std::shared_ptr<vuprs::FPGA_IOManager> ioManager_h2c,
                                     std::shared_ptr<vuprs::FPGA_IOManager> ioManager_c2h)
            {
        
                if (ioManager_h2c == nullptr || ioManager_c2h == nullptr)
                {
                    return false;
                }
            
                auto current_h2c = this->bindIOManager_h2c.lock();
                auto current_c2h = this->bindIOManager_c2h.lock();
            
                if (this->isIOManagerBind) 
                {
                    if ((current_h2c && current_h2c != ioManager_h2c) || (current_c2h && current_c2h != ioManager_c2h)) 
                    {
                        UnbindFileManager();
                    }
                }
            
                this->bindIOManager_h2c = ioManager_h2c;
                this->bindIOManager_c2h = ioManager_c2h;
                this->isIOManagerBind = true;

                return true;
            }

            /**
             * @brief Unbind FPGA file manager.
             */
            void UnbindFileManager()
            {
                this->bindIOManager_h2c.reset();
                this->bindIOManager_c2h.reset();
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
    };
}

#endif
