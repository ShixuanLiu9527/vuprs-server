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

#ifndef _WIN32
#include <sys/mman.h>
#endif

#include <fcntl.h>

#if __BYTE_ORDER == __LITTLE_ENDIAN
    #define ltohl(x)               (x)
    #define ltohs(x)               (x)
    #define htoll(x)               (x)
    #define htols(x)               (x)
#elif __BYTE_ORDER == __BIG_ENDIAN
    #define ltohl(x)     __bswap_32(x)
    #define ltohs(x)     __bswap_16(x)
    #define htoll(x)     __bswap_32(x)
    #define htols(x)     __bswap_16(x)
#endif

#ifdef _WIN32
    #define O_SYNC 0
    #define PROT_READ 0
    #define PROT_WRITE 0
    #define MAP_SHARED 0
    #define MAP_FAILED 0
#endif

#define __AXI_LITE_DEVICE_COUNT__   6U  /* must equal to AXI-Lite device number */
#define __XDMA_AXI_LITE_MMAP_SIZE__ (__AXI_LITE_DEVICE_COUNT__ * 64U * 1024UL)  /* N * 64 kB address in VUPRS FPGA AXI-Lite bus address space */

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
            std::string deviceFilename;
            std::mutex mut;

            virtual int OpenFlags() = 0;

            /**
             * @brief Operation after opened.
             * 
             * @note e.g. mmap().
             */
            virtual bool OperationAfterOpened() = 0;

        public:

            FPGA_IOManagerBase(const FPGA_IOManagerBase&) = delete;
            FPGA_IOManagerBase& operator=(const FPGA_IOManagerBase&) = delete;

            FPGA_IOManagerBase(const std::string &deviceFilename);
            FPGA_IOManagerBase();
            virtual ~FPGA_IOManagerBase();

            /**
             * @brief Open FPGA device file.
             * 
             * @param deviceFilename xdma device file.
             * 
             * @retval true: success, false: failed.
             */
            bool Open(const std::string &deviceFilename) noexcept;

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
    class FPGA_IOManagerForDevice: public FPGA_IOManagerBase
    {
        private:

            std::atomic<bool> isMemoryMapped;
            void *mmap_base;  /* memory map base address */

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
            ~FPGA_IOManagerForDevice();

            /**
             * @brief Read/Write value from/to FPGA register.
             * 
             * @note 1st: mmap(), 2nd: read/write, 3rd: munmap().
             * @note AXI-Lite only, cannot be used in AXI-Full reading/writing.
             * 
             * @param ioValue read/write value.
             * @param absoluteAddress address of the register in FPGA (= PCIe BAR-address + register offset).
             * @param isRead true: read, false: write.
             * 
             * @retval true: success, false: failed.
             * 
             * @throw std::runtime_error
             */
            bool RegisterIO(uint32_t* ioValue, uint32_t absoluteAddress, bool isRead);

            /**
             * @brief Read/Write value list from/to FPGA registers.
             * 
             * @note 1st: mmap(), 2nd: read/write, 3rd: munmap().
             * @note AXI-Lite only, cannot be used in AXI-Full reading/writing.
             * 
             * @param ioValueList read/write value list.
             * @param absoluteAddressList address list of registers in FPGA (= PCIe BAR-address + register offset).
             * @param isRead true: read, false: write.
             * 
             * @retval true: success, false: failed.
             * 
             * @throw std::runtime_error
             */
            bool RegisterListIO(std::vector<uint32_t> *ioValueList, const std::vector<uint32_t> &absoluteAddressList, bool isRead);
    };

    /**
     * @brief IO Manager for memories.
     * 
     * @note Thread safety.
     */
    class FPGA_IOManagerForMemory: public FPGA_IOManagerBase
    {
        protected:

            int OpenFlags() override;
            bool OperationAfterOpened() override;

        public:

            /**
             * @brief Read/Write data from FPGA/buffer to buffer/FPGA.
             * 
             * @note 1st: lseek(), 2nd: read()/write().
             * @note AXI-Full only, cannot be used in AXI-Lite reading/writing.
             * 
             * @param source buffer.data()
             * @param absoluteAddress starting addressing for reading/writing (= FPGA-address of memory + user offset).
             * @param transferBytes transfer size in bytes.
             * @param isRead true: read, false: write.
             * 
             * @retval true: success, false: failed.
             * 
             * @throw std::runtime_error
             */
            bool BufferIO(void* source, uint32_t absoluteAddress, uint32_t transferBytes, bool isRead);
    };

    /**
     * @brief IO Manager for interrupt.
     * 
     * @note Thread safety.
     */
    class FPGA_IOManagerForInterrput: public FPGA_IOManagerBase
    {
        protected:

            int OpenFlags() override;
            bool OperationAfterOpened() override;

        public:

            bool ReadEvent(uint32_t *readValue);
    };
}

#endif
