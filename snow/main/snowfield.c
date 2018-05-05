#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>
#include <malloc.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "ssd1306.h"
#include "ssd1306_draw.h"

#define NewSnowflakesDelayMS 250
#define NewSnowflakesCount 3

#define DefaultSnowLevel 10
#define MaxSnowflakes 128

#define ValidateParamPtr( ptr, retexpr ) { \
    if ( ptr == NULL ) { \
        ESP_LOGE( "snowfield", "ValidateParamPtr: %s == NULL", #ptr ); \
        retexpr; \
    } \
}

#define ValidateParamRange( value, min, max, retexpr ) { \
    if ( value < min ) { \
        ESP_LOGE( "snowfield", "ValidateParamRange: %s < %s", #value, #min ); \
        retexpr; \
    } \
    if ( value > max ) { \
        ESP_LOGE( "snowfield", "ValidateParamRange: %s > %s", #value, #max ); \
        retexpr; \
    } \
}

#define Clamp( value, min, max ) { \
    value = ( value < min ) ? min : value; \
    value = ( value > max ) ? max : value; \
}\

struct SnowflakeInfo {
    int x;
    int y;
    int dx;
    int dy;
    bool Free;
};

struct SnowflakeTaskData {
    struct SnowflakeInfo* Snowflakes;
    struct SSD1306_Device* Display;
    int* LineHeights;
};

static int64_t TimeInMS( void );
static int RandomRange( int Min, int Max );
static bool InitTaskData( struct SnowflakeTaskData* TaskData, struct SSD1306_Device* Display );
static void ResetSnowflakes( struct SnowflakeTaskData* TaskData );
static void ResetSnowyField( struct SnowflakeTaskData* TaskData, int InitialSnowLevel );
static void DrawSnowyField( struct SnowflakeTaskData* TaskData );
static void DrawSnowflakes( struct SnowflakeTaskData* TaskData );
static bool HasSnowflakeLanded( struct SnowflakeTaskData* TaskData, struct SnowflakeInfo* Flake );
static struct SnowflakeInfo* FindFreeSnowflake( struct SnowflakeTaskData* TaskData );
static struct SnowflakeInfo* CreateSnowflake( struct SnowflakeTaskData* TaskData );

/* TimeInMS:
 * Returns: The current system time in milliseconds.
 */
static int64_t TimeInMS( void ) {
    struct timeval tv = { 0, 0 };

    gettimeofday( &tv, NULL );
    return ( tv.tv_usec / 1000 ) + ( tv.tv_sec * 1000 );
}

/* RandomRange:
 * Returns a random value between (Min) and (Max)
 */
static int RandomRange( int Min, int Max ) {
    return ( int ) ( esp_random( ) % ( Max + 1 - Min ) + Min );
}

/* InitTaskData:
 * This allocates and initialized all the data private to this task.
 * Returns true if successful.
 */
static bool InitTaskData( struct SnowflakeTaskData* TaskData, struct SSD1306_Device* Display ) {
    ValidateParamPtr( TaskData, return false );
    ValidateParamPtr( Display, return false );

    TaskData->Snowflakes = malloc( MaxSnowflakes * sizeof( struct SnowflakeInfo ) );
    TaskData->LineHeights = malloc( Display->Width * sizeof( int ) );
    TaskData->Display = Display;

    if ( TaskData->Snowflakes != NULL && TaskData->LineHeights != NULL ) {
        return true;
    }

    if ( TaskData->Snowflakes != NULL ) {
        free( TaskData->Snowflakes );
    }

    if ( TaskData->LineHeights != NULL ) {
        free( TaskData->LineHeights );
    }

    return false;
}

/* ResetSnowflakes:
 * Sets all snowflake entries to 0,0 with no movement and marks it as available.
 */
static void ResetSnowflakes( struct SnowflakeTaskData* TaskData ) {
    int i = 0;

    ValidateParamPtr( TaskData, return );
    ValidateParamPtr( TaskData->Snowflakes, return );

    for ( i = 0; i < MaxSnowflakes; i++ ) {
        TaskData->Snowflakes[ i ].x = 0;
        TaskData->Snowflakes[ i ].y = 0;
        TaskData->Snowflakes[ i ].dx = 0;
        TaskData->Snowflakes[ i ].dy = 0;
        TaskData->Snowflakes[ i ].Free = true;
    }
}

/* ResetSnowyField:
 * Initializes all line heights to (InitialSnowLevel)
 */
static void ResetSnowyField( struct SnowflakeTaskData* TaskData, int InitialSnowLevel ) {
    int i = 0;

    ValidateParamPtr( TaskData, return );
    ValidateParamPtr( TaskData->LineHeights, return );
    ValidateParamPtr( TaskData->Display, return );
    ValidateParamRange( InitialSnowLevel, 0, TaskData->Display->Height - 1, return );

    for ( i = 0; i < TaskData->Display->Width; i++ ) {
        TaskData->LineHeights[ i ] = InitialSnowLevel;
    }
}

/* DrawSnowyField:
 * Draws the ground, should be redone.
 */
static void DrawSnowyField( struct SnowflakeTaskData* TaskData ) {
    int i = 0;

    ValidateParamPtr( TaskData, return );
    ValidateParamPtr( TaskData->LineHeights, return );
    ValidateParamPtr( TaskData->Display, return );

    for ( i = 0; i < TaskData->Display->Width; i++ ) {
        /* HACKHACKHACK:
         * SSD1306_DrawVLine should be able to accept negative
         * height values, this would look a lot better then.
         */
        SSD1306_DrawVLine( 
            TaskData->Display,
            i,
            TaskData->Display->Height - 1 - TaskData->LineHeights[ i ],
            TaskData->LineHeights[ i ],
            SSD_COLOR_WHITE
        );
    }
}

/* DrawSnowflakes:
 * Draws all snowflakes that are marked as active.
 */
static void DrawSnowflakes( struct SnowflakeTaskData* TaskData ) {
    int i = 0;

    ValidateParamPtr( TaskData, return );
    ValidateParamPtr( TaskData->Snowflakes, return );
    ValidateParamPtr( TaskData->LineHeights, return );
    ValidateParamPtr( TaskData->Display, return );

    for ( i = 0; i < MaxSnowflakes; i++ ) {
        if ( TaskData->Snowflakes[ i ].Free == false ) {
            SSD1306_DrawPixel( TaskData->Display,
                TaskData->Snowflakes[ i ].x,
                TaskData->Snowflakes[ i ].y,
                SSD_COLOR_WHITE
            );
        }
    }
}

/* UpdateSnowflakes:
 * Advances all active snowflakes according to their delta x/y.
 * If they have landed on the ground then reset them and mark as active
 * for use again.
 * 
 * TODO:
 * Moving like real snow.
 * Removing any snowflakes that may have moved offscreen.
 */
static void UpdateSnowflakes( struct SnowflakeTaskData* TaskData ) {
    struct SnowflakeInfo* Flake = NULL;
    int NeighbourLeft = 0;
    int NeighbourRight = 0;
    int i = 0;

    ValidateParamPtr( TaskData, return );
    ValidateParamPtr( TaskData->Snowflakes, return );
    ValidateParamPtr( TaskData->LineHeights, return );

    for ( i = 0; i < MaxSnowflakes; i++ ) {
        Flake = &TaskData->Snowflakes[ i ];

        if ( Flake->Free == false ) {
            Flake->y+= Flake->dy;

            if ( HasSnowflakeLanded( TaskData, Flake ) == true ) {
                NeighbourLeft = ( Flake->x > 0 ) ? TaskData->LineHeights[ Flake->x - 1 ] : 0;
                NeighbourRight = ( Flake->x < ( TaskData->Display->Width - 1 ) ) ? TaskData->LineHeights[ Flake->x + 1 ] : 0;

                if ( Flake->x > 0 && Flake->x < ( TaskData->Display->Width - 1 ) ) {
                    if ( ( TaskData->LineHeights[ Flake->x ] - NeighbourLeft ) >= 2 ) {
                        TaskData->LineHeights[ Flake->x - 1 ]+= 1;
                    }

                    if ( ( TaskData->LineHeights[ Flake->x ] - NeighbourRight ) >= 2 ) {
                        TaskData->LineHeights[ Flake->x + 1 ]+= 1;
                    }

                    Clamp( TaskData->LineHeights[ Flake->x - 1 ], DefaultSnowLevel, TaskData->Display->Height - 1 );
                    Clamp( TaskData->LineHeights[ Flake->x + 1 ], DefaultSnowLevel, TaskData->Display->Height - 1 );
                }             

                TaskData->LineHeights[ Flake->x ]+= 1;
                Clamp( TaskData->LineHeights[ Flake->x ], DefaultSnowLevel, TaskData->Display->Height - 1 );

                Flake->x = 0;
                Flake->y = 0;
                Flake->dx = 0;
                Flake->dy = 0;
                Flake->Free = true;
            }
        }
    }
}

/* UpdateDisplay:
 * Clears the display then draws the snowy field and snowflakes.
 */
static void UpdateDisplay( struct SnowflakeTaskData* TaskData ) {
    ValidateParamPtr( TaskData, return );
    ValidateParamPtr( TaskData->Display, return );

    SSD1306_Clear( TaskData->Display, SSD_COLOR_BLACK );
        DrawSnowyField( TaskData );
        DrawSnowflakes( TaskData );
    SSD1306_Update( TaskData->Display );
}

/* HasSnowflakeLanded:
 * Returns true if (Flake) has touched the ground by checking if
 * it's y coordinate is equal to the line height at it's x coordinate.
 */
static bool HasSnowflakeLanded( struct SnowflakeTaskData* TaskData, struct SnowflakeInfo* Flake ) {
    int LineY = 0;

    ValidateParamPtr( TaskData, return false );
    ValidateParamPtr( TaskData->Snowflakes, return false );
    ValidateParamPtr( Flake, return false );
    ValidateParamPtr( TaskData->LineHeights, return false );

    LineY = TaskData->Display->Height - 1 - TaskData->LineHeights[ Flake->x ];
    return ( Flake->y >= LineY ) ? true : false;
}

/* FindFreeSnowflake:
 * Returns the first snowflake that has it's Free variable set to true.
 * Returns NULL if no free snowflakes are available.
 */
static struct SnowflakeInfo* FindFreeSnowflake( struct SnowflakeTaskData* TaskData ) {
    int i = 0;

    ValidateParamPtr( TaskData, return NULL );
    ValidateParamPtr( TaskData->Snowflakes, return NULL );

    for ( i = 0; i < MaxSnowflakes; i++ ) {
        if ( TaskData->Snowflakes[ i ].Free == true ) {
            return &TaskData->Snowflakes[ i ];
        }
    }

    return NULL;
}

/* CreateSnowflake:
 * Finds an unused snowflake object and intializes it's
 * coordinates as well as which direction it will be moving.
 * Returns snowflake object or NULL if there were no free snowflake
 * object.
 */
static struct SnowflakeInfo* CreateSnowflake( struct SnowflakeTaskData* TaskData ) {
    struct SnowflakeInfo* Flake = NULL;

    ValidateParamPtr( TaskData, return NULL );
    ValidateParamPtr( TaskData->Snowflakes, return NULL );

    if ( ( Flake = FindFreeSnowflake( TaskData ) ) != NULL ) {
        Flake->x = RandomRange( 0, TaskData->Display->Width - 1 );
        Flake->y = RandomRange( -3, 2 );
        Flake->dx = 0;
        Flake->dy = 1;
        Flake->Free = false;

        //printf( "New snowflake!\n" );
    }

    return Flake;
}

void SnowyFieldTask( void* Param ) {
    struct SnowflakeTaskData TaskData;
    int64_t NextSnowflakesTimeMS = 0;
    int64_t TimeNowMS = 0;
    int i = 0;

    ValidateParamPtr( Param, vTaskDelete( NULL ); return );

    if ( InitTaskData( &TaskData, ( struct SSD1306_Device* ) Param ) == true ) {
        ResetSnowflakes( &TaskData );
        ResetSnowyField( &TaskData, DefaultSnowLevel );

        NextSnowflakesTimeMS = TimeInMS( ) + NewSnowflakesDelayMS;

        while ( true ) {
            TimeNowMS = TimeInMS( );

            //printf( "Time: %d\n", ( int ) TimeNowMS );

            if ( TimeNowMS >= NextSnowflakesTimeMS ) {
                for ( i = 0; i < NewSnowflakesCount; i++ ) {
                    CreateSnowflake( &TaskData );
                }

                NextSnowflakesTimeMS = TimeNowMS + NewSnowflakesDelayMS;
            }

            UpdateSnowflakes( &TaskData );
            UpdateDisplay( &TaskData );

            /* This should animate at roughly 30fps, less so on i2c displays. */
            vTaskDelay( pdMS_TO_TICKS( 33 ) );
        }
    }

    printf( "Finished!\n" );
    vTaskDelete( NULL );
}
