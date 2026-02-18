#ifndef FPGA_OPERATION_API_H
#define FPGA_OPERATION_API_H

#include "fpga_controller.h"
#include "fpga_data_conversion.h"

namespace vuprs
{
    /* ----------------------------------------------------------------------------- */
    /* ----------------------------- ADC Controller -------------------------------- */
    /* ----------------------------------------------------------------------------- */

    /**
     * @brief Start ADC sampling.
     * 
     * @note Controller must be configured in advance.
     * @note Sampling frequency must smaller than MAX_FS
     * @note It is recommended to start at the very last.
     * 
     * @param controller FPGA controller.
     * @param fs sampling frequency (unit: Hz).
     * 
     * @retval true: success.
     * @retval false: failed.
     * 
     * @throw std::runtime_error
     */
    bool FPGA_API__ADC__StartADC(vuprs::FPGAController *controller, double fs);

    /**
     * @brief Reset ADC.
     */
    bool FPGA_API__ADC__ResetADC(vuprs::FPGAController *controller);

    /* ----------------------------------------------------------------------------- */
    /* ----------------------------- Circular Buffer ------------------------------- */
    /* ----------------------------------------------------------------------------- */

    /**
     * @brief Read circular buffer.
     * 
     * @note Controller must be configured in advance.
     * @note The function will read and reset safely.
     * 
     * @param controller FPGA controller.
     * @param signal output signal data in circular buffer.
     * 
     * @retval true: success.
     * @retval false: failed.
     * 
     * @throw std::runtime_error.
     */
    bool FPGA_API__CBUF__ReadCircularBuffer(vuprs::FPGAController *controller, 
        vuprs::SignalData *signal);

    /* ----------------------------------------------------------------------------- */
    /* ------------------------------ Predelay Unit -------------------------------- */
    /* ----------------------------------------------------------------------------- */

    /**
     * @brief Set predelay for Pre-delay Unit.
     * 
     * @note Controller must be configured in advance.
     * @note All predelay values should be equal or smaller than MAX_PDLY.
     * 
     * @param controller FPGA controller.
     * @param channelPredelay channel predelay values (n * Ts).
     * @param channelName corrsponding channel name.
     * 
     * @retval true: success.
     * @retval false: failed.
     * 
     * @throw std::runtime_error
     */
    bool FPGA_API__PDLY__SetPredelay(vuprs::FPGAController *controller, 
        const std::vector<uint16_t> &channelPredelay, const std::vector<std::string> &channelName);

    /* ----------------------------------------------------------------------------- */
    /* ---------------------------- FIR Filter Bank -------------------------------- */
    /* ----------------------------------------------------------------------------- */

    /**
     * @brief Set new FIR filter bank coefficients.
     * 
     * @note Controller must be configured in advance.
     * @note coefficients[i].size() must equal to FIR_LEN.
     * 
     * @param controller FPGA controller.
     * @param coefficients new coefficients of FIR filter bank (raw data, no need for scale).
     * @param maxAbsoluteCoefficient max absolute coefficients (= max(abs(coefficients))).
     * 
     * @retval true: success.
     * @retval false: failed.
     * 
     * @throw std::runtime_error
     */
    bool FPGA_API__FIR__SetCoefficients(vuprs::FPGAController *controller, 
        std::vector<std::vector<double>> *coefficients, double maxAbsoluteCoefficient);

    /**
     * @brief Set new FIR filter bank coefficients & FIR filter length.
     * 
     * @note Controller must be configured in advance.
     * @note coefficients[i].size() must equal to len.
     * 
     * @param controller FPGA controller.
     * @param coefficients new coefficients of FIR filter bank (raw data, no need for scale).
     * @param maxAbsoluteCoefficient max absolute coefficients (= max(abs(coefficients))).
     * @param len new length of FIR filter (<= MAX_FIR_LEN).
     * 
     * @retval true: success.
     * @retval false: failed.
     * 
     * @throw std::runtime_error
     */
    bool FPGA_API__FIR__SetLengthAndCoefficients(vuprs::FPGAController *controller, 
        std::vector<std::vector<double>> *coefficients, double maxAbsoluteCoefficient, uint32_t len);

    /* ----------------------------------------------------------------------------- */
    /* ------------------------------------ DDR ------------------------------------ */
    /* ----------------------------------------------------------------------------- */

    /**
     * @brief Read data from DDR to buffer.
     * 
     * @note Controller must be configured in advance.
     * 
     * @param controller FPGA controller.
     * @param buffer aligned buffer.
     * @param ddrOffset address offset in DDR (where to start reading).
     * @param transferSize transfer size in bytes.
     * 
     * @retval true: success.
     * @retval false: failed.
     * 
     * @throw std::runtime_error
     */
    bool FPGA_API__FIR__ReadDDR(vuprs::FPGAController *controller, 
        vuprs::AlignedBufferDMA *buffer, uint32_t ddrOffset, uint32_t transferSize);
}

#endif
