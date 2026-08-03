#ifndef VUPRS_CONFIG_H
#define VUPRS_CONFIG_H

/* -------------------------------------------------------------------- */
/* ------------------------- Debug options ---------------------------- */
/* -------------------------------------------------------------------- */

/**
 * @brief Enable custom debug information.
 */
#define DEBUG false

/**
 * @defgroup Debug files.
 */
#define DEBUG_FILES_ROOT_DIR "./debug-data"               /* Directory for all debug files */
#define DEBUG_FILES_DIR "data"                            /* Directory for debug files */
#define CIRCULAR_BUFFER_DEBUG_FILENAME "multi-signal.csv" /* Circular buffer debug filename (.csv) */
#define FIR_RESULT_DEBUG_FILENAME "bf-result.csv"         /* FIR result debug filename (.csv) */
#define FIR_RESULT_BIN_DEBUG_FILENAME "bf-result-bin.bin" /* FIR result debug filename (.bin) */
#define FIR_COEF_DEBUG_FILENAME "fir-coef.csv"            /* FIR coefficients debug filename (bin) */
#define FIR_COEF_BIN_DEBUG_FILENAME "fir-coef-bin.bin"    /* FIR coefficients debug filename (.csv) */
/**
 * @}
 */

#define DEBUG_DATA_GROUP_COUNT 10 /* Debug data group count */

/* -------------------------------------------------------------------- */
/* --------------------- Algorithm parameters ------------------------- */
/* -------------------------------------------------------------------- */

/**
 * @defgroup Physical parameters.
 */
#define DEFAULT_WAVE_VELOCITY 346.0 /* Wave velocity of the sound */
/**
 * @}
 */

/**
 * @defgroup Default scanning parameters.
 * @{
 */
#define DEFAULT_SCANNING_POINTS_IN_HALF 100 /* Number of Fibonacci points in the scanning hemisphere */
#define DEFAULT_SCANNING_ALTITUDE_MIN 15.0  /* Minium altitude angle for scanning */
/**
 * @}
 */

/**
 * @defgroup Covariance matrix fitting parameters.
 * @{
 */
#define DEFAULT_COVARIANCE_SNAP_WINDOW_SIZE 100 /* Equivalent snapshot window size of covariance matrix EMA */
#define DEFAULT_ADJACENT_FREQ_AVERAGE_INDEX 0.8 /* Adjacent frequency average index of covariance estimation */
/**
 * @}
 */

#define VUPRS_CXX_STANDARD__11 11U
#define VUPRS_CXX_STANDARD__14 14U
#define VUPRS_CXX_STANDARD__17 17U
#define VUPRS_CXX_STANDARD__20 20U
#if defined(_MSC_VER) && !defined(__clang__)
#ifndef _MSVC_LANG
#define VUPRS_CXX_STANDARD VUPRS_CXX_STANDARD__11
#elif _MSVC_LANG >= 202002L
#define VUPRS_CXX_STANDARD VUPRS_CXX_STANDARD__20
#elif _MSVC_LANG >= 201703L
#define VUPRS_CXX_STANDARD VUPRS_CXX_STANDARD__17
#elif _MSVC_LANG >= 201402L
#define VUPRS_CXX_STANDARD VUPRS_CXX_STANDARD__14
#elif _MSVC_LANG >= 201103L
#define VUPRS_CXX_STANDARD VUPRS_CXX_STANDARD__11
#else
#error SOLVER requires at least C++11
#endif
#elif defined(__cplusplus)
#if __cplusplus >= 202002L
#define VUPRS_CXX_STANDARD VUPRS_CXX_STANDARD__20
#elif __cplusplus >= 201703L
#define VUPRS_CXX_STANDARD VUPRS_CXX_STANDARD__17
#elif __cplusplus >= 201402L
#define VUPRS_CXX_STANDARD VUPRS_CXX_STANDARD__14
#elif __cplusplus >= 201103L
#define VUPRS_CXX_STANDARD VUPRS_CXX_STANDARD__11
#else
#error SOLVER requires at least C++11
#endif
#else
#define VUPRS_CXX_STANDARD VUPRS_CXX_STANDARD__11
#warning SOLVER CXX standard is set to default: C++11
#endif
#if __has_include(<filesystem>)
#define VUPRS_HAS_FILESYSTEM true
#else
#define VUPRS_HAS_FILESYSTEM false
#endif

#endif
