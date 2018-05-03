#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "spi_master_lobo.h"
#include "ssd1306.h"

static const spi_lobo_host_device_t SPIHost = TFT_HSPI_HOST;

#define SPIFrequency ( 1 * 1000 * 1000 )
#define MOSIPin 13
#define SCLKPin 14
#define DCPin 4

static const int SSD1306_SPI_Command_Mode = 0;
static const int SSD1306_SPI_Data_Mode = 1;

static bool SPIMultiDisplayWriteBytes( spi_lobo_device_handle_t SPIHandle, int CSPin, int WriteMode, const uint8_t* Data, size_t DataLength );
static bool SPIMultiDisplayWriteCommand( struct SSD1306_Device* DeviceHandle, SSDCmd Command );
static bool SPIMultiDisplayWriteData( struct SSD1306_Device* DeviceHandle, const uint8_t* Data, size_t DataLength );
static bool SPIDefaultReset( struct SSD1306_Device* DeviceHandle );

static spi_lobo_bus_config_t BusConfig = {
    .sclk_io_num = SCLKPin,
    .mosi_io_num = MOSIPin,
    .miso_io_num = -1,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 4094
};

bool SPIMultiDisplayInitDefault( void ) {
    ESP_ERROR_CHECK_NONFATAL( gpio_set_direction( DCPin, GPIO_MODE_OUTPUT ), return false );
    ESP_ERROR_CHECK_NONFATAL( gpio_set_level( DCPin, 1 ), return false );

    return true;
}


bool SPIMultiAttachDisplayDefault( struct SSD1306_Device* DeviceHandle, int Width, int Height, int CSForThisDisplay, int RSTForThisDisplay ) {
    spi_lobo_device_interface_config_t SPIDeviceConfig;
    spi_lobo_device_handle_t SPIDeviceHandle;

    NullCheck( DeviceHandle, return false );

    memset( &SPIDeviceConfig, 0, sizeof( spi_lobo_device_interface_config_t ) );

    gpio_pad_select_gpio( CSForThisDisplay );

    SPIDeviceConfig.clock_speed_hz = SPIFrequency;
    SPIDeviceConfig.spics_io_num = -1;
    SPIDeviceConfig.spics_ext_io_num = CSForThisDisplay;

    if ( RSTForThisDisplay >= 0 ) {
        ESP_ERROR_CHECK_NONFATAL( gpio_set_direction( RSTForThisDisplay, GPIO_MODE_OUTPUT ), return false );
        ESP_ERROR_CHECK_NONFATAL( gpio_set_level( RSTForThisDisplay, 0 ), return false );
    }

    ESP_ERROR_CHECK_NONFATAL( spi_lobo_bus_add_device( SPIHost, &BusConfig, &SPIDeviceConfig, &SPIDeviceHandle ), return false );

    spi_lobo_set_speed( SPIDeviceHandle, 1 * 1000 * 1000 );

    return SSD1306_Init_SPI( DeviceHandle,
        Width,
        Height,
        RSTForThisDisplay,
        CSForThisDisplay,
        ( void* ) SPIDeviceHandle,
        SPIMultiDisplayWriteCommand,
        SPIMultiDisplayWriteData,
        SPIDefaultReset
    );
}

static bool SPIMultiDisplayWriteBytes( spi_lobo_device_handle_t SPIHandle, int CSPin, int WriteMode, const uint8_t* Data, size_t DataLength ) {
    spi_lobo_transaction_t SPITransaction;

    NullCheck( SPIHandle, return false );
    NullCheck( Data, return false );

    if ( DataLength > 0 ) {
        memset( &SPITransaction, 0, sizeof( spi_lobo_transaction_t ) );

        SPITransaction.length = DataLength * 8;
        SPITransaction.tx_buffer = Data;

        ESP_ERROR_CHECK_NONFATAL( gpio_set_level( DCPin, WriteMode ), return false );

        ESP_ERROR_CHECK_NONFATAL( spi_lobo_device_select( SPIHandle, 1 ), return false );
            ESP_ERROR_CHECK_NONFATAL( spi_lobo_transfer_data( SPIHandle, &SPITransaction ), return false );
        ESP_ERROR_CHECK_NONFATAL( spi_lobo_device_deselect( SPIHandle ), return false );
    }

    return true;
}

static bool SPIMultiDisplayWriteCommand( struct SSD1306_Device* DeviceHandle, SSDCmd Command ) {
    static uint8_t CommandByte = 0;

    NullCheck( DeviceHandle, return false );
    NullCheck( DeviceHandle->SPIHandle, return false );

    CommandByte = Command;
    return SPIMultiDisplayWriteBytes( ( spi_lobo_device_handle_t ) DeviceHandle->SPIHandle, DeviceHandle->CSPin, SSD1306_SPI_Command_Mode, &CommandByte, 1 );
}

static bool SPIMultiDisplayWriteData( struct SSD1306_Device* DeviceHandle, const uint8_t* Data, size_t DataLength ) {
    NullCheck( DeviceHandle, return false );
    NullCheck( DeviceHandle->SPIHandle, return false );

    return SPIMultiDisplayWriteBytes( ( spi_lobo_device_handle_t ) DeviceHandle->SPIHandle, DeviceHandle->CSPin, SSD1306_SPI_Data_Mode, Data, DataLength );
}

static bool SPIDefaultReset( struct SSD1306_Device* DeviceHandle ) {
    NullCheck( DeviceHandle, return false );

    if ( DeviceHandle->RSTPin >= 0 ) {
        ESP_ERROR_CHECK_NONFATAL( gpio_set_level( DeviceHandle->RSTPin, 0 ), return false );
            vTaskDelay( pdMS_TO_TICKS( 100 ) );
        ESP_ERROR_CHECK_NONFATAL( gpio_set_level( DeviceHandle->RSTPin, 1 ), return false );
    }

    return true;
}
