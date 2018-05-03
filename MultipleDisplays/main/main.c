/**
 * Copyright (c) 2017-2018 Tara Keeling
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "ssd1306.h"
#include "ssd1306_draw.h"
#include "ssd1306_font.h"
#include "ssd1306_default_if.h"

extern bool SPIMultiAttachDisplayDefault( struct SSD1306_Device* DeviceHandle, int Width, int Height, int CSForThisDisplay, int RSTForThisDisplay );
extern bool SPIMultiDisplayInitDefault( void );

static const int I2CDisplayAddress = 0x3C;
static const int I2CDisplayWidth = 128;
static const int I2CDisplayHeight = 64;
static const int I2CResetPin = -1;

struct SSD1306_Device I2CDisplay;

static const int SPIDisplayWidth = 128;
static const int SPIDisplayHeight = 64;
static const int SPIResetPin = -1;

struct SSD1306_Device SPIDisplay0;
struct SSD1306_Device SPIDisplay1;
struct SSD1306_Device SPIDisplay2;
struct SSD1306_Device SPIDisplay3;

static const int SPIDisplay0CS = 15;
static const int SPIDisplay1CS = 5;
static const int SPIDisplay2CS = 16;
static const int SPIDisplay3CS = 26;

void SetupDemo( struct SSD1306_Device* DisplayHandle, const struct SSD1306_FontDef* Font );
void SayHello( struct SSD1306_Device* DisplayHandle, const char* HelloText, ... );

bool DefaultBusInit( void ) {
    assert( SSD1306_I2CMasterInitDefault( ) == true );
    assert( SSD1306_I2CMasterAttachDisplayDefault( &I2CDisplay, I2CDisplayWidth, I2CDisplayHeight, I2CDisplayAddress, I2CResetPin ) == true );
    
    //assert( SSD1306_SPIMasterInitDefault( ) == true );
    //assert( SSD1306_SPIMasterAttachDisplayDefault( &SPIDisplay0, 128, 64, 15, -1 ) == true );

    assert( SPIMultiDisplayInitDefault( ) == true );
    assert( SPIMultiAttachDisplayDefault( &SPIDisplay0, 128, 64, SPIDisplay0CS, 17 ) == true );
    assert( SPIMultiAttachDisplayDefault( &SPIDisplay1, 128, 64, SPIDisplay1CS, -1 ) == true );
    assert( SPIMultiAttachDisplayDefault( &SPIDisplay2, 128, 64, SPIDisplay2CS, -1 ) == true );
    assert( SPIMultiAttachDisplayDefault( &SPIDisplay3, 128, 64, SPIDisplay3CS, -1 ) == true );

    return true;
}

void SetupDemo( struct SSD1306_Device* DisplayHandle, const struct SSD1306_FontDef* Font ) {
    SSD1306_Clear( DisplayHandle, SSD_COLOR_BLACK );
    SSD1306_SetFont( DisplayHandle, Font );

    SSD1306_SetHFlip( DisplayHandle, true );
    SSD1306_SetVFlip( DisplayHandle, true );

    //SSD1306_SetContrast( DisplayHandle, 0xFF );
}

void SayHello( struct SSD1306_Device* DisplayHandle, const char* HelloText, ... ) {
    static char Buffer[ 256 ];
    va_list Argp;

    va_start( Argp, HelloText );
        vsnprintf( Buffer, 256, HelloText, Argp );
    va_end( Argp );

    SSD1306_Clear( DisplayHandle, SSD_COLOR_BLACK );
    SSD1306_FontDrawAnchoredString( DisplayHandle, TextAnchor_Center, Buffer, SSD_COLOR_WHITE );
    SSD1306_Update( DisplayHandle );
}

void app_main( void ) {
    int i = 0;

    printf( "Ready...\n" );

    if ( DefaultBusInit( ) == true ) {
        printf( "BUS Init lookin good...\n" );
        printf( "Drawing.\n" );

        SetupDemo( &I2CDisplay, &Font_droid_sans_fallback_16x17 );

        SetupDemo( &SPIDisplay0, &Font_droid_sans_fallback_16x17 );
        SetupDemo( &SPIDisplay1, &Font_droid_sans_fallback_16x17 );
        SetupDemo( &SPIDisplay2, &Font_droid_sans_fallback_16x17 );
        SetupDemo( &SPIDisplay3, &Font_droid_sans_fallback_16x17 );

        while ( true ) {
            SayHello( &I2CDisplay, "I2C0: %d", i );

            SayHello( &SPIDisplay0, "SPI0: %d", i );
            SayHello( &SPIDisplay1, "SPI1: %d", i );
            SayHello( &SPIDisplay2, "SPI2: %d", i );
            SayHello( &SPIDisplay3, "SPI3: %d", i );

            i++;
            vTaskDelay( pdMS_TO_TICKS( 16 ) );
        }

        printf( "Done!\n" );
    }
}
