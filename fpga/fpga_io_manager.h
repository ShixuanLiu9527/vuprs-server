#ifndef FPGA_IO_MANAGER_H
#define FPGA_IO_MANAGER_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <fcntl.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/select.h>
#include <sys/mman.h>
#endif

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define ltohl(x) (x)
#define ltohs(x) (x)
#define htoll(x) (x)
#define htols(x) (x)
#elif __BYTE_ORDER == __BIG_ENDIAN
#define ltohl(x) __bswap_32(x)
#define ltohs(x) __bswap_16(x)
#define htoll(x) __bswap_32(x)
#define htols(x) __bswap_16(x)
#endif

#ifdef _WIN32
#define O_SYNC 0
#define PROT_READ 0
#define PROT_WRITE 0
#define MAP_SHARED 0
#define MAP_FAILED 0
#endif

#define __AXI_LITE_DEVICE_COUNT__ 6U                                           /* must equal to AXI-Lite device number */
#define __XDMA_AXI_LITE_MMAP_SIZE__ (__AXI_LITE_DEVICE_COUNT__ * 64U * 1024UL) /* N * 64 kB address in VUPRS FPGA AXI-Lite bus address space */

namespace vuprs
{
    /**
     * @brief Base class for IOManager.
     *
     * @note Thread safety.
     */
    class FPGA_IOManagerBase
    {
    protected:
        std::atomic<int> fd;
        std::string device_filename;
        std::mutex mut;

        virtual int OpenFlags() = 0;

        /**
         * @brief Operation after opened.
         *
         * @note e.g. mmap().
         */
        virtual bool OperationAfterOpened() = 0;

    public:
        FPGA_IOManagerBase(const FPGA_IOManagerBase &) = delete;
        FPGA_IOManagerBase &operator=(const FPGA_IOManagerBase &) = delete;

        FPGA_IOManagerBase(const std::string &device_filename);
        FPGA_IOManagerBase();
        virtual ~FPGA_IOManagerBase();

        /**
         * @brief Open FPGA device file.
         *
         * @param device_filename xdma device file.
         *
         * @retval true: success, false: failed.
         */
        bool Open(const std::string &device_filename) noexcept;

        /**
         * @brief Close FPGA device file.
         */
        void Close() noexcept;

        /**
         * @brief Indicate xdma device file open.
         */
        bool IsOpen() const;

        std::string GetDeviceFilename() const;
    };

    /**
     * @brief IO Manager for device registers.
     *
     * @note Thread safety.
     */
    class FPGA_IOManagerForDevice : public FPGA_IOManagerBase
    {
    private:
        std::atomic<bool> is_mmapped;
        void *mmap_base; /* memory map base address */

        /**
         * @brief Do memory map.
         */
        bool MemoryMap();

        /**
         * @brief Do memory unmap.
         */
        bool MemoryUnmap();

        int OpenFlags() override;
        bool OperationAfterOpened() override;

    public:
        FPGA_IOManagerForDevice();
        FPGA_IOManagerForDevice(const std::string &device_filename);
        ~FPGA_IOManagerForDevice();

        /**
         * @brief Read/Write value from/to FPGA register.
         *
         * @note 1st: mmap(), 2nd: read/write, 3rd: munmap().
         * @note AXI-Lite only, cannot be used in AXI-Full reading/writing.
         *
         * @param io_value read/write value.
         * @param absolute_address address of the register in FPGA (= PCIe BAR-address + register offset).
         * @param is_read true: read, false: write.
         *
         * @retval true: success, false: failed.
         *
         * @throw std::runtime_error
         */
        bool RegisterIO(uint32_t *io_value,
                        uint32_t absolute_address,
                        bool is_read);

        /**
         * @brief Read/Write value list from/to FPGA registers.
         *
         * @note 1st: mmap(), 2nd: read/write, 3rd: munmap().
         * @note AXI-Lite only, cannot be used in AXI-Full reading/writing.
         *
         * @param io_value_list read/write value list.
         * @param absolute_address_list address list of registers in FPGA (= PCIe BAR-address + register offset).
         * @param is_read true: read, false: write.
         *
         * @retval true: success, false: failed.
         *
         * @throw std::runtime_error
         */
        bool RegisterListIO(std::vector<uint32_t> *io_value_list,
                            const std::vector<uint32_t> &absolute_address_list,
                            bool is_read);
    };

    /**
     * @brief IO Manager for memories.
     *
     * @note Thread safety.
     */
    class FPGA_IOManagerForMemory : public FPGA_IOManagerBase
    {
    protected:
        int OpenFlags() override;
        bool OperationAfterOpened() override;

    public:
        FPGA_IOManagerForMemory();
        FPGA_IOManagerForMemory(const std::string &device_filename);
        ~FPGA_IOManagerForMemory();

        /**
         * @brief Read/Write data from FPGA/buffer to buffer/FPGA.
         *
         * @note 1st: lseek(), 2nd: read()/write().
         * @note AXI-Full only, cannot be used in AXI-Lite reading/writing.
         *
         * @param source buffer.data()
         * @param absolute_address starting addressing for reading/writing (= FPGA-address of memory + user offset).
         * @param transferBytes transfer size in bytes.
         * @param is_read true: read, false: write.
         *
         * @retval true: success, false: failed.
         *
         * @throw std::runtime_error
         */
        bool BufferIO(void *source, uint32_t absolute_address, uint32_t transferBytes, bool is_read);
    };

    /**
     * @brief IO Manager for interrupt.
     *
     * @note Thread safety.
     */
    class FPGA_IOManagerForInterrput : public FPGA_IOManagerBase
    {
    protected:
        std::atomic<uint32_t> timeout_ms;

        int OpenFlags() override;
        bool OperationAfterOpened() override;

    public:
        FPGA_IOManagerForInterrput();
        FPGA_IOManagerForInterrput(const std::string &device_filename);
        ~FPGA_IOManagerForInterrput();

        bool SetTimeout(uint32_t timeout_ms = 100);

        bool ReadEvent(uint32_t *read_value);
    };
}

#endif
