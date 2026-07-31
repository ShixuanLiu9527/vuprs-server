#ifndef FPGA_MODULE_TEMPLATE_H
#define FPGA_MODULE_TEMPLATE_H

#include "3rdparty/nlohmann/json.hpp"
#include "system_tools/string_parse.h"
#include "system_tools/aligned_buffer.h"
#include "fpga/fpga_io_manager.h"
#include "logger/log_manager.h"

#define _IS_CODING_MODE false /* should be false before compilering */
#if _IS_CODING_MODE
#error "In coding mode."
#endif

#define __MEGABYTES__ (1024U * 1024U);
#define __KILOBYTES__ 1024U

#define __LINUX_DMA_MAX_TRANSFER_BYTES__ 0x7ffff000 /* Maximum transfer size in Linux-32bit & Linux-64bit */

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

    template <typename REG_SEL_ENUM>
    struct _DeviceRegisterConfig
    {
        uint32_t *offsetVal;
        std::string name;
        REG_SEL_ENUM enumVal;
    };

#if _IS_CODING_MODE
    enum class REG_SEL_ENUM /* for coding */
    {
        REG_A,
        REG_B,
        REG_C
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
    template <typename REG_SEL_ENUM>
#endif
    class FPGADeviceTemplate
    {
    private:
        std::atomic<bool> is_io_manager_bind;
        std::atomic<bool> is_io_manager_bind_interrupt;

        std::weak_ptr<vuprs::FPGA_IOManagerForDevice> bind_io_manager_device;
        std::weak_ptr<vuprs::FPGA_IOManagerForInterrput> bind_io_manager_interrput;

        std::string control_device_filename;
        std::string event_device_filename;

        bool RegisterIO(uint32_t reg_addr, uint32_t *io_value, bool is_read)
        {
            /* Security Check */
            PARAM_CHECK(this->is_io_manager_bind, "fpga", " in [FPGADeviceTemplate::RegisterIO] FPGA file manager is NULL.");
            std::shared_ptr<vuprs::FPGA_IOManagerForDevice> manager;
            {
                std::unique_lock<std::mutex> lock(this->mut_dev); /* LOCK */
                manager = this->bind_io_manager_device.lock();
            }
            if (!manager)
                return false;
            return manager->RegisterIO(io_value, reg_addr, is_read);
        }

        bool RegisterIO(const std::vector<REG_SEL_ENUM> &mul_reg_selection, std::vector<uint32_t> *io_value, bool is_read)
        {
            /* Security Check */
            int reg_num = mul_reg_selection.size();
            PARAM_CHECK(this->is_io_manager_bind, "fpga", " in [FPGADeviceTemplate::RegisterIO] FPGA file manager is NULL.");
            PARAM_CHECK(reg_num > 0, "fpga", " in [FPGADeviceTemplate::RegisterIO] No registers read or written.");
            PARAM_CHECK(is_read || io_value->size() == reg_num, "fpga", " in [FPGADeviceTemplate::RegisterIO] mul_read_value.size() != register count to read.");
            std::shared_ptr<vuprs::FPGA_IOManagerForDevice> manager;
            {
                std::unique_lock<std::mutex> lock(this->mut_dev); /* LOCK */
                manager = this->bind_io_manager_device.lock();
            }
            if (!manager)
                return false;
            /* Operation */
            std::vector<uint32_t> multi_reg_addr_offset;
            multi_reg_addr_offset.resize(reg_num);
            for (int i = 0; i < reg_num; i++)
            {
                multi_reg_addr_offset[i] = this->GetRegisterAbsoluteAddress(mul_reg_selection[i]);
            }
            return manager->RegisterListIO(io_value, multi_reg_addr_offset, is_read);
        }

        bool EventIO(uint32_t *read_value)
        {
            PARAM_CHECK(this->is_io_manager_bind_interrupt, "fpga", " in [FPGADeviceTemplate::EventIO] FPGA event file manager is NULL.");
            std::shared_ptr<vuprs::FPGA_IOManagerForInterrput> manager;
            *read_value = 0;
            {
                std::unique_lock<std::mutex> lock(this->mut_event); /* LOCK */
                manager = this->bind_io_manager_interrput.lock();
            }
            if (!manager)
                return false;
            return manager->ReadEvent(read_value);
        }

    protected:
        FPGABus control_bus;
        FPGABus data_bus;
        std::atomic<uint32_t> fpga_address;
        std::atomic<uint32_t> bar_offset;
        std::vector<vuprs::_DeviceRegisterConfig<REG_SEL_ENUM>> register_table;
        mutable std::mutex mut;       /* Global lock */
        mutable std::mutex mut_dev;   /* Device IO manager lock */
        mutable std::mutex mut_event; /* Event IO manager lock */
        std::atomic<bool> config_done;

        virtual void GenerateRegisterTable() = 0;

        /**
         * @brief Set all register offset to 0.
         */
        void SetRegisterOffsetDefault()
        {
            std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
            int reg_num = this->register_table.size();
            for (int i = 0; i < reg_num; i++)
            {
                *(this->register_table[i].offsetVal) = 0;
            }
        }

        bool LoadMainInfoFromJsonObj(const nlohmann::json &obj)
        {
            /* Security Check */
            PARAM_CHECK(obj.contains("register-offset"), "fpga", " in [FPGADeviceTemplate::LoadMainInfoFromJsonObj] register-offset not found.");
            /* Operation */
            {
                std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
                int reg_num = this->register_table.size();
                auto registers = obj["register-offset"];
                for (int i = 0; i < reg_num; i++)
                {
                    vuprs::__JsonStringParseINT<uint32_t>(this->register_table[i].offsetVal, registers, this->register_table[i].name, true);
                }
                /* Base Address */
                uint32_t fpga_address, bar_offset;
                vuprs::__JsonStringParseINT<uint32_t>(&fpga_address, obj, "fpga-address", true);
                vuprs::__JsonStringParseINT<uint32_t>(&bar_offset, obj, "bar-offset", true);
                this->fpga_address = fpga_address;
                this->bar_offset = bar_offset;
                vuprs::__JsonParseString(&this->control_device_filename, obj, "control-device-file", true);
                vuprs::__JsonParseString(&this->event_device_filename, obj, "event-device-file", false); /* event is not required for all devices */
            }
            return true;
        }

    public:
        FPGADeviceTemplate(const FPGADeviceTemplate &) = delete;
        FPGADeviceTemplate(FPGADeviceTemplate &&) = delete;

        FPGADeviceTemplate &operator=(const FPGADeviceTemplate &) = delete;
        FPGADeviceTemplate &operator=(FPGADeviceTemplate &&) = delete;

        FPGADeviceTemplate()
        {
            {
                std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
                this->control_bus = vuprs::FPGABus::AXI_LITE;
                this->data_bus = vuprs::FPGABus::AXI_STREAM;
                this->fpga_address = 0;
                this->bar_offset = 0;
                this->config_done = false;
                this->register_table.clear();
                this->is_io_manager_bind = false;
                this->is_io_manager_bind_interrupt = false;
            }
            {
                std::unique_lock<std::mutex> lock(this->mut_dev); /* LOCK */
                this->bind_io_manager_device.reset();
            }
            {
                std::unique_lock<std::mutex> lock(this->mut_event); /* LOCK */
                this->bind_io_manager_interrput.reset();
            }
        }

        virtual ~FPGADeviceTemplate() {}
        virtual bool LoadFromJsonObj(const nlohmann::json &obj) = 0;

        std::string ControlDeviceFilename() const
        {
            std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
            return this->control_device_filename;
        }

        std::string EventDeviceFilename() const
        {
            std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
            return this->event_device_filename;
        }

        /**
         * @brief Bind FPGA file manager.
         */
        bool BindFPGAFileManager(std::shared_ptr<vuprs::FPGA_IOManagerForDevice> io_manager)
        {
            if (!io_manager)
            {
                return false;
            }
            std::shared_ptr<vuprs::FPGA_IOManagerForDevice> manager;
            {
                std::unique_lock<std::mutex> lock(this->mut_dev); /* LOCK */
                manager = this->bind_io_manager_device.lock();
            }
            if (manager && manager != io_manager)
            {
                this->UnbindFileManager();
            }
            this->is_io_manager_bind = true;
            {
                std::unique_lock<std::mutex> lock(this->mut_dev); /* LOCK */
                this->bind_io_manager_device = io_manager;
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
                std::unique_lock<std::mutex> lock(this->mut_event); /* LOCK */
                manager = this->bind_io_manager_interrput.lock();
            }
            if (manager && manager != ioManager_interrupt)
            {
                this->UnbindFileManager();
            }
            this->is_io_manager_bind_interrupt = true;
            {
                std::unique_lock<std::mutex> lock(this->mut_event); /* LOCK */
                this->bind_io_manager_interrput = ioManager_interrupt;
            }
            return true;
        }

        /**
         * @brief Unbind FPGA file manager.
         */
        void UnbindFileManager()
        {
            {
                std::unique_lock<std::mutex> lock(this->mut_dev); /* LOCK */
                this->bind_io_manager_device.reset();
            }
            this->is_io_manager_bind = false;
        }

        /**
         * @brief Unbind FPGA file manager (for interrupt).
         */
        void UnbindFileManager_Interrupt()
        {
            {
                std::unique_lock<std::mutex> lock(this->mut_event); /* LOCK */
                this->bind_io_manager_interrput.reset();
            }
            this->is_io_manager_bind_interrupt = false;
        }

        /**
         * @brief Get absolute address of register (= device bar address + register offset).
         */
        uint32_t GetRegisterAbsoluteAddress(REG_SEL_ENUM reg_selection)
        {
            /* Security Check */
            PARAM_CHECK(this->config_done, "fpga", " in [FPGADeviceTemplate::GetRegisterAbsoluteAddress] Config not complete.");
            /* Operation */
            uint32_t reg_offset = 0;
            bool reg_found = false;
            int reg_num;
            {
                std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
                reg_num = this->register_table.size();
                for (int i = 0; i < reg_num; i++)
                {
                    if (reg_selection == this->register_table[i].enumVal)
                    {
                        reg_offset = *(this->register_table[i].offsetVal);
                        reg_found = true;
                        break;
                    }
                }
            }
            PARAM_CHECK(reg_found, "fpga", " in [FPGADeviceTemplate::GetRegisterAbsoluteAddress] Invalid register selection.");
            return reg_offset + this->bar_offset;
        }

        /* ------------------------------ Single Register IO ------------------------------ */

        /**
         * @brief Read single register (use register selection).
         *
         * @param reg_selection register to read.
         * @param read_value read value pointer.
         *
         * @retval true: success, false: failed.
         */
        bool ReadSingleRegister(REG_SEL_ENUM reg_selection, uint32_t *read_value)
        {
            uint32_t reg_addr = this->GetRegisterAbsoluteAddress(reg_selection);
            return this->RegisterIO(reg_addr, read_value, true);
        }

        /**
         * @brief Read single register (use offset).
         *
         * @param offset register offset (must aligned to 4 bytes).
         * @param read_value read value pointer.
         *
         * @retval true: success, false: failed.
         */
        bool ReadSingleRegister(uint32_t offset, uint32_t *read_value)
        {
            PARAM_CHECK(offset % sizeof(uint32_t) == 0, "fpga", " in [FPGADeviceTemplate::ReadSingleRegister] Offset must aligned to 4 bytes.");
            uint32_t reg_addr = offset + this->bar_offset;
            return this->RegisterIO(reg_addr, read_value, true);
        }

        /**
         * @brief Write single register (use register selection).
         *
         * @param reg_selection register to write.
         * @param write_value write value.
         *
         * @retval true: success, false: failed.
         */
        bool WriteSingleRegister(REG_SEL_ENUM reg_selection, uint32_t write_value)
        {
            uint32_t reg_addr = this->GetRegisterAbsoluteAddress(reg_selection);
            return this->RegisterIO(reg_addr, &write_value, false);
        }

        /**
         * @brief Write single register (use offset).
         *
         * @param offset register offset (must aligned to 4 bytes).
         * @param write_value write value.
         *
         * @retval true: success, false: failed.
         */
        bool WriteSingleRegister(uint32_t offset, uint32_t write_value)
        {
            PARAM_CHECK(offset % sizeof(uint32_t) == 0, "fpga", " in [FPGADeviceTemplate::WriteSingleRegister] Offset must aligned to 4 bytes.");
            uint32_t reg_addr = offset + this->bar_offset;
            return this->RegisterIO(reg_addr, &write_value, false);
        }

        /**
         * @brief Read one bit of certain register.
         *
         * @param reg_selection register to operate.
         * @param bit bit select (valid value: 0, 1, ..., 31).
         * @param value read result.
         *
         * @retval true: success, false: failed.
         */
        bool ReadSingleRegisterBIT(REG_SEL_ENUM reg_selection, uint32_t bit, uint32_t *value)
        {
            uint32_t reg_addr = this->GetRegisterAbsoluteAddress(reg_selection);
            return this->ReadSingleRegisterBIT(reg_addr, bit, value);
        }

        /**
         * @brief Read one bit of certain register.
         *
         * @param reg_addr register address.
         * @param bit bit select (valid value: 0, 1, ..., 31).
         * @param value read result.
         *
         * @retval true: success, false: failed.
         */
        bool ReadSingleRegisterBIT(uint32_t reg_addr, uint32_t bit, uint32_t *value)
        {
            PARAM_CHECK(bit <= 31, "fpga", " in [FPGADeviceTemplate::ReadSingleRegisterBIT] Invalid Bit position (valid <= 31).");
            bool operate_status = true;
            uint32_t r_val;
            operate_status &= this->RegisterIO(reg_addr, &r_val, true); /* read */
            *value = FPGA_REG_BIT(r_val, bit);
            return operate_status;
        }

        /**
         * @brief Set/Clear one bit of certain register.
         *
         * @param reg_selection register to operate.
         * @param bit bit select (valid value: 0, 1, ..., 31).
         * @param isSet true: set this bit to 1, false: set this bit to 0.
         *
         * @retval true: success, false: failed.
         */
        bool WriteSingleRegisterBIT(REG_SEL_ENUM reg_selection, uint32_t bit, bool isSet)
        {
            PARAM_CHECK(bit <= 31, "fpga", " in [FPGADeviceTemplate::WriteSingleRegisterBIT] Invalid Bit position (valid <= 31).");
            bool operate_status = true;
            uint32_t r_val, w_val;
            uint32_t reg_addr = this->GetRegisterAbsoluteAddress(reg_selection);
            operate_status &= this->RegisterIO(reg_addr, &r_val, true); /* read */
            if (isSet)
            {
                w_val = FPGA_SET_REG_BIT(r_val, bit);
            }
            else
            {
                w_val = FPGA_CLEAR_REG_BIT(r_val, bit);
            }
            operate_status &= this->RegisterIO(reg_addr, &w_val, false);
            return operate_status;
        }

        /**
         * @brief Wait for register bit to certain value.
         *
         * @param reg_selection register to operate.
         * @param bit bit select (valid value: 0, 1, ..., 31).
         * @param wait_for_value 0 or 1.
         * @param timeout_us timeout (unit: us).
         *
         * @retval true: success, false: failed.
         */
        bool WaitForRegisterBIT(REG_SEL_ENUM reg_selection, uint32_t bit, uint32_t wait_for_value, uint32_t timeout_us = 1000)
        {
            PARAM_CHECK(bit <= 31, "fpga", " in [FPGADeviceTemplate::WaitForRegisterBIT] Invalid Bit position (valid <= 31).");
            uint32_t waitTime = 0;
            uint32_t r_val;
            uint32_t reg_offset = this->GetRegisterAbsoluteAddress(reg_selection);
            bool operate_status = true;
            if (timeout_us <= 0)
            {
                timeout_us = 100;
            }
            do
            {
                operate_status &= this->ReadSingleRegisterBIT(reg_offset, bit, &r_val);
                if (r_val == wait_for_value)
                    break;
                if (waitTime > timeout_us)
                    break;
                usleep(100);
                waitTime += 100;
            } while (r_val != wait_for_value);
            return operate_status & (r_val == wait_for_value);
        }

        /**
         * @brief Read value to bits of certain register.
         *
         * @param reg_selection register to operate.
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
        bool ReadSingleRegisterBITRegion(REG_SEL_ENUM reg_selection, uint32_t lower, uint32_t upper, uint32_t *value)
        {
            PARAM_CHECK(lower <= 31 && upper <= 31, "fpga", " in [FPGADeviceTemplate::ReadSingleRegisterBITRegion] Invalid Bit position (valid <= 31).");
            if (lower == upper)
            {
                return this->ReadSingleRegisterBIT(reg_selection, lower, value);
            }
            bool operate_status = true;
            uint32_t r_val;
            uint32_t reg_addr = this->GetRegisterAbsoluteAddress(reg_selection);
            operate_status &= this->RegisterIO(reg_addr, &r_val, true); /* read */
            uint32_t bit_count = upper - lower + 1;
            uint32_t mask = ((1 << bit_count) - 1) << lower;
            *value = (r_val & mask) >> lower;
            return operate_status;
        }

        /**
         * @brief Write value to bits of certain register.
         *
         * @param reg_selection register to operate.
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
        bool WriteSingleRegisterBITRegion(REG_SEL_ENUM reg_selection, uint32_t lower, uint32_t upper, uint32_t value)
        {
            PARAM_CHECK(lower <= 31 && upper <= 31, "fpga", " in [FPGADeviceTemplate::WriteSingleRegisterBITRegion] Invalid Bit position (valid <= 31).");
            if (lower == upper)
            {
                return this->WriteSingleRegisterBIT(reg_selection, lower, (bool)(value & 0x01));
            }
            bool operate_status = true;
            uint32_t r_val, w_val;
            uint32_t reg_addr = this->GetRegisterAbsoluteAddress(reg_selection);
            operate_status &= this->RegisterIO(reg_addr, &r_val, true); /* read */
            uint32_t bit_count = upper - lower + 1;
            uint32_t mask = ((1 << bit_count) - 1) << lower;
            uint32_t max_value = (1 << bit_count) - 1;
            uint32_t safe_value = value & max_value;
            w_val = (r_val & ~mask) | (safe_value << lower);
            operate_status &= this->RegisterIO(reg_addr, &w_val, false);
            return operate_status;
        }

        /* ----------------------------- Multiple Register IO ----------------------------- */

        /**
         * @brief Read multiple register.
         *
         * @param mul_reg_selection register to read.
         * @param mul_read_value read value pointer.
         *
         * @retval true: success, false: failed.
         */
        bool ReadMultipleRegister(const std::vector<REG_SEL_ENUM> &mul_reg_selection, std::vector<uint32_t> *mul_read_value)
        {
            return this->RegisterIO(mul_reg_selection, mul_read_value, true);
        }

        /**
         * @brief Write multiple register.
         *
         * @param reg_selection register to write.
         * @param write_value write value.
         *
         * @retval true: success, false: failed.
         */
        bool WriteMultipleRegister(const std::vector<REG_SEL_ENUM> &mul_reg_selection, const std::vector<uint32_t> &mul_write_value)
        {
            std::vector<uint32_t> write_value_list = mul_write_value;
            return this->RegisterIO(mul_reg_selection, &write_value_list, false);
        }

        /**
         * @brief Read all register info from device.
         *
         * @param register_name output register name.
         * @param reg_offset output register offset.
         * @param read_value output register value.
         *
         * @retval true: success.
         * @retval false: failed.
         */
        bool ReadAllRegisters(std::vector<std::string> *register_name, std::vector<uint32_t> *reg_offset, std::vector<uint32_t> *read_value)
        {
            std::vector<REG_SEL_ENUM> mul_reg_selection;
            int reg_num;
            {
                std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
                reg_num = this->register_table.size();
                register_name->resize(reg_num);
                reg_offset->resize(reg_num);
                mul_reg_selection.resize(reg_num);
                for (int i = 0; i < reg_num; i++)
                {
                    mul_reg_selection[i] = this->register_table[i].enumVal;
                    (*register_name)[i] = this->register_table[i].name;
                    (*reg_offset)[i] = *this->register_table[i].offsetVal;
                }
            }
            this->ReadMultipleRegister(mul_reg_selection, read_value);
            return true;
        }

        /* ----------------------------- Hardware Interrupt IO ----------------------------- */

        /**
         * @brief Read hardware interrupt.
         *
         * @param read_value read value (1: interrupt detected, 0: no interrupt).
         *
         * @retval true: success.
         * @retval false: failed.
         */
        bool ReadEvent(uint32_t *read_value)
        {
            return this->EventIO(read_value);
        }

        /**
         * @brief Set interrupt timeout.
         *
         * @param timeout_ms timeout in milliseconds (for select).
         *
         * @retval true: success.
         * @retval false: failed.
         */
        bool SetInterruptTimeout(uint32_t timeout_ms)
        {
            PARAM_CHECK(this->is_io_manager_bind_interrupt, "fpga", " in [FPGADeviceTemplate::SetInterruptTimeout] FPGA event file manager is NULL.");
            std::shared_ptr<vuprs::FPGA_IOManagerForInterrput> manager;
            {
                std::unique_lock<std::mutex> lock(this->mut_event); /* LOCK */
                manager = this->bind_io_manager_interrput.lock();
            }
            if (!manager)
                return false;
            return manager->SetTimeout(timeout_ms);
        }

        /* ------------------------------- Device flag ----------------------------------- */

        bool ConfigDone() const
        {
            return this->config_done;
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
        std::atomic<bool> is_io_manager_bind;
        std::weak_ptr<vuprs::FPGA_IOManagerForMemory> bind_io_manager_h2c;
        std::weak_ptr<vuprs::FPGA_IOManagerForMemory> bind_io_manager_c2h;
        std::string h2c_control_device_filename, c2h_control_device_filename;

        bool BufferIO(uint32_t offset, vuprs::AlignedBufferDMA *buffer, uint64_t transfer_byte_size, bool is_read)
        {
            /* Security Check */
            PARAM_CHECK(this->is_io_manager_bind, "fpga", " in [FPGAMemoryTemplate::BufferIO] FPGA file manager is NULL.");
            PARAM_CHECK(transfer_byte_size <= __LINUX_DMA_MAX_TRANSFER_BYTES__, "fpga", " in [FPGAMemoryTemplate::BufferIO] Too big transfer size.");
            PARAM_CHECK((transfer_byte_size + offset) <= static_cast<uint32_t>(this->max_capacity_kB * __KILOBYTES__ - 1), "fpga", " in [FPGAMemoryTemplate::BufferIO] Invalid transfer size (valid: <= " + std::to_string(this->max_capacity_kB * __KILOBYTES__ - offset) + ")");
            if (transfer_byte_size <= 0)
            {
                return true;
            }
            PARAM_CHECK(buffer != nullptr, "fpga", " in [FPGAMemoryTemplate::BufferIO] *Buffer is nullptr.");
            /* Operation */
            uint32_t target_offset = offset + this->fpga_address;
            if (is_read)
            {
                buffer->malloc(transfer_byte_size);
                std::shared_ptr<vuprs::FPGA_IOManagerForMemory> c2h_manager;
                {
                    std::unique_lock<std::mutex> lock(this->mut_c2h); /* LOCK */
                    c2h_manager = this->bind_io_manager_c2h.lock();
                }
                return c2h_manager->BufferIO(buffer->data(), target_offset, transfer_byte_size, true);
            }
            else
            {
                std::shared_ptr<vuprs::FPGA_IOManagerForMemory> h2c_manager;
                {
                    std::unique_lock<std::mutex> lock(this->mut_h2c); /* LOCK */
                    h2c_manager = this->bind_io_manager_h2c.lock();
                }
                return h2c_manager->BufferIO(buffer->data(), target_offset, transfer_byte_size, false);
            }
        }

        bool WordIO(uint32_t offset, uint32_t *io_value, bool is_read)
        {
            /* Security Check */
            PARAM_CHECK(this->is_io_manager_bind, "fpga", " in [FPGAMemoryTemplate::WordIO] FPGA file manager is NULL.");
            PARAM_CHECK(offset <= static_cast<uint32_t>(this->max_capacity_kB * __KILOBYTES__ - 1), "fpga", " in [FPGAMemoryTemplate::WordIO] Invalid offset (valid: <= " + std::to_string(this->max_capacity_kB * __KILOBYTES__ - 1) + ")");
            PARAM_CHECK(io_value != nullptr, "fpga", " in [FPGAMemoryTemplate::WordIO] *read_value is nullptr.");
            /* Operation */
            uint32_t target_offset = offset + this->fpga_address;
            if (is_read)
            {
                std::shared_ptr<vuprs::FPGA_IOManagerForMemory> c2h_manager;
                {
                    std::unique_lock<std::mutex> lock(this->mut_c2h); /* LOCK */
                    c2h_manager = this->bind_io_manager_c2h.lock();
                }
                return c2h_manager->BufferIO(io_value, target_offset, sizeof(uint32_t), true);
            }
            else
            {
                std::shared_ptr<vuprs::FPGA_IOManagerForMemory> h2c_manager;
                {
                    std::unique_lock<std::mutex> lock(this->mut_h2c); /* LOCK */
                    h2c_manager = this->bind_io_manager_h2c.lock();
                }
                return h2c_manager->BufferIO(io_value, target_offset, sizeof(uint32_t), false);
            }
        }

    protected:
        FPGABus data_bus;
        std::atomic<uint32_t> fpga_address;
        std::atomic<uint32_t> max_capacity_kB;

        mutable std::mutex mut;     /* Global mutex lock */
        mutable std::mutex mut_c2h; /* C2H mutex lock */
        mutable std::mutex mut_h2c; /* H2C mutex lock */

        std::atomic<bool> config_done;

        bool LoadMainInfoFromJsonObj(const nlohmann::json &obj)
        {
            std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
            uint32_t fpga_address, max_capacity_kB;
            vuprs::__JsonStringParseINT<uint32_t>(&fpga_address, obj, "fpga-address", true);
            vuprs::__JsonStringParseINT<uint32_t>(&max_capacity_kB, obj, "memory-capacity-kilobytes", true);
            vuprs::__JsonParseString(&this->h2c_control_device_filename, obj, "h2c-device-file", true);
            vuprs::__JsonParseString(&this->c2h_control_device_filename, obj, "c2h-device-file", true);
            this->fpga_address = fpga_address;
            this->max_capacity_kB = max_capacity_kB;
            return true;
        }

    public:
        FPGAMemoryTemplate()
        {
            {
                std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
                this->data_bus = vuprs::FPGABus::AXI_FULL;
                this->fpga_address = 0;
                this->max_capacity_kB = 1;
                this->config_done = false;
                this->is_io_manager_bind = false;
            }
            {
                std::unique_lock<std::mutex> lock(this->mut_c2h); /* LOCK */
                this->bind_io_manager_c2h.reset();
            }
            {
                std::unique_lock<std::mutex> lock(this->mut_h2c); /* LOCK */
                this->bind_io_manager_h2c.reset();
            }
        }

        virtual ~FPGAMemoryTemplate() {}
        virtual bool LoadFromJsonObj(const nlohmann::json &obj) = 0;

        std::string H2C_ControlDeviceFilename() const
        {
            std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
            return this->h2c_control_device_filename;
        }

        std::string C2H_ControlDeviceFilename() const
        {
            std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
            return this->c2h_control_device_filename;
        }

        /**
         * @brief Memory capacity in bytes.
         */
        uint32_t MaxSizeBytes() const
        {
            std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
            return this->max_capacity_kB * __KILOBYTES__;
        }

        /**
         * @brief Bind FPGA file manager.
         */
        bool BindFPGAFileManager(std::shared_ptr<vuprs::FPGA_IOManagerForMemory> io_manager_h2c,
                                 std::shared_ptr<vuprs::FPGA_IOManagerForMemory> io_manager_c2h)
        {
            if (io_manager_h2c == nullptr || io_manager_c2h == nullptr)
            {
                return false;
            }
            std::shared_ptr<vuprs::FPGA_IOManagerForMemory> current_h2c;
            std::shared_ptr<vuprs::FPGA_IOManagerForMemory> current_c2h;
            {
                std::unique_lock<std::mutex> lock(this->mut_h2c); /* LOCK */
                current_h2c = this->bind_io_manager_h2c.lock();
            }
            {
                std::unique_lock<std::mutex> lock(this->mut_c2h); /* LOCK */
                current_c2h = this->bind_io_manager_c2h.lock();
            }
            if (this->is_io_manager_bind)
            {
                if ((current_h2c && current_h2c != io_manager_h2c) || (current_c2h && current_c2h != io_manager_c2h))
                {
                    this->UnbindFileManager();
                }
            }
            {
                std::unique_lock<std::mutex> lock(this->mut_h2c); /* LOCK */
                this->bind_io_manager_h2c = io_manager_h2c;
            }
            {
                std::unique_lock<std::mutex> lock(this->mut_c2h); /* LOCK */
                this->bind_io_manager_c2h = io_manager_c2h;
            }
            this->is_io_manager_bind = true;
            return true;
        }

        /**
         * @brief Unbind FPGA file manager.
         */
        void UnbindFileManager()
        {
            {
                std::unique_lock<std::mutex> lock(this->mut_h2c); /* LOCK */
                this->bind_io_manager_h2c.reset();
            }
            {
                std::unique_lock<std::mutex> lock(this->mut_c2h); /* LOCK */
                this->bind_io_manager_c2h.reset();
            }
            this->is_io_manager_bind = false;
        }

        /**
         * @brief Read data from memory to buffer.
         *
         * @note The function can automaticaly allocate the buffer.
         *
         * @param buffer the buffer to store data.
         * @param offset offset from base address of the memory.
         * @param transfer_byte_size transfer size in bytes.
         *
         * @retval true: success, false: failed.
         *
         * @throw std::runtime_error
         */
        bool ReadMemory(vuprs::AlignedBufferDMA *buffer, uint32_t offset, uint64_t transfer_byte_size)
        {
            return this->BufferIO(offset, buffer, transfer_byte_size, true);
        }

        /**
         * @brief Write buffer data to memory.
         *
         * @note The buffer must be allocated in advance.
         *
         * @param buffer data buffer.
         * @param offset offset from base address of the memory.
         * @param transfer_byte_size transfer size in bytes.
         *
         * @retval true: success, false: failed.
         *
         * @throw std::runtime_error
         */
        bool WriteMemory(vuprs::AlignedBufferDMA *buffer, uint32_t offset, uint64_t transfer_byte_size)
        {
            return this->BufferIO(offset, buffer, transfer_byte_size, false);
        }

        /**
         * @brief Read 4 bytes data from memory.
         *
         * @param offset offset from base address of the memory.
         * @param read_value read value.
         *
         * @retval true: success, false: failed.
         *
         * @throw std::runtime_error
         */
        bool ReadMemory(uint32_t offset, uint32_t *read_value)
        {
            return this->WordIO(offset, read_value, true);
        }

        /**
         * @brief Write 4 bytes data to memory.
         *
         * @param offset offset from base address of the memory.
         * @param write_value write value.
         *
         * @retval true: success, false: failed.
         *
         * @throw std::runtime_error
         */
        bool WriteMemory(uint32_t offset, uint32_t write_value)
        {
            return this->WordIO(offset, &write_value, false);
        }

        bool ConfigDone() const
        {
            return this->config_done;
        }

        uint32_t FPGAAddress() const
        {
            return this->fpga_address;
        }
    };
}

#endif
