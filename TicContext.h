#pragma once

/**
   @brief Context for boot data
*/
typedef struct {
  uint32_t      startupTime;          /*!< Initial startup time (in ms), corresponding to the time when setup() is invoked after boot */
  uint8_t       nbDotsProgress;       /*!< The number of dots on the progress bar while entering to programming mode */
} ctx_boot_t;

typedef enum {
  TIC_NO_SYNC = 0,
  TIC_IN_SYNC
} tic_state_t;

/**
   @brief Context for TIC data & measurements
*/
typedef struct {
  uint32_t      lastValidSinsts;      /*!< Last known value for SINSTS */
  uint32_t      lateTicDecodeCount;   /*!< How many late TIC decode events occurred since startup */
  uint32_t      ticUpdates;           /*!< The total number of TIC updates received from the meter */
  uint32_t      lostTicBytes;         /*!< How many TIC bytes were skipped by decoder */
  uint32_t      nbFramesParsed;       /*!< Decoded TIC frames counter */
  tic_state_t   lastTicDecodeState;   /*!< The last known TIC decoding state */
  bool          beat;                 /*!< Heartbeat (toggled between true and false for each received TIC frame) */
} ctx_tic_t;

/**
   @brief Context for LCD display data
*/
typedef struct {
  
} ctx_lcd_t;

/**
   @brief Global context
*/
typedef struct {
  ctx_tic_t     tic;                  /*!< TIC & measurements-related context values */
  ctx_boot_t    boot;                 /*!< Boot-related context values */
  uint32_t      displayedPower;       /*!< Currently displayed power value */
  uint8_t       stage;                /*!< The current stage in the startup state machine */
  uint8_t       charsOnLine0;         /*!< Number of characters displayed at the left of the first line on the display (used to efficiently clear data) */
} g_ctx_t;
