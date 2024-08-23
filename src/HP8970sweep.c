/*
 * Copyright (c) 2024 Michael G. Katzmann
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include <glib-2.0/glib.h>
#include <gpib/ib.h>
#include <errno.h>
#include <HP8970.h>
#include <locale.h>

#include "GPIBcomms.h"
#include "HP8970comms.h"
#include "messageEvent.h"

gdouble
LOfrequency( tGlobal *pGlobal, gdouble freqRF ) {

    tMode mode = pGlobal->HP8970settings.mode;
    gdouble freqLO = pGlobal->HP8970settings.extLOfreqLO;
    gdouble freqIF = pGlobal->HP8970settings.extLOfreqIF;
    tSideband sideband = pGlobal->HP8970settings.extLOsideband;

    gdouble LOfrequency = 0.0;

    switch( mode ) {
        case eMode1_0:  // no external LO and no frequency conversion
        default:
            LOfrequency = 0.0;
            break;
        case eMode1_1:  // variable frequency LO, fixed IF (amplifier)
        case eMode1_3:  // variable frequency LO, fixed IF (receiver)
            switch( sideband ) {
                case eDSB:
                default:
                    LOfrequency =  freqRF;
                    break;
                case eLSB:
                    LOfrequency =  freqRF + freqIF;
                    break;
                case eUSB:
                    LOfrequency = freqRF - freqIF;
                    break;
            }
            break;
        case eMode1_2:  // fixed LO, variable IF (amplifier)
        case eMode1_4:  // fixed LO, variable IF (receiver)
            LOfrequency = freqLO;
            break;
    }

    return LOfrequency;
}


/*!     \brief  initialize a circular buffer
 *
 * Allocate buffer and set counters
 *
 * \param  pCircBuffer      pointer to the circular buffer structure
 * \param  size             size of the new buffer
 */
void
initCircularBuffer( tCircularBuffer *pCircBuffer, guint size, tAbscissa abscissa ) {

	size = size+1;	// add one so that tail != head when size items in buffer
    pCircBuffer->measurementData = g_realloc( pCircBuffer->measurementData, size * sizeof( tNoiseAndGain ) );
    pCircBuffer->head = 0;
    pCircBuffer->tail = 0;
    pCircBuffer->size = size;

    pCircBuffer->minNoise = UNINITIALIZED_DOUBLE;
    pCircBuffer->maxNoise = UNINITIALIZED_DOUBLE;
    pCircBuffer->minGain  = UNINITIALIZED_DOUBLE;
    pCircBuffer->maxGain  = UNINITIALIZED_DOUBLE;

    if( abscissa == eFreqAbscissa ) {
        pCircBuffer->minAbscissa.freq  = UNINITIALIZED_DOUBLE;
        pCircBuffer->maxAbscissa.freq  = UNINITIALIZED_DOUBLE;
    } else {
        pCircBuffer->minAbscissa.time  = 0;
        pCircBuffer->maxAbscissa.time  = 0;
    }
}

/*!     \brief  add item to a circular buffer
 *
 * add item to a circular buffer, possibly overwriting older data
 *
 * \param  pCircBuffer      pointer to the circular buffer structure
 * \param  pData            pointer to the data
 * \return TRUE if no overflow (or FALSE if full)
 */
gboolean
addItemToCircularBuffer( tCircularBuffer *pCircBuffer, tNoiseAndGain *pItem, gboolean bOverwrite ) {
    // if we full, either return (if we are told not to overwrite)
    // or move the head, discarding the first item
    if( (pCircBuffer->tail + 1) % pCircBuffer->size  == pCircBuffer->head ) {
        if( bOverwrite )
            return FALSE;
        else
            pCircBuffer->head = (pCircBuffer->head + 1)  % pCircBuffer->size;
    }

    pCircBuffer->measurementData[ pCircBuffer->tail ] = *pItem;
    pCircBuffer->tail = (pCircBuffer->tail + 1) % pCircBuffer->size;

    // Update the minimum and maximum values
    updateBoundaries( pItem->noise, &pCircBuffer->minNoise, &pCircBuffer->maxNoise );
    updateBoundaries( pItem->gain,  &pCircBuffer->minGain,  &pCircBuffer->maxGain );

    return TRUE;
}

/*!     \brief  get the number of items stored in a circular buffer
 *
 * get the number of items stored in a circular buffer
 *
 * \param  pCircBuffer      pointer to the circular buffer structure
 * \return number if items
 */
gint
nItemsInCircularBuffer( tCircularBuffer *pCircBuffer ) {
    gint nItems = pCircBuffer->tail - pCircBuffer->head;

    if( nItems < 0 )
        nItems += pCircBuffer->size;

    return( nItems );
}

/*!     \brief  get a particular item from a circular buffer
 *
 * get a particular item from a circular buffer
 *
 * \param  pCircBuffer      pointer to the circular buffer structure
 * \param  item             possition in the buffer of the data we want
 */

tNoiseAndGain *
getItemFromCircularBuffer( tCircularBuffer *pCircBuffer, guint item ) {
    if( item == LAST_ITEM ) {
        if( pCircBuffer->tail == pCircBuffer->head ) {
            return NULL;
        } else {
            if( pCircBuffer->tail >= 0 )
                return( &pCircBuffer->measurementData[ pCircBuffer->tail-1 ] );
            else
                return( &pCircBuffer->measurementData[ pCircBuffer->size-1 ] );
        }
    }
    if( item > pCircBuffer->size )
        return NULL;

    item = ( item + pCircBuffer->head ) % pCircBuffer->size;

    return( &pCircBuffer->measurementData[ item ] );
}

/*!     \brief  determine minimum and maximum values for time/frequency based on the data
 *
 * determine minimum and maximum values for time/frequency based on the data
 *
 * \param  pCircBuffer      pointer to the circular buffer structure
 * \return 0 if OK or -1 if error
 */

gboolean
determineTimeExtremesInCircularBuffer( tCircularBuffer *pCircBuffer ) {
    tNoiseAndGain *pFirst = getItemFromCircularBuffer( pCircBuffer, 0 );
    tNoiseAndGain *pLast = getItemFromCircularBuffer( pCircBuffer, LAST_ITEM );

    // empty
    if( pCircBuffer->tail == pCircBuffer->head )
                return ERROR;

    pCircBuffer->minAbscissa.time = pFirst->abscissa.time;
    pCircBuffer->maxAbscissa.time = pLast->abscissa.time;

    return 0;
}

#define TIME_FROM_POSN( x, y ) pCircBuffer->measurementData[ ((x)->head + (y)) % (x)->size].abscissa.time

// Function to find the closest element in the circular buffer
gint
findTimeDeltaInCircularBuffer(tCircularBuffer *pCircBuffer, gdouble delta) {
    gint nItems = ((pCircBuffer->tail - pCircBuffer->head) + pCircBuffer->size) % pCircBuffer->size;
    gint low = 0;              // index to first item
    gint high = nItems - 1;    // index to last item / since they are ascending this is also the highest

    // nonsense if no items
    if( nItems == 0 )
        return INVALID;

    gint64 lowTime, highTime;

    lowTime  = TIME_FROM_POSN( pCircBuffer, low );
    highTime = TIME_FROM_POSN( pCircBuffer, high );

    // if delta is 10.0 seconds, then look for the last time-10 seconds
    gint64 target = highTime - (gint64)(delta * 1.0e3);

    // Corner cases
    if (target <= lowTime)  return low;
    if (target >= highTime) return high;

    // Binary search
    while (low <= high) {
        gint mid = low + (high - low) / 2;
        gdouble midTime = TIME_FROM_POSN( pCircBuffer, mid );

        lowTime  = TIME_FROM_POSN( pCircBuffer, low );
        highTime = TIME_FROM_POSN( pCircBuffer, high );

        if ( midTime == target) return mid; // Exact match

        // Update the closest element if necessary
        if (abs(midTime - target) < abs(lowTime - target)) {
            low = mid;
        }

        if (midTime < target) {
            low  = mid + 1; // Search in upper half
        } else {
            high = mid - 1; // Search in lower half
        }
    }

    // At this point, low is either equal to high or one position ahead
    // We need to compare the elements at low and low-1 to find the closest
    gint64 lowMinusOneTime = TIME_FROM_POSN( pCircBuffer, low -1 );
    if (low > 0 && abs( lowMinusOneTime - target) < abs(lowTime - target)) {
        return low - 1;
    } else {
        return low;
    }
}



/*!     \brief  Sweep the HP8970 to obtain noise figure (and gain)
 *
 * Sweep the HP8970 to obtain noise figure (and gain)
 *
 * \param  pGlobal          pointer to global data
 * \param  descGPIB_HP8970  descriptor of the HP8970 GPIB connection
 * \param  descGPIB_extLO   descriptor of the external LO GPIB connection
 * \param  pGPIBstatus      pointer to the GPIB status
 * \return TRUE if successful
 */
gboolean
sweepHP8970( tGlobal *pGlobal, gint descGPIB_HP8970, gint descGPIB_extLO, gint *pGPIBstatus ) {
    gint HP8970error;
    GString *pstCommands;
    gchar HP8970status, LOstatus;;
    gboolean completionStatus = FALSE;
    gdouble LOfreq = 0.0;
    gboolean bLOerror = FALSE;
    tMode mode =  pGlobal->HP8970settings.mode;

    while TRUE {
        pstCommands = g_string_new ( NULL );
        gboolean bExtLO;
        gdouble freqMHz, freqStartMHz, freqStopMHz, freqStepMHz;
        gboolean bContinue;
        gchar *sMessage;

        postInfo( "HP8970 data sweep 🧹");
        gtk_widget_set_sensitive( pGlobal->widgets[ eW_btn_CSV ], FALSE );

        // snapshot of the settings we need to send
        bExtLO = !(mode == eMode1_0 || mode == eMode1_4);

        freqStartMHz = pGlobal->HP8970settings.range[ bExtLO ].freqStartMHz;
        freqStopMHz = pGlobal->HP8970settings.range[ bExtLO ].freqStopMHz;
        freqStepMHz = pGlobal->HP8970settings.range[ bExtLO ].freqStepSweepMHz;

        // Set the external signal generator (LO) for higher modes
        if( pGlobal->flags.bNoLOcontrol == FALSE && mode != eMode1_0 ) {
            if( pGlobal->HP8970settings.sExtLOsetup )
                if( GPIBasyncWrite (descGPIB_extLO, pGlobal->HP8970settings.sExtLOsetup, pGPIBstatus, 10 * TIMEOUT_RW_1SEC) != eRDWT_OK )
                    break;
            // We only have to set the LO frequency once for modes 1.2 and 1.4
            if( ( LOfreq = LOfrequency( pGlobal, freqStartMHz ) ) != 0.0 ) {
                g_string_printf( pstCommands, pGlobal->HP8970settings.sExtLOsetFreq, LOfreq );
                if( GPIBasyncWrite (descGPIB_extLO, pstCommands->str, pGPIBstatus, 10 * TIMEOUT_RW_1SEC) != eRDWT_OK ) {
                    bLOerror = TRUE;
                    break;
                }
                *pGPIBstatus = ibrsp (descGPIB_extLO, &LOstatus); // get the status byte from the LO
                sMessage = g_strdup_printf( "Signal Generator (LO): %.0lf MHz", LOfreq );
                postInfoLO( sMessage );
                g_free( sMessage );
            }
            usleep( pGlobal->HP8970settings.settlingTime_ms * 1000 );
            // We start here and add a step each time
        }

        // Ensure that the basics are set in the HP8970
        g_string_printf( pstCommands, "H1"        "T1"        "E%1d"      "IF%dMZ"    "LF%dMZ"
                                      "B%1d"      "FA%dMZ"    "FB%dMZ"    "SS%dMZ"    "F%1d"
                                      "N%1d"      "E%1d"      "D0"        "TC%.2lfEN" "L%1d"
                                      "LA%.3lfEN" "LB%.3lfEN" "LT%.2lfEN" "M%1d",
                      // H1 - provide Gain & Noise Figure data
                      // T1 - Hold
                      pGlobal->HP8970settings.mode,
                      (gint)pGlobal->HP8970settings.extLOfreqIF,
                      (gint) pGlobal->HP8970settings.extLOfreqLO,

                      pGlobal->HP8970settings.extLOsideband,
                      (gint)freqStartMHz,
                      (gint)freqStopMHz,
                      (gint)freqStepMHz,
                      (gint)round( log2( pGlobal->HP8970settings.smoothingFactor ) ),

                      pGlobal->HP8970settings.noiseUnits,
                      pGlobal->HP8970settings.mode,
                      // D0 - input temperature units K
                      pGlobal->HP8970settings.coldTemp,
                      pGlobal->HP8970settings.switches.bLossCompensation,

                      pGlobal->HP8970settings.lossBeforeDUT,
                      pGlobal->HP8970settings.lossAfterDUT,
                      pGlobal->HP8970settings.lossTemp,
                      pGlobal->HP8970settings.switches.bCorrectedNFAndGain ? 2 : 1

        );
        if( GPIBasyncWrite (descGPIB_HP8970, pstCommands->str, pGPIBstatus, 10 * TIMEOUT_RW_1SEC) != eRDWT_OK )
            break;

        *pGPIBstatus = ibrsp (descGPIB_HP8970, &HP8970status);    // Clear out status

        initCircularBuffer( &pGlobal->plot.measurementBuffer, (freqStopMHz - freqStartMHz) / freqStepMHz + 2, eFreqAbscissa );

        pGlobal->plot.measurementBuffer.minAbscissa.freq  = freqStartMHz * MHz(1.0);
        pGlobal->plot.measurementBuffer.maxAbscissa.freq  = freqStopMHz * MHz(1.0);
        pGlobal->plot.flags.bSpotFrequencyPlot = FALSE;

        // Initially do a frequency sweep which uses the step increment in the 8970
        // initiate a single sweep
        enableSRQonDataReady (descGPIB_HP8970, pGPIBstatus);
        GPIBasyncWrite (descGPIB_HP8970, "W2", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);

        pGlobal->plot.flags.bValidNoiseData = FALSE;
        pGlobal->plot.flags.bValidGainData = FALSE;

        getTimeStamp(&pGlobal->plot.sDateTime);

        // Sweep with the sweep step (may not be the same as the calibration step)
        for( freqMHz = freqStartMHz, bContinue = TRUE;
                GPIBsucceeded( *pGPIBstatus ) && bContinue && checkMessageQueue(NULL) != SEVER_DIPLOMATIC_RELATIONS; ) {

            tNoiseAndGain measurement;
            measurement.flags.all = 0;

            // This is the last measurement
            if( freqMHz == freqStopMHz )
                bContinue = FALSE;

            if( GPIBtriggerMeasurement (descGPIB_HP8970, &measurement,
                                        pGPIBstatus, &HP8970error, 30 * TIMEOUT_RW_1SEC) != eRDWT_OK )
                break;  // this will exit the for loop if error

            if( freqMHz + freqStepMHz > freqStopMHz ) {
                freqMHz = freqStopMHz;
            } else {
                freqMHz += freqStepMHz;
            }

            // Changing the LO only in mode 1.1 & 1.3 (1.2 & 1.4 have a fixed LO that we already set)
            if( pGlobal->flags.bNoLOcontrol == FALSE && ( mode == eMode1_1 || mode == eMode1_3 ) ) {

                if( ( LOfreq = LOfrequency( pGlobal, freqMHz ) ) != 0.0 ) {
                    g_string_printf( pstCommands, pGlobal->HP8970settings.sExtLOsetFreq, LOfreq );

                    if( GPIBasyncWrite (descGPIB_extLO, pstCommands->str, pGPIBstatus, 10 * TIMEOUT_RW_1SEC) != eRDWT_OK ) {
                        bLOerror = TRUE;
                        break;
                    }
                    *pGPIBstatus = ibrsp (descGPIB_extLO, &LOstatus); // get the status byte from the LO
                }
                sMessage = g_strdup_printf( "Signal Generator (LO): %.0lf MHz", LOfreq );
                postInfoLO( sMessage );
                g_free( sMessage );

                usleep( pGlobal->HP8970settings.settlingTime_ms * 1000 );
            }

            measurement.flags.each.bNoiseInvalid =
                    IS_HP8970_ERROR( measurement.noise );
            measurement.flags.each.bNoiseOverflow =
                    IS_HP8970_OVERFLOW( measurement.noise );

            measurement.flags.each.bGainInvalid =
                    IS_HP8970_ERROR( measurement.gain );
            measurement.flags.each.bGainOverflow =
                    IS_HP8970_OVERFLOW( measurement.gain );

            if( measurement.flags.each.bNoiseInvalid == FALSE )
                pGlobal->plot.flags.bValidNoiseData = TRUE;
            if( measurement.flags.each.bGainInvalid == FALSE )
                pGlobal->plot.flags.bValidGainData = TRUE;
;
            addItemToCircularBuffer( &pGlobal->plot.measurementBuffer, &measurement, TRUE );

            if( HP8970error ) {
                sMessage = g_strdup_printf( "Sweep: %.0lf MHz ☠️  %s",
                                            measurement.abscissa.freq / MHz( 1.0 ),
                                            HP8970errorString( HP8970error ) );
            } else {
                sMessage = g_strdup_printf( "Sweep: %.0lf MHz",
                                            measurement.abscissa.freq / MHz( 1.0 ) );
            }
            postInfo( sMessage );
            g_free( sMessage );
            postMessageToMainLoop(TM_REFRESH_PLOT, NULL);
        }

        // resume auto trigger & disable SRQ
        GPIBasyncWrite (descGPIB_HP8970, "T0Q0", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);


        completionStatus = TRUE;
        // if( enableSRQonDataReady( descGPIB_HP8970, &GPIBstatus ) != eRDWT_OK )
        //    break;
        break;
    }
    g_string_free ( pstCommands, TRUE );

    if( GPIBfailed( *pGPIBstatus ) ) {
        if( bLOerror ) {
            postErrorLO( "Communications failure with signal generator (LO)" );
        } else {
            postError( "Communications failure with HP8790" );
        }
        pGlobal->plot.flags.bValidNoiseData = FALSE;
        pGlobal->plot.flags.bValidGainData  = FALSE;
    } else if( HP8970error > 0 ) {
        gchar *sError = g_strdup_printf( "HP8970 error: %s", HP8970errorString( HP8970error ) );
        postError( sError );
        g_free( sError );
    } else {
        postInfo( "HP8970 data sweep OK");
        postInfoLO( "");
        postMessageToMainLoop(TM_REFRESH_PLOT, NULL);
    }

    if( pGlobal->flags.bNoLOcontrol == FALSE && mode != eMode1_0 )
        ibloc(descGPIB_HP8970);

    ibrsp (descGPIB_HP8970, &HP8970status);    // Clear out status
    gtk_widget_set_sensitive( pGlobal->widgets[ eW_btn_CSV ], pGlobal->plot.flags.bValidNoiseData );

    return completionStatus;
}



/*!     \brief  Repeatedly take read NF (& gain) at the spot frequency
 *
 * Repeatedly take read NF (& gain) at the spot frequency
 *
 * \param  pGlobal          pointer to global data
 * \param  descGPIB_HP8970  descriptor of the HP8970 GPIB connection
 * \param  descGPIB_extLO   descriptor of the external LO GPIB connection
 * \param  pGPIBstatus      pointer to the GPIB status
 * \return TRUE if successful
 */
gboolean
spotFrequencyHP8970( tGlobal *pGlobal, gint descGPIB_HP8970, gint descGPIB_extLO, gint *pGPIBstatus ) {
    gint HP8970error;
    GString *pstCommands;
    gchar HP8970status, LOstatus;
    gboolean completionStatus = FALSE;
    tGPIBReadWriteStatus rtn;
    gdouble freqSpotMHz, LOfreq;
    gboolean bExtLO, bLOerror = FALSE;
    tMode mode;

    mode = pGlobal->HP8970settings.mode;
    bExtLO = !(mode == eMode1_0 || mode == eMode1_4);

    freqSpotMHz = pGlobal->HP8970settings.range[ bExtLO ].freqSpotMHz;

    while TRUE {
        pstCommands = g_string_new ( NULL );
        gtk_widget_set_sensitive( pGlobal->widgets[ eW_btn_CSV ], FALSE );

        gboolean bContinue;
        gchar *sMessage;

        postInfo( "HP8970 spot frequency measurement");

        // snapshot of the settings we need to send
        bExtLO = !(pGlobal->HP8970settings.mode == eMode1_0 || pGlobal->HP8970settings.mode == eMode1_4);

        // Set the external signal generator (LO) for higher modes
        if( pGlobal->flags.bNoLOcontrol == FALSE && mode != eMode1_0 ) {
            if( pGlobal->HP8970settings.sExtLOsetup )
                if( GPIBasyncWrite (descGPIB_extLO, pGlobal->HP8970settings.sExtLOsetup, pGPIBstatus, 10 * TIMEOUT_RW_1SEC) != eRDWT_OK )
                    break;
            // We only have to set the LO frequency once for modes 1.2 and 1.4
            if( ( LOfreq = LOfrequency( pGlobal, freqSpotMHz ) ) != 0.0 ) {
                g_string_printf( pstCommands, pGlobal->HP8970settings.sExtLOsetFreq, LOfreq );
                if( GPIBasyncWrite (descGPIB_extLO, pstCommands->str, pGPIBstatus, 10 * TIMEOUT_RW_1SEC) != eRDWT_OK ) {
                    bLOerror = TRUE;
                    break;
                }
                *pGPIBstatus = ibrsp (descGPIB_extLO, &LOstatus); // get the status byte from the LO
                sMessage = g_strdup_printf( "Signal Generator: %.0lf MHz", LOfreq );
                postInfoLO( sMessage );
                g_free( sMessage );
            }
            usleep( pGlobal->HP8970settings.settlingTime_ms * 1000 );
            // We start here and add a step each time
        }

        // Ensure that the basics are set in the HP8970
        g_string_printf( pstCommands, "H1"        "T1"        "E%1d"      "IF%dMZ"    "LF%dMZ"
                                      "B%1d"      "FR%dMZ"    "F%1d"      "N%1d"      "D0"
                                      "TC%.2lfEN" "L%1d"      "LA%.3lfEN" "LB%.3lfEN" "LT%.2lfEN"
                                      "M%1d",
                         // H1 - provide Gain & Noise Figure data
                         // T1 - Hold
                         pGlobal->HP8970settings.mode,
                         (gint)pGlobal->HP8970settings.extLOfreqIF,
                         (gint) pGlobal->HP8970settings.extLOfreqLO,

                         pGlobal->HP8970settings.extLOsideband,
                         (gint)pGlobal->HP8970settings.range[ bExtLO ].freqSpotMHz,
                         (gint)round( log2( pGlobal->HP8970settings.smoothingFactor ) ),
                         pGlobal->HP8970settings.noiseUnits,
                         // D0 - input temperature units in K

                         pGlobal->HP8970settings.coldTemp,
                         pGlobal->HP8970settings.switches.bLossCompensation,
                         pGlobal->HP8970settings.lossBeforeDUT,
                         pGlobal->HP8970settings.lossAfterDUT,
                         pGlobal->HP8970settings.lossTemp,

                         pGlobal->HP8970settings.switches.bCorrectedNFAndGain ? 2 : 1 );
        if( GPIBasyncWrite (descGPIB_HP8970, pstCommands->str, pGPIBstatus, 10 * TIMEOUT_RW_1SEC) != eRDWT_OK )
            break;

        *pGPIBstatus = ibrsp (descGPIB_HP8970, &HP8970status);    // Clear out status

        initCircularBuffer( &pGlobal->plot.measurementBuffer, MAX_SPOT_POINTS, eTimeAbscissa );

        pGlobal->plot.noiseUnits = pGlobal->HP8970settings.noiseUnits;

        pGlobal->plot.flags.bDataCorrectedNFAndGain = pGlobal->HP8970settings.switches.bCorrectedNFAndGain;
        pGlobal->plot.smoothingFactor = pGlobal->HP8970settings.smoothingFactor;

        pGlobal->plot.flags.bSpotFrequencyPlot = TRUE;

        // Get data via SQR after trigger
        enableSRQonDataReady (descGPIB_HP8970, pGPIBstatus);

        pGlobal->plot.flags.bValidNoiseData = FALSE;
        pGlobal->plot.flags.bValidGainData = FALSE;

        getTimeStamp(&pGlobal->plot.sDateTime);

        // Standard resolution just sweep. This is faster than setting the frequency each time but less noticeable once we do smoothing.
        for( bContinue = TRUE;
                GPIBsucceeded( *pGPIBstatus )
                    && bContinue
                    && checkMessageQueue(NULL) != SEVER_DIPLOMATIC_RELATIONS
                    && bLOerror == FALSE
                    && pGlobal->HP8970settings.switches.bSpotFrequency; ) {
            tNoiseAndGain measurement;

            measurement.flags.all = 0;

            rtn = GPIBtriggerMeasurement (descGPIB_HP8970, &measurement,
                                          pGPIBstatus, &HP8970error, 30 * TIMEOUT_RW_1SEC);
            if( rtn == eRDWT_ABORT )
                break;
            else if( rtn != eRDWT_OK ) {
                break;  // this will exit the for loop when interrupted or error
            }

            measurement.abscissa.time = g_get_real_time() / 1000;    // convert microseconds to milliseconds
            measurement.flags.each.bNoiseInvalid =
                    IS_HP8970_ERROR( measurement.noise );
            measurement.flags.each.bNoiseOverflow =
                    IS_HP8970_OVERFLOW( measurement.noise );

            measurement.flags.each.bGainInvalid =
                    IS_HP8970_ERROR( measurement.gain );
            measurement.flags.each.bGainOverflow =
                    IS_HP8970_OVERFLOW( measurement.gain );

            if( measurement.flags.each.bNoiseInvalid == FALSE )
                pGlobal->plot.flags.bValidNoiseData = TRUE;
            if( measurement.flags.each.bGainInvalid == FALSE )
                pGlobal->plot.flags.bValidGainData = TRUE;

            addItemToCircularBuffer( &pGlobal->plot.measurementBuffer, &measurement, TRUE );

            // we will display 60 seconds * the smoothing factor
            pGlobal->plot.measurementBuffer.idxTimeBeforeTail = findTimeDeltaInCircularBuffer(&pGlobal->plot.measurementBuffer,
                                                                                              TIME_PLOT_LENGTH * pGlobal->HP8970settings.smoothingFactor  );

            if( HP8970error ) {
                sMessage = g_strdup_printf( "Spot measurement: %.1lf s  ☠️  %s",
                                            measurement.abscissa.freq,
                                            HP8970errorString( HP8970error ) );
            } else {
                sMessage = g_strdup_printf( "Spot measurement: %.1lf s",
                        measurement.abscissa.freq );
            }
            postInfo( sMessage );
            g_free( sMessage );
            postMessageToMainLoop(TM_REFRESH_PLOT, NULL);
        }


        // resume auto trigger & disable SRQ
        GPIBasyncWrite (descGPIB_HP8970, "T0Q0", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);

        if( rtn == eRDWT_ABORT ) {
            postInfo( "Ending spot measurement");
            postMessageToMainLoop(TM_REFRESH_PLOT, NULL);
        } else if( GPIBfailed( *pGPIBstatus ) ) {
            if( bLOerror ) {
                postErrorLO( "Communications failure with signal generator (LO)" );
            } else {
                postError( "Communications failure with HP8790" );
            }
            pGlobal->plot.flags.bValidNoiseData = FALSE;
            pGlobal->plot.flags.bValidGainData  = FALSE;
        } else if( HP8970error > 0 ) {
            gchar sError[ MEDIUM_STRING ];
            g_snprintf( sError, MEDIUM_STRING, "HP8970 error: %s", HP8970errorString( HP8970error ) );
            postError( sError );
        } else {
            postInfo( "HP8970 spot frequency measurement ended");
            postInfoLO( "");
            postMessageToMainLoop(TM_REFRESH_PLOT, NULL);
        }

        completionStatus = TRUE;
        // if( enableSRQonDataReady( descGPIB_HP8970, &GPIBstatus ) != eRDWT_OK )
        //    break;
        break;
    }

    g_string_free ( pstCommands, TRUE );
    ibrsp (descGPIB_HP8970, &HP8970status);    // Clear out status

    gtk_widget_set_sensitive( pGlobal->widgets[ eW_btn_CSV ], pGlobal->plot.flags.bValidNoiseData );
    return completionStatus;
}


/*!     \brief  Calibrate the HP8970 to account for the 2nd stage noise and gain
 *
 * Calibrate the HP8970 to account for the 2nd stage noise and gain
 *
 * \param  pGlobal          pointer to global data
 * \param  descGPIB_HP8970  descriptor of the HP8970 GPIB connection
 * \param  descGPIB_extLO   descriptor of the external LO GPIB connection
 * \param  pGPIBstatus      pointer to the GPIB status
 * \return TRUE if successful
 */
gboolean
calibrateHP8970( tGlobal *pGlobal, gint descGPIB_HP8970, gint descGPIB_extLO, gint *pGPIBstatus ) {
    gint HP8970error;
    GString *pstCommands;
    gchar HP8970status, LOstatus;
    tMode mode;

    tCircularBuffer *pCircularBuffer = &pGlobal->plot.measurementBuffer;

    gdouble LOfreq;
    gdouble freqStartMHz, freqStopMHz, freqStepMHz, freqRF_MHz;
    tNoiseAndGain measurement;

    pstCommands = g_string_new ( NULL );
    gboolean bExtLO, bContinue, bRestartSweep, completionStatus = FALSE, bLOerror = FALSE;
    gchar *sMessage;
    gint nCalPoint;

    mode = pGlobal->HP8970settings.mode;
    bExtLO = !(mode == eMode1_0 || mode == eMode1_4);

    freqStartMHz = pGlobal->HP8970settings.range[ bExtLO ].freqStartMHz;
    freqStopMHz  = pGlobal->HP8970settings.range[ bExtLO ].freqStopMHz;
    freqStepMHz  = pGlobal->HP8970settings.range[ bExtLO ].freqStepCalMHz;

    while TRUE {


        postInfo( "HP8970 calibration 📏");

        // snapshot of the settings we need to send

        // Calibration using LO only if down converter is 'part of the measurement system'
        // In mode 1.3 and 1.4, calibration is done without the frequency translator.+
        if( pGlobal->flags.bNoLOcontrol == FALSE && (mode == eMode1_1 || mode == eMode1_2) ) {
            if( pGlobal->HP8970settings.sExtLOsetup )
                if( GPIBasyncWrite (descGPIB_extLO, pGlobal->HP8970settings.sExtLOsetup, pGPIBstatus, 10 * TIMEOUT_RW_1SEC) != eRDWT_OK )
                    break;
            // We only have to set the LO frequency once for modes 1.2 and 1.4
            if( ( LOfreq = LOfrequency( pGlobal, freqStartMHz ) ) != 0.0 ) {
                g_string_printf( pstCommands, pGlobal->HP8970settings.sExtLOsetFreq, LOfreq );
                if( GPIBasyncWrite (descGPIB_extLO, pstCommands->str, pGPIBstatus, 10 * TIMEOUT_RW_1SEC) != eRDWT_OK ) {
                    bLOerror = TRUE;
                    break;
                }
                *pGPIBstatus = ibrsp (descGPIB_extLO, &LOstatus); // get the status byte from the LO
                sMessage = g_strdup_printf( "Signal Generator (LO): %.0lf MHz", LOfreq );
                postInfoLO( sMessage );
                g_free( sMessage );
            }
            usleep( pGlobal->HP8970settings.settlingTime_ms * 1000 );
            // We start here and add a step each time
        }


        // H1         - provide Gain & Noise Figure data
        // T1         - Hold
        // E?         - Mode
        // IF????MZ   - IF frequency
        // LF????MZ   - LO frequency
        // B?         - sideband
        // FA????MZ   - Start Frequency
        // FB????MZ   - Stop Frequency
        // SS?MZ      - Step Size
        // F?         - Smoothing Factor
        // N?         - Noise units Fdb/F/YdB/Y/TeK
        // D0         - Input temp in K
        // TC???.??EN - Cold Temperature
        // L?         - Loss compensation on/off
        // LA??.???EN - Loss before DUT
        // LB??.???EN - Loss after DUT
        // LT??.??EN  - Loss temperature
        g_string_printf( pstCommands, "H1"         "T1"         "E%1d"       "IF%dMZ"    "LF%dMZ"    "B%1d"
        		                      "FA%dMZ"     "FB%dMZ"     "SS%dMZ"     "F%1d"      "N%1d"      "D0"
        		                      "TC%.2lfEN"  "L%1d"       "LA%.3lfEN"  "LB%.3lfEN" "LT%.2lfEN",
                      pGlobal->HP8970settings.mode,
                      (gint)pGlobal->HP8970settings.extLOfreqIF,
                      (gint) pGlobal->HP8970settings.extLOfreqLO,
                      pGlobal->HP8970settings.extLOsideband,

                      (gint)freqStartMHz,
                      (gint)freqStopMHz,
                      (gint)freqStepMHz,
                      (gint)round( log2( pGlobal->HP8970settings.smoothingFactor ) ),
                      pGlobal->HP8970settings.noiseUnits,

                      pGlobal->HP8970settings.coldTemp,
                      pGlobal->HP8970settings.switches.bLossCompensation,
                      pGlobal->HP8970settings.lossBeforeDUT,
                      pGlobal->HP8970settings.lossAfterDUT,
                      pGlobal->HP8970settings.lossTemp );

        g_string_append_printf( pstCommands, "IF%dMZ" "LF%dMZ" "B%1d",
        		pGlobal->HP8970settings.extLOfreqIF,  pGlobal->HP8970settings.extLOfreqLO, pGlobal->HP8970settings.extLOsideband );
        if( GPIBasyncWrite (descGPIB_HP8970, pstCommands->str, pGPIBstatus, 10 * TIMEOUT_RW_1SEC) != eRDWT_OK )
            break;

        *pGPIBstatus = ibrsp (descGPIB_HP8970, &HP8970status);    // Clear out status

        pGlobal->plot.flags.bValidNoiseData  = FALSE;
        pGlobal->plot.flags.bValidGainData   = FALSE;
        pGlobal->plot.flags.bCalibrationPlot = TRUE;
        postMessageToMainLoop(TM_REFRESH_PLOT, NULL);

        pGlobal->plot.flags.bSpotFrequencyPlot = FALSE;
        // Initially do a frequency sweep which uses the step increment in the 8970
        // initiate a single sweep
        enableSRQonDataReady (descGPIB_HP8970, pGPIBstatus);
        GPIBasyncWrite (descGPIB_HP8970, "CA", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);

        // See if there is an error when we try to calibrate
        HP8970getFreqNoiseGain ( descGPIB_HP8970, 2 * TIMEOUT_RW_1SEC, pGPIBstatus,
        		&measurement, &HP8970error);

        switch( HP8970error ) {
        case 20:	// we expect it to be not calibrated
        case 21:	// Current frequency is out of calibrated range
        case 22:	// Current RF attenuation not calibrated
        case 23:	// Not calibrated in the current measurement and sideband modes
        case 24:	// Not calibrated for the current IF
        case 25:	// Not calibrated for the current LO frequency
        case 99:	// Measurement overflow
        	HP8970error = 0;
        	break;
        default:
        	break;
        }

        getTimeStamp(&pGlobal->plot.sDateTime);

        for( nCalPoint = 1, bContinue = TRUE, bRestartSweep = TRUE, freqRF_MHz = pGlobal->HP8970settings.range[ bExtLO ].freqStartMHz;
                GPIBsucceeded( *pGPIBstatus ) && bContinue
                		&& checkMessageQueue(NULL) != SEVER_DIPLOMATIC_RELATIONS
						&& HP8970error == 0; nCalPoint++ ) {

            tGPIBReadWriteStatus rtn;

            tNoiseAndGain calDataPoint;

            rtn = GPIBtriggerMeasurement (descGPIB_HP8970, &calDataPoint,
                                                    pGPIBstatus, &HP8970error, 30 * TIMEOUT_RW_1SEC);

            if( bRestartSweep ) {
                initCircularBuffer( pCircularBuffer, (freqStopMHz - freqStartMHz) / freqStepMHz + 2, eFreqAbscissa );
                pCircularBuffer->minAbscissa.freq  = freqStartMHz * MHz(1.0);
                pCircularBuffer->maxAbscissa.freq  = freqStopMHz * MHz(1.0);
                pGlobal->plot.flags.bValidNoiseData = FALSE;
                pGlobal->plot.flags.bValidGainData  = FALSE;
                bRestartSweep = FALSE;
            }
            calDataPoint.flags.all = 0;

            if( rtn & CAL_COMPLETE )
                bContinue = FALSE;

            if( (rtn & ~CAL_COMPLETE ) != eRDWT_OK )
                break;  // this will exit the for loop if error

            // Changing the LO only in mode 1.1 (1.2 has a fixed LO)
            if( pGlobal->flags.bNoLOcontrol == FALSE && mode == eMode1_1 ) {
                if( freqRF_MHz >= freqStopMHz ) {   // If we have sent the stop freq, then go back to the beginning
                    freqRF_MHz = freqStartMHz;
                    nCalPoint = 0;
                } else if( freqRF_MHz + freqStepMHz > freqStopMHz
                		|| (rtn & CAL_COMPLETE) ) {
                    freqRF_MHz = freqStopMHz;
                } else {
                    freqRF_MHz += freqStepMHz;
                }

                if( nCalPoint >= (pGlobal->flags.bbHP8970Bmodel == e8970A ? CAL_POINTS_8970A:CAL_POINTS_8970B)) {
                    freqRF_MHz = freqStopMHz;
                    nCalPoint = 0;
                }

                if( ( LOfreq = LOfrequency( pGlobal, freqRF_MHz ) ) != 0.0 ) {
                    g_string_printf( pstCommands, pGlobal->HP8970settings.sExtLOsetFreq, LOfreq );

                    if( GPIBasyncWrite (descGPIB_extLO, pstCommands->str, pGPIBstatus, 10 * TIMEOUT_RW_1SEC) != eRDWT_OK ) {
                        bLOerror = TRUE;
                        break;
                    }
                    *pGPIBstatus = ibrsp (descGPIB_extLO, &LOstatus); // get the status byte from the LO
                }
                sMessage = g_strdup_printf( "Signal Generator (LO): %.0lf MHz", LOfreq );
                postInfoLO( sMessage );
                g_free( sMessage );

                usleep( pGlobal->HP8970settings.settlingTime_ms * 1000 );
            }

            calDataPoint.flags.each.bNoiseInvalid =
                    IS_HP8970_ERROR( calDataPoint.noise );
            calDataPoint.flags.each.bNoiseOverflow =
                    IS_HP8970_OVERFLOW( calDataPoint.noise );

            calDataPoint.flags.each.bGainInvalid =
                    IS_HP8970_ERROR( calDataPoint.gain );
            calDataPoint.flags.each.bGainOverflow =
                    IS_HP8970_OVERFLOW( calDataPoint.gain );

            if( calDataPoint.flags.each.bNoiseInvalid == FALSE )
                pGlobal->plot.flags.bValidNoiseData = TRUE;
            if( calDataPoint.flags.each.bGainInvalid == FALSE )
                pGlobal->plot.flags.bValidGainData = TRUE;

            gboolean bOverflow = (addItemToCircularBuffer( pCircularBuffer, &calDataPoint, FALSE ) == FALSE);

            if( HP8970error ) {
                sMessage = g_strdup_printf( "Calibration point %d: %.0lf MHz ☠️  %s", nCalPoint,
                                            calDataPoint.abscissa.freq / MHz( 1.0 ),
                                            HP8970errorString( HP8970error ) );
            } else {
                sMessage = g_strdup_printf( "Calibration point %d: %.0lf MHz", nCalPoint,
                                            calDataPoint.abscissa.freq / MHz( 1.0 ) );
            }
            postInfo( sMessage );
            g_free( sMessage );

            if( bContinue ) {
                // Calibration runs three times
                if( bOverflow || calDataPoint.abscissa.freq >= pCircularBuffer->maxAbscissa.freq ) {
                    bRestartSweep = TRUE;
                }
                postMessageToMainLoop(TM_REFRESH_PLOT, NULL);
            }
        }

        pGlobal->plot.flags.bValidNoiseData = FALSE;
        pGlobal->plot.flags.bValidGainData  = FALSE;
        postMessageToMainLoop(TM_REFRESH_PLOT, NULL);

        // sweep off (in case we've interrupted the calibration .. according to the manual, this is the only way to stop the calibration)
        // resume auto trigger & disable SRQ
        gint altGPIBstatus = 0; // we try to write even if there was a GPIB error. If we don't provide an alternate status gint
                                // the write will not be attempted
        GPIBasyncWrite (descGPIB_HP8970, "W0T0Q0", &altGPIBstatus, 10 * TIMEOUT_RW_1SEC);

        completionStatus = TRUE;
        break;
    }
    g_string_free ( pstCommands, TRUE );

    if( GPIBfailed( *pGPIBstatus ) ) {
        if( bLOerror ) {
            postErrorLO( "Communications failure with signal generator (LO)" );
        } else {
            postError( "Communications failure with HP8790" );
        }
        pGlobal->plot.flags.bValidNoiseData = FALSE;
        pGlobal->plot.flags.bValidGainData  = FALSE;
    } else {
        if( HP8970error ) {
            sMessage = g_strdup_printf( "Calibration ☠️  %s",
                                        HP8970errorString( HP8970error ) );
            postError( sMessage );
            g_free( sMessage );
        }  else {
        	postInfo( "HP8970 calibration OK");
        }
        postInfoLO( "");
        postMessageToMainLoop(TM_REFRESH_PLOT, NULL);
    }

    if( pGlobal->flags.bNoLOcontrol == FALSE && (mode == eMode1_1 || mode == eMode1_2) )
        ibloc(descGPIB_HP8970);

    pGlobal->plot.flags.bCalibrationPlot = FALSE;

    return completionStatus;
}

