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
  int32_t       lastValidWithdrawnPower;      /*!< Last known withdrawn power (if negative, we are actually injecting power), or INT32_MIN if unknown */
  uint32_t      lateTicDecodeCount;   /*!< How many late TIC decode events occurred since startup */
  uint32_t      ticUpdates;           /*!< The total number of TIC updates received from the meter */
  uint32_t      lostTicBytes;         /*!< How many TIC bytes were skipped by decoder */
  uint32_t      nbFramesParsed;       /*!< Decoded TIC frames counter */
  bool          beat;                 /*!< Heartbeat (toggled between true and false for each received TIC frame) */
  tic_state_t   lastTicDecodeState;   /*!< The last known TIC decoding state */
} ctx_tic_t;

/**
   @brief Context for LCD display data
*/
typedef struct { /*FIXME: new, implement algo fully using this data */
  int32_t       displayedPower;       /*!< Currently displayed power value */
  uint8_t       nbCharsOnLine0;       /*!< Number of characters displayed at the left of the first line on the display (used to efficiently clear data) */
  char          charsOnLine0[16+1];   /*!< List of characters displayed on the first LCD line+'\0' to terminate the string if the line is full */
} ctx_lcd_t;

/**
   @brief Global context
*/
typedef struct {
  uint8_t       stage;                /*!< The current stage in the startup state machine */
  ctx_tic_t     tic;                  /*!< TIC & measurements-related context values */
  ctx_lcd_t     lcd;                  /*!< LCD display-related context values */
  ctx_boot_t    boot;                 /*!< Boot-related context values */
} g_ctx_t;
