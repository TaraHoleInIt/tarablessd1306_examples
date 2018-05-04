/**
 * Copyright (c) 2017-2018 Tara Keeling
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "ssd1306.h"
#include "ssd1306_draw.h"
#include "ssd1306_font.h"
#include "ssd1306_default_if.h"

#define USE_I2C_DISPLAY
#define USE_SPI_DISPLAY

#if defined USE_I2C_DISPLAY
    static const int I2CDisplayAddress = 0x3C;
    static const int I2CDisplayWidth = 128;
    static const int I2CDisplayHeight = 64;
    static const int I2CResetPin = -1;

    struct SSD1306_Device I2CDisplay;
#endif

#if defined USE_SPI_DISPLAY
    static const int SPIDisplayChipSelect = 15;
    static const int SPIDisplayWidth = 128;
    static const int SPIDisplayHeight = 64;
    static const int SPIResetPin = 5;

    struct SSD1306_Device SPIDisplay;
#endif

struct SSD1306_Device* DisplayList[ ] = {
#if defined USE_I2C_DISPLAY
    &I2CDisplay,
#endif
#if defined USE_SPI_DISPLAY
    &SPIDisplay,
#endif
};

const int DisplayCount = sizeof( DisplayList ) / sizeof( DisplayList[ 0 ] );

extern void SnowyFieldTask( void* Param );

bool DefaultBusInit( void ) {
    #if defined USE_I2C_DISPLAY
        assert( SSD1306_I2CMasterInitDefault( ) == true );
        assert( SSD1306_I2CMasterAttachDisplayDefault( &I2CDisplay, I2CDisplayWidth, I2CDisplayHeight, I2CDisplayAddress, I2CResetPin ) == true );
    #endif

    #if defined USE_SPI_DISPLAY
        assert( SSD1306_SPIMasterInitDefault( ) == true );
        assert( SSD1306_SPIMasterAttachDisplayDefault( &SPIDisplay, SPIDisplayWidth, SPIDisplayHeight, SPIDisplayChipSelect, SPIResetPin ) == true );
    #endif

    return true;
}

void app_main( void ) {
    int i = 0;

    printf( "Ready...\n" );

    if ( DefaultBusInit( ) == true ) {
        printf( "BUS Init lookin good...\n" );
        printf( "Drawing.\n" );

        for ( i = 0; i < DisplayCount; i++ ) {
            SSD1306_SetContrast( DisplayList[ i ], 0xFF );

            printf( "Starting demo on display %d\n", i );
            xTaskCreate( SnowyFieldTask, "SnowyFieldTask", 4096, DisplayList[ i ], i, NULL );
        }

        printf( "Done!\n" );
    }
}
