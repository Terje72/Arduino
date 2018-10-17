/*
 Esp.h - ESP8266-specific APIs
 Copyright (c) 2015 Ivan Grokhotkov. All rights reserved.
 This file is part of the esp8266 core for Arduino environment.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef ESP_H
#define ESP_H

#include <Arduino.h>

/**
 * AVR macros for WDT managment
 */
typedef enum {
    WDTO_0MS    = 0,   //!< WDTO_0MS
    WDTO_15MS   = 15,  //!< WDTO_15MS
    WDTO_30MS   = 30,  //!< WDTO_30MS
    WDTO_60MS   = 60,  //!< WDTO_60MS
    WDTO_120MS  = 120, //!< WDTO_120MS
    WDTO_250MS  = 250, //!< WDTO_250MS
    WDTO_500MS  = 500, //!< WDTO_500MS
    WDTO_1S     = 1000,//!< WDTO_1S
    WDTO_2S     = 2000,//!< WDTO_2S
    WDTO_4S     = 4000,//!< WDTO_4S
    WDTO_8S     = 8000 //!< WDTO_8S
} WDTO_t;


#define wdt_enable(time)    ESP.wdtEnable(time)
#define wdt_disable()       ESP.wdtDisable()
#define wdt_reset()         ESP.wdtFeed()

#define cli()        ets_intr_lock()       // IRQ Disable
#define sei()        ets_intr_unlock()     // IRQ Enable

enum RFMode {
    RF_DEFAULT = 0, // RF_CAL or not after deep-sleep wake up, depends on init data byte 108.
    RF_CAL = 1,      // RF_CAL after deep-sleep wake up, there will be large current.
    RF_NO_CAL = 2,   // no RF_CAL after deep-sleep wake up, there will only be small current.
    RF_DISABLED = 4 // disable RF after deep-sleep wake up, just like modem sleep, there will be the smallest current.
};

#define RF_MODE(mode) int __get_rf_mode() { return mode; }
#define RF_PRE_INIT() void __run_user_rf_pre_init()

// compatibility definitions
#define WakeMode RFMode
#define WAKE_RF_DEFAULT  RF_DEFAULT
#define WAKE_RFCAL       RF_CAL
#define WAKE_NO_RFCAL    RF_NO_CAL
#define WAKE_RF_DISABLED RF_DISABLED

enum ADCMode {
    ADC_TOUT = 33,
    ADC_TOUT_3V3 = 33,
    ADC_VCC = 255,
    ADC_VDD = 255
};

#define ADC_MODE(mode) int __get_adc_mode(void) { return (int) (mode); }

typedef enum {
     FM_QIO = 0x00,
     FM_QOUT = 0x01,
     FM_DIO = 0x02,
     FM_DOUT = 0x03,
     FM_UNKNOWN = 0xff
} FlashMode_t;

class EspClass {
    public:
        // TODO: figure out how to set WDT timeout
        void wdtEnable(uint32_t timeout_ms = 0);
        // note: setting the timeout value is not implemented at the moment
        void wdtEnable(WDTO_t timeout_ms = WDTO_0MS);

        void wdtDisable();
        void wdtFeed();

        void deepSleep(uint64_t time_us, RFMode mode = RF_DEFAULT);
        void deepSleepInstant(uint64_t time_us, RFMode mode = RF_DEFAULT);
        uint64_t deepSleepMax();

        bool rtcUserMemoryRead(uint32_t offset, uint32_t *data, size_t size);
        bool rtcUserMemoryWrite(uint32_t offset, uint32_t *data, size_t size);

        void reset();
        void restart();

        uint16_t getVcc();
        uint32_t getChipId();

        uint32_t getFreeHeap();
        uint16_t getMaxFreeBlockSize();
        uint8_t getHeapFragmentation(); // in %
        void getHeapStats(uint32_t* free = nullptr, uint16_t* max = nullptr, uint8_t* frag = nullptr);

        uint32_t getFreeContStack();
        void resetFreeContStack();

        const char * getSdkVersion();
        String getCoreVersion();
        String getFullVersion();

        uint8_t getBootVersion();
        uint8_t getBootMode();

        uint8_t getCpuFreqMHz();

        uint32_t getFlashChipId();
        //gets the actual chip size based on the flash id
        uint32_t getFlashChipRealSize();
        //gets the size of the flash as set by the compiler
        uint32_t getFlashChipSize();
        uint32_t getFlashChipSpeed();
        FlashMode_t getFlashChipMode();
        uint32_t getFlashChipSizeByChipId();

        uint32_t magicFlashChipSize(uint8_t byte);
        uint32_t magicFlashChipSpeed(uint8_t byte);
        FlashMode_t magicFlashChipMode(uint8_t byte);

        bool checkFlashConfig(bool needsEquals = false);

        bool flashEraseSector(uint32_t sector);
        bool flashWrite(uint32_t offset, uint32_t *data, size_t size);
        bool flashRead(uint32_t offset, uint32_t *data, size_t size);

        uint32_t getSketchSize();
        String getSketchMD5();
        uint32_t getFreeSketchSpace();
        bool updateSketch(Stream& in, uint32_t size, bool restartOnFail = false, bool restartOnSuccess = true);

        String getResetReason();
        String getResetInfo();
        struct rst_info * getResetInfoPtr();

        bool eraseConfig();

        inline uint32_t getCycleCount();

        bool getAutoFormatSPIFFS ();
        void setAutoFormatSPIFFS (bool autoFormat);
};

uint32_t EspClass::getCycleCount()
{
    uint32_t ccount;
    __asm__ __volatile__("esync; rsr %0,ccount":"=a" (ccount));
    return ccount;
}

extern EspClass ESP;

#endif //ESP_H
