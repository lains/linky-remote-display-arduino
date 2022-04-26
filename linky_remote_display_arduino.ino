//#define SERIAL_RX_BUFFER_SIZE 512
#include <Wire.h>
#include <Adafruit_RGBLCDShield.h>
#include <utility/Adafruit_MCP23017.h>
#include <avr/wdt.h>
#include <LibTeleinfo.h>

Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

// These #defines make it easier to set the backlight color
#define RED 0x1
#define YELLOW 0x3
#define GREEN 0x2
#define TEAL 0x6
#define BLUE 0x4
#define VIOLET 0x5
#define WHITE 0x7

#define MONOCHROME_DISPLAY

#ifndef MONOCHROME_DISPLAY
#define BACK_LIGHT_ON TEAL
#else
#define BACK_LIGHT_ON RED // MONO backlight ON is on RED line
#endif

#define ELEC_ICON_IDX 0
#define FULL_SQUARE_IDX 1
/* Icon created with https://www.quinapalus.com/hd44780udg.html (character size: 5x8) */
uint8_t icons[2][8] = { { 0x8, 0x4, 0x2, 0x4, 0x8, 0x5, 0x3, 0x7 },
                        { 0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f}
                      };

constexpr uint16_t NO_SERIAL_INIT_TIMEOUT_MS=5000; /*!< How long (in ms) should we delay (since boot time) the serial setup when a user action is performed at boot time? (max value=655350/LCD_WIDTH, thus 40s) */
constexpr uint16_t TIC_PROBE_FAIL_TIMEOUT_MS=10000; /*!< How long (in ms) should we wait (since boot time)for TIC synchronization before issuing a warning messages */
#define LCD_WIDTH 16
#define LCD_HEIGHT 2

#define uint32ToNbDigits(i) (i<10?1:(i<100?2:(i<1000?3:(i<10000?4:5))))
/**
 * Boot progress and TIC decoding state machine
 */
#define STAGE_WAIT_RELEASE_INIT 0
#define STAGE_WAIT_RELEASE_CHAR_CREATED 1
#define STAGE_WAIT_RELEASE_DISPLAY_DONE 2
#define STAGE_SERIAL_INIT 10
#define STAGE_TIC_INIT 11
#define STAGE_TIC_PROBE 20  /*!< Waiting to start synchronization with TIC stream */
#define STAGE_TIC_SYNC_FAIL 21  /*!< No TIC stream or impossible to decode TIC stream */
#define STAGE_TIC_IN_SYNC 30 /*!< Synchronized to TIC stream, currently receiving TIC data */
#define STAGE_TIC_IN_SYNC_RUNNING_LATE 31 /*!< Synchronized to TIC stream, currently receiving TIC data but we are running late in decoding incoming bytes */

/**
 * @brief Context for boot data
 */
typedef struct {
  uint32_t      startupTime;          /*!< Initial startup time (in ms), corresponding to the time when setup() is invoked after boot */
  uint8_t       nbDotsProgress;       /*!< The number of dots on the progress bar while entering to programming mode */
} ctx_boot_t;

/**
 * @brief Context for TIC data & measurements
 */
typedef struct {
  uint32_t      lastValidSinsts;      /*!< Last known value for SINSTS */
  uint32_t      lateTicDecodeCount;   /*!< How many late TIC decode events occurred since startup */
  uint32_t      ticUpdates;           /*!< The total number of TIC updates received from the meter */
  _State_e      lastTicDecodeState;   /*!< The last known TIC decoding state */
  bool          beat;                 /*!< Heartbeat (toggled between true and false for each received TIC frame) */
} ctx_tic_t;

/**
 * @brief Context for LCD display data
 */
typedef struct {
} ctx_lcd_t;

/**
 * @brief Global context
 */
typedef struct {
  ctx_tic_t     tic;                  /*!< TIC & measurements-related context values */
  ctx_boot_t    boot;                 /*!< Boot-related context values */
  uint32_t      displayedPower;       /*!< Currently displayed power value */
  uint8_t       stage;                /*!< The current stage in the startup state machine */
  uint8_t       charsOnLine0;         /*!< Number of characters displayed at the left of the first line on the display (used to efficiently clear data) */
} g_ctx_t;

g_ctx_t ctx ; /*!< Global context storage */


TInfo tinfo; // TIC decoder instance

void(*swReset) (void) = 0;  /*!< declare reset fuction at address 0 */

/**
 * @brief Setup the board at initial boot
 */
void setup() {
  Serial.begin(9600); /* Ephemeral serial init for program updload */
  ctx.boot.startupTime = millis();
  ctx.boot.nbDotsProgress = 0;
  ctx.tic.lateTicDecodeCount = 0;
  ctx.tic.ticUpdates = 0;
  ctx.tic.lastValidSinsts = -1;
  ctx.displayedPower = -1;
  ctx.tic.lastTicDecodeState = (_State_e)(-1);
  ctx.charsOnLine0 = LCD_WIDTH; /* LCD display assumed to be full of characters to clear */
  ctx.tic.beat = false;
  pinMode(LED_BUILTIN, OUTPUT);
  lcd.begin(LCD_WIDTH, LCD_HEIGHT); /* Initialize the LCD display: 16x2 */
  lcd.setBacklight(WHITE);
  if (lcd.readButtons() & BUTTON_SELECT) {
    /* BUTTON_SELECT pressed at startup, do not start the serial port if button is maintained */
    ctx.stage = STAGE_WAIT_RELEASE_INIT;
    /* Keep button pressed to swtch to programming mode */
    lcd.setCursor(0, 0);
    lcd.print("Programming mode");
  }
  else {
    ctx.stage = STAGE_SERIAL_INIT;
  }
}

/**
 * @brief Enter an infinite chase effect display on the LCD when switched to upload mode
 */
void infiniteLoopWaitUpdate() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Waiting 4 upload");
  while (1) {
    for (unsigned int i=0; i<LCD_WIDTH-1; i++) {
      lcd.setCursor(i, 1);
      lcd.print('*');
      delay(1000);
      lcd.setCursor(i, 1);
      lcd.print(' ');
    }
    for (unsigned int i=LCD_WIDTH-1; i>0; i--) {
      lcd.setCursor(i, 1);
      lcd.print('*');
      delay(1000);
      lcd.setCursor(i, 1);
      lcd.print(' ');
    }
  }
}

/**
 * @brief Display a progress bar when progressing towards upload mode at startup
 * 
 * @note This requires continuous user interaction (override) to continue progression towards upload mode, and should be called repeatedly in a loop
 * @warning If upload mode is reached, this function will never return. If progressing to upload mode, this function will update the display and return
 */
void bootModeCheckAndProgressDisplay(ctx_boot_t &boot_ctx) {
  int timeNow = millis();
  if (timeNow > boot_ctx.startupTime) {
    uint16_t timeDelta = timeNow - boot_ctx.startupTime;
    if (timeDelta < NO_SERIAL_INIT_TIMEOUT_MS) {
      /* Note we divide both timeDelta and NO_SERIAL_INIT_TIMEOUT_MS by 10 here to avoid 16-bit overflow when multiplying by timeDelta (uint16_t) by LCD_WIDTH */
      uint8_t currentNbDotsProgress = ((uint16_t)timeDelta/10 * LCD_WIDTH) / (NO_SERIAL_INIT_TIMEOUT_MS/10);
      if (boot_ctx.nbDotsProgress < currentNbDotsProgress) {
        lcd.setCursor(currentNbDotsProgress-1, 1);
        lcd.write(FULL_SQUARE_IDX);
        boot_ctx.nbDotsProgress = currentNbDotsProgress;
      }
    }
    else {  /* Delayed serial action has been maintained from the very start of boot and during the whole NO_SERIAL_INIT_TIMEOUT_MS period, assume we will stay in the bootloader without decoding TIC */
      Serial.println("Stay in boot");
      infiniteLoopWaitUpdate();
      swReset();
      while (true) {}
    }
  } /* else there was a count error, continue booting */
}

/**
 * @brief Process new TIC data received from the meter
 * @param me linked list pointer on the concerned data
 * @param tic The context for TIC data & measurements
 * @param flags Current flags value
 */
void processTicIncomingData(ValueList* me, ctx_tic_t& tic, uint8_t flags) {
  uint32_t sinsts = (uint32_t)(-1);
  uint8_t charsDrawnAtLine1;

  /* Parse through incoming data */
  ValueList* datap= me;
  int count = 0;
  while (datap != nullptr) {
    if (strcmp("SINSTS", datap->name) == 0) {
      sinsts = strtol(datap->value, NULL, 10);
    }
    datap = datap->next;
    count++;
  }

  /*
  if (flags & TINFO_FLAGS_ADDED) {
    lcd.print('+');
  }

  if (flags & TINFO_FLAGS_UPDATED) {
    lcd.print('!');
  }
  */
  if (sinsts != (uint32_t)(-1)) { /* We got a proper value for sinsts */
    tic.lastValidSinsts = sinsts;
    //lcd.setCursor(15, 0); /* Last character on first line */
    if (tic.beat) {
      //lcd.write(ELEC_ICON_IDX);
      digitalWrite(LED_BUILTIN, HIGH);
    }
    else {
      //lcd.print(' ');
      digitalWrite(LED_BUILTIN, LOW);
    }
    tic.beat = !tic.beat;
  }
  /*
  lcd.setCursor(0, 1);
  lcd.print(calls);
  lcd.print('>');
  lcd.print(count);
  lcd.print(' ');
  */
}

/**
 * @brief Callback function invoked when new of modified data is received
 * @param me linked list pointer on the concerned data
 * @param flags Current flags value
 */
void ticNewDataCallback(ValueList* me, uint8_t flags) {
  ctx.tic.ticUpdates++;
  if (ctx.stage != STAGE_TIC_IN_SYNC)
    return;

  wdt_reset();
  processTicIncomingData(me, ctx.tic, flags);
  wdt_reset();
}

/**
 * @brief Update the values displayed on the LCD screen
 * @param currentTicDecodeState The current TIC decoding state
 * @param ctx The current context
 */
void updateDisplay(uint8_t currentTicDecodeState, g_ctx_t& ctx) {
  if (currentTicDecodeState == TINFO_READY) {
    if (ctx.stage < STAGE_TIC_IN_SYNC) {
      lcd.clear();  /* Switching to sync then to power display mode */
      ctx.stage = STAGE_TIC_IN_SYNC;
    }
    else if (ctx.stage == STAGE_TIC_IN_SYNC) {  /* Display only if STAGE_TIC_IN_SYNC, not even when STAGE_TIC_IN_SYNC_RUNNING_LATE, as we don't have enought time to  */
      if (ctx.tic.lastValidSinsts != -1) {
        if (ctx.displayedPower != ctx.tic.lastValidSinsts) {
          uint8_t displayedChars = uint32ToNbDigits(ctx.tic.lastValidSinsts) + 1; /* +1 for the W symbol */
          if (displayedChars > LCD_WIDTH) /* Foolproof */
            displayedChars = LCD_WIDTH;
          lcd.setCursor(0, 0);
          lcd.print(ctx.tic.lastValidSinsts);
          lcd.print('W');
          if (ctx.charsOnLine0 < displayedChars) {
            ctx.charsOnLine0 = displayedChars;
          }
          else {
            while (ctx.charsOnLine0 > displayedChars) {  /* We have previous chars to clean up */
              lcd.print(' ');
              ctx.charsOnLine0--;
            }
          }
          wdt_reset();
        }
        ctx.displayedPower = ctx.tic.lastValidSinsts;
      }
    }
  }
}

/**
 * @brief Main loop
 */
void loop() {
  if (ctx.stage < STAGE_SERIAL_INIT) {
    while (lcd.readButtons() & BUTTON_SELECT) {
      if (ctx.stage < STAGE_WAIT_RELEASE_CHAR_CREATED) {
        lcd.createChar(FULL_SQUARE_IDX, icons[FULL_SQUARE_IDX]);
        ctx.stage = STAGE_WAIT_RELEASE_CHAR_CREATED;
      }
      bootModeCheckAndProgressDisplay(ctx.boot);
    } /* No button pressed before end of timeout, continue booting */
    ctx.stage = STAGE_SERIAL_INIT;  /* Delayed serial action has been stopped before the end of NO_SERIAL_INIT_TIMEOUT_MS period, continue serial initialization */
  }
  if (ctx.stage == STAGE_SERIAL_INIT) {
    Serial.println("Swicthing serial port to TIC");
    Serial.begin(9600, SERIAL_7E1);
    wdt_enable(WDTO_500MS);
    lcd.createChar(ELEC_ICON_IDX, icons[ELEC_ICON_IDX]);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Syncing...");
    ctx.stage = STAGE_TIC_PROBE;
    tinfo.init(TINFO_MODE_STANDARD);
    tinfo.attachData(ticNewDataCallback);
//    lcd.setCursor(0, 1);
  }
  if (ctx.stage >= STAGE_TIC_PROBE) {
    wdt_reset();
    int waitingRxBytes = Serial.available();  /* Is there incoming data pending, and how much (in bytes) */
    if (waitingRxBytes > 0) {
      if (ctx.tic.lastTicDecodeState == TINFO_READY && ctx.stage == STAGE_TIC_IN_SYNC && waitingRxBytes > (SERIAL_RX_BUFFER_SIZE*3/4)) { /* Less that 1/4 of incoming buffer is available, we are running late */
        ctx.stage = STAGE_TIC_IN_SYNC_RUNNING_LATE;
        ctx.tic.lateTicDecodeCount++;
      }
      _State_e newTicDecodeState = tinfo.process(Serial.read());
      wdt_reset();
      updateDisplay(newTicDecodeState, ctx);
      ctx.tic.lastTicDecodeState = newTicDecodeState;
    }
    else {  /* No waiting RX byte... we processed every TIC byte, we are running early */
      if (ctx.stage == STAGE_TIC_IN_SYNC_RUNNING_LATE)
        ctx.stage = STAGE_TIC_IN_SYNC;
    }
    if (ctx.stage == STAGE_TIC_PROBE && ctx.tic.lastTicDecodeState != TINFO_READY) {
      if (millis() - ctx.boot.startupTime > TIC_PROBE_FAIL_TIMEOUT_MS) {
        ctx.stage = STAGE_TIC_SYNC_FAIL;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("No signal!");
        digitalWrite(LED_BUILTIN, HIGH);
        wdt_reset();
      }
    }
  }
}
