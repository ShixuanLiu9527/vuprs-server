#ifndef VUPRS_CONFIG_H
#define VUPRS_CONFIG_H

/* -------------------------------------------------------------------- */
/* ---------------------- Functional configs -------------------------- */
/* -------------------------------------------------------------------- */

/**
 * @brief Enable inference.
 */
#define ENABLE_INFERENCE true

/* -------------------------------------------------------------------- */
/* ------------------------- Debug options ---------------------------- */
/* -------------------------------------------------------------------- */

/**
 * @brief Enable custom debug information.
 */
#define VUPRS_DEBUG false

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
#define DEFAULT_WAVE_VELOCITY 346.0        /* Wave velocity of the sound */
#define MIN_SAMPLING_FREQUENCY 10.0        /* Minimum supported sampling frequency (Hz) */
#define MIN_BEAMFORMER_FREQUENCY 10.0      /* Minimum lower/upper band edge (Hz) */
#define MIN_COVARIANCE_SNAPSHOT_WINDOW 1.0 /* Minimum covariance EMA snapshot window */
#define MAX_WAVE_VELOCITY 100000.0         /* Engineering upper bound for wave velocity (m/s) */
/**
 * @}
 */

/**
 * @defgroup Default scanning parameters.
 * @{
 */
#define DEFAULT_SCANNING_POINTS_IN_HALF 100 /* Number of Fibonacci points in the scanning hemisphere */
#define MAX_SCANNING_POINTS_IN_HALF 10000   /* Upper bound to avoid unreasonable scan grid memory/time */
#define MIN_SCANNING_ALTITUDE_DEG 10.0      /* Exclusive lower bound of scanning altitude (deg) */
#define DEFAULT_SCANNING_ALTITUDE_MIN 15.0  /* Minium altitude angle for scanning */
#define SCAN_MAX_RATE_HZ 20                 /* Max scan update rate (snapshot capture + power map), Hz */
#define INFERENCE_MAX_RATE_HZ 10            /* Max inference trigger rate, Hz */
/**
 * @}
 */

/**
 * @defgroup Covariance matrix fitting parameters.
 * @{
 */
#define DEFAULT_COVARIANCE_SNAP_WINDOW_SIZE 30.0 /* Equivalent snapshot window size of covariance matrix EMA */
#define DEFAULT_ADJACENT_FREQ_AVERAGE_INDEX 0.8   /* Adjacent frequency average index of covariance estimation */
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
