#ifndef FPGA_MODULE_TEMPLATE_H
#define FPGA_MODULE_TEMPLATE_H

#include "nlohmann/json.hpp"
#include "string_parse.h"
#include "aligned_buffer.h"
#include "fpga_io_manager.h"

#define __MEGABYTES__                             (1024U * 1024U);
#define __KILOBYTES__                             1024U

#define __LINUX_DMA_MAX_TRANSFER_BYTES__          0x7ffff000  /* Maximum transfer size in Linux-32bit & Linux-64bit */

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
    template<typename REG_SEL_ENUM>
    class FPGADeviceTemplate
    {
        private:

            bool isIOManagerBind;
            vuprs::FPGA_IOManager* bindIOManager;
            std::string controlDeviceFilename;

            bool RegisterIO(REG_SEL_ENUM registerSelection, uint32_t* ioValue, bool isRead)
            {
                /* Security Check */

                if (!this->isIOManagerBind) 
                {
                    throw std::runtime_error("FPGA file manager is NULL.");
                }

                /* Operation */

                uint32_t registerAddressOffset = this->GetRegisterAbsoluteAddress(registerSelection);
                
                if (isRead) 
                {
                    return this->bindIOManager->RegisterIO(ioValue, registerAddressOffset, true);
                }
                else 
                {
                    return this->bindIOManager->RegisterIO(ioValue, registerAddressOffset, false);
                }
            }

            bool RegisterIO(std::vector<REG_SEL_ENUM> mulRegisterSelection, std::vector<uint32_t*> ioValue, bool isRead)
            {
                /* Security Check */

                if (!this->isIOManagerBind) 
                {
                    throw std::runtime_error("FPGA file manager is NULL."); 
                }
                if (registerNumber <= 0) 
                {
                    throw std::runtime_error("No registers read or written."); 
                }
                if (ioValue.size() != registerNumber) 
                {
                    throw std::runtime_error("mulReadValue.size() != register count to read.");
                }
               
                /* Operation */

                std::vector<uint32_t> multiRegisterAddressOffset;
                int registerNumber = mulRegisterSelection.size();

                multiRegisterAddressOffset.resize(registerNumber);

                for (int i = 0; i < registerNumber; i++)
                {
                    multiRegisterAddressOffset[i] = this->GetRegisterAbsoluteAddress(mulRegisterSelection[i]);
                }

                if (isRead) 
                {
                    return this->bindIOManager->RegisterListIO(ioValue, multiRegisterAddressOffset, true);
                }
                else 
                {
                    return this->bindIOManager->RegisterListIO(ioValue, multiRegisterAddressOffset, false);
                }
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
                this->bindIOManager = nullptr;
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
            bool BindFPGAFileManager(vuprs::FPGA_IOManager* ioManager)
            {
                if (ioManager != nullptr)
                {
                    this->isIOManagerBind = true;
                    this->bindIOManager = ioManager;
                    return true;
                }
                this->isIOManagerBind = false;
                return false;
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
             * @brief Read single register.
             * 
             * @param registerSelection register to read.
             * @param readValue read value pointer.
             * 
             * @retval true: success, false: failed.
             */
            bool ReadSingleRegister(REG_SEL_ENUM registerSelection, uint32_t *readValue)
            {
                return this->RegisterIO(registerSelection, readValue, true);
            }

            /**
             * @brief Write single register.
             * 
             * @param registerSelection register to write.
             * @param writeValue write value.
             * 
             * @retval true: success, false: failed.
             */
            bool WriteSingleRegister(REG_SEL_ENUM registerSelection, uint32_t writeValue)
            {
                return this->RegisterIO(registerSelection, &writeValue, false);
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
            bool ReadMultipleRegister(std::vector<REG_SEL_ENUM> mulRegisterSelection, std::vector<uint32_t*> mulReadValue)
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
            bool WriteMultipleRegister(std::vector<REG_SEL_ENUM> mulRegisterSelection, std::vector<uint32_t> mulWriteValue)
            {
                std::vector<uint32_t*> ioValueList;
                int registerNumber = mulWriteValue.size()
                ioValueList.resize(registerNumber);

                for (int i = 0; i < registerNumber; i++) 
                {
                    ioValueList[i] = &mulWriteValue[i];
                }

                return this->RegisterIO(mulRegisterSelection, ioValueList, false);
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
            vuprs::FPGA_IOManager *bindIOManager_h2c, *bindIOManager_c2h;
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
                    return this->bindIOManager_c2h->BufferIO(buffer->data(), targetOffset, transferByteSize, true);
                }
                else 
                {
                    return this->bindIOManager_h2c->BufferIO(buffer->data(), targetOffset, transferByteSize, false);
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
                    return this->bindIOManager_c2h->BufferIO(ioValue, targetOffset, sizeof(uint32_t), true);
                }
                else 
                {
                    return this->bindIOManager_h2c->BufferIO(ioValue, targetOffset, sizeof(uint32_t), false);
                }
            }

        protected:

            FPGABus dataBus;
            uint32_t fpgaAddress;
            uint32_t maxCapacityKB;

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

                this->isIOManagerBind = false;
                this->bindIOManager_c2h = nullptr;
                this->bindIOManager_h2c = nullptr;
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
            bool BindFPGAFileManager(vuprs::FPGA_IOManager* ioManager_h2c, vuprs::FPGA_IOManager* ioManager_c2h)
            {
                if (ioManager_h2c != nullptr && ioManager_c2h != nullptr)
                {
                    this->isIOManagerBind = true;
                    this->bindIOManager_h2c = ioManager_h2c;
                    this->bindIOManager_c2h = ioManager_c2h;
                    return true;
                }
                this->isIOManagerBind = false;
                return false;
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
    };
}

#endif
