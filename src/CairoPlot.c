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

/*! \file GTKplot.c
 *  \brief Plot trace onto GtkDrawingArea widget
 *
 * Plot data obtained from the HP8970A/B instrument and
 * transformed to cairo objects for display on the GtkDrawingArea.
 * The drawing contexts of the PNG, PDF Cairo objects are substituted for the widget's in
 * order to print or save to image.
 *
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <cairo/cairo.h>
#include <glib-2.0/glib.h>
#include "HP8970.h"
#include "widgetID.h"
#include <math.h>

#define SIGN(x) ((x > 0) ? 1 : ((x < 0) ? -1 : 0))

GdkRGBA plotElementColorsFactory[ eMAX_COLORS ] = {
        [eColorNoise]      = {0.00, 0.00, 0.40, 1.0},  // dark blue
        [eColorGain]       = {0.00, 0.40, 0.00, 1.0},  // dark green
        [eColorFrequency]  = {0.00, 0.00, 0.00, 1.0},  // black
        [eColorGrid]       = {0.30, 0.30, 0.80, 0.6},  // light blue
        [eColorGridGain]   = {0.30, 0.80, 0.30, 0.6},  // light green
        [eColorTitle]      = {0.00, 0.00, 0.00, 1.0},  // black
        [eColorTBD1]       = {0.51, 0.51, 0.84, 1.0},  // light blue
        [eColorTBD2]       = {0.00, 0.00, 1.00, 1.0},
        [eColorTBD3]       = {1.00, 0.00, 0.00, 1.0},
        [eColorTBD4]       = {1.00, 0.00, 0.00, 1.0},
        [eColorTBD5]       = {1.00, 0.00, 0.00, 1.0},
        [eColorTBD6]       = {0.00, 0.00, 1.00, 1.0}
};
GdkRGBA plotElementColors[ eMAX_COLORS ];

/*!     \brief  Determine the noise and gain grid based max/min values set by user
 *
 * Determine the noise and gain grid based user limits
 *
 * \param pGlobal       pointer to the global data structure
 * \param gridType      type of grid
 * \return              0
 */
gint
determineFixedGridDivisions( tGlobal *pGlobal, tGridAxes gridType ) {
    gdouble gridRange, division;
    gdouble logDecade, logFraction, log10diff;
    gint quantum;
    gdouble headroom, min, max;
    gint returnStatus = 0;
    int i;

    if( gridType == eNoise ) {
        min = pGlobal->fixedGridNoise[ pGlobal->plot.noiseUnits ][ 0 ];
        max = pGlobal->fixedGridNoise[ pGlobal->plot.noiseUnits ][ 1 ];
    } else {
        min = pGlobal->fixedGridGain[ 0 ];
        max = pGlobal->fixedGridGain[ 1 ];
    }
    const static gdouble logRanges[ N_RANGES ] = { LOG10, LOG5, LOG2, LOG1 };
    const static gdouble multipliers[ N_RANGES ] = { 10, 5, 2, 1 };

    log10diff = log10(max-min);
    logFraction = modf( log10diff, &logDecade );

    // If the log difference is less than one (difference is fractional),
    // divide the decade by 10 (subtract 1 from log) and add one to fraction to make positive
    if( log10diff < 0.0 ) {
        logDecade -= 1.0;
        logFraction += 1.0;
    }

    for( i = 1, quantum  = 0, headroom = logRanges[0] - logFraction; i < N_RANGES; i++ ) {
        if( headroom > fabs(logRanges[ i ] - logFraction)) {
            headroom = fabs(logRanges[ i ] - logFraction);
            quantum = i;
        }
    }

    // create the range
    gridRange = pow( 10.0, logDecade ) * multipliers[ quantum ];

    // ten divisions per grid
    division = gridRange/10.0;

#define VERY_SMALL  0.0000001
    pGlobal->plot.axis[ gridType ].min = min;
    pGlobal->plot.axis[ gridType ].max = max;
    pGlobal->plot.axis[ gridType ].perDiv = division;

    gdouble residual = fmod( min, division );
    if( fabs( residual / division ) > VERY_SMALL )
        pGlobal->plot.axis[ gridType ].offset = division - fmod( min, division );
    else
        pGlobal->plot.axis[ gridType ].offset = 0;

    if( pGlobal->plot.axis[ gridType ].offset > division )
        pGlobal->plot.axis[ gridType ].offset -= division;

    if( fabs( pGlobal->plot.axis[ gridType ].offset ) < VERY_SMALL )
        pGlobal->plot.axis[ gridType ].offset = 0.0;

    return returnStatus;
}


/*!     \brief  Determine the frequency grid based on the data (whole grid divisions)
 *
 * Determine the frequency grid based on the data
 *
 * \param pGlobal       pointer to the global data structure
 * \param min           smallest number in plot
 * \param max           largest number in plot
 * \return              0
 */
gint
quantizePlotFrequencyRange( tGlobal *pGlobal, gdouble min, gdouble max ) {
    gint rtnStatus = 0;

    gint i, closest;
#define N_DIVISIONS 5
    gdouble divisions[N_DIVISIONS] = { -0.6989700043, -0.3010299957, 0.0, 0.3010299957, 0.6989700043 };
    gdouble f, fr;
    gboolean bExpandRange = FALSE;
    gdouble logDecade, logFraction, perDiv, closestLogSpanDiff;

    gdouble maxPlot = max;
    gdouble minPlot = min;

    gdouble range = maxPlot - minPlot;


    if( range < MIN_RANGE_FREQ ) {
        range = MIN_RANGE_FREQ;
        bExpandRange = TRUE;
    }

    // Determine X axis grid (Frequency)
#define NOMINAL_NO_DIVISIONS    10

    logFraction = modf( log10( range / NOMINAL_NO_DIVISIONS ), &logDecade );
    for( i=0, closestLogSpanDiff = 1.0, closest=0; i < N_DIVISIONS; i++ ) {
        if( fabs( closestLogSpanDiff )> fabs( divisions[i] - logFraction )) {
            closestLogSpanDiff = divisions[i] - logFraction;
            closest = i;
        }
    }

    perDiv = round( pow( 10.0, logDecade + divisions[ closest ] ) * 1000 ) / 1000;

    if( ( fr = modf( minPlot / perDiv , &f )) != 0.0 ) {
        if( fr < 0.0 )
            minPlot = perDiv * (f - 1);
        else
            minPlot = perDiv * f;
    }

    if( ( fr = modf( maxPlot / perDiv , &f )) != 0 )
        maxPlot = perDiv * (f + 1);

    while( bExpandRange && maxPlot - minPlot < range ) {
        minPlot -= perDiv;
        if( maxPlot - minPlot < range )
            maxPlot += perDiv;
    }

    pGlobal->plot.axis[ eFreq ].min = minPlot;
    pGlobal->plot.axis[ eFreq ].max = maxPlot;
    pGlobal->plot.axis[ eFreq ].offset = 0.0;
    pGlobal->plot.axis[ eFreq ].perDiv = perDiv;

    return rtnStatus;
}


/*!     \brief  Determine the X and Y plot scales based on the data (autoranging)
 *
 * Determine the X and Y plot scales based on the data
 *
 * \param pGlobal       pointer to the global data structure
 * \param min           smallest number in plot
 * \param max           largest number in plot
 * \param gridType      grid that we are looking at
 * \return              0
 */
gint
quantizePlotRange( tGlobal *pGlobal, gdouble min, gdouble max, tGridAxes gridType ) {
    gdouble gridRange, division, quantizedMin, quantizedMax, border;
    gdouble logDecade, logFraction, log10diff, minRange = 0.1;
    gint rtnStatus = 0;
    gdouble quantum, headroom;
    int i;

    const static gdouble logRanges[ N_RANGES ] = { LOG10, LOG5, LOG2, LOG1 };
    const static gdouble minNoiseRange[eMAX_NOISE_UNITS]
                             = { MIN_RANGE_NOISE_FdB, MIN_RANGE_NOISE_F, MIN_RANGE_NOISE_YdB, MIN_RANGE_NOISE_Y, MIN_RANGE_NOISE_Y };


    log10diff = log10(max-min);
    logFraction = modf( log10diff, &logDecade );

    // If the log difference is less than one (difference is fractional),
    // divide the decade by 10 (subtract 1 from log) and add one to fraction to make positive
    if( log10diff < 0.0 ) {
        logDecade -= 1.0;
        logFraction += 1.0;
    }

    for( i = 1, quantum  = logRanges[0], headroom = logRanges[0] - logFraction; i < N_RANGES; i++ ) {
        if((logRanges[ i ] - logFraction) > 0.0 && headroom > (logRanges[ i ] - logFraction)) {
            headroom = (logRanges[ i ] - logFraction);
            quantum = logRanges[ i ];
        }
    }

    // create the range
    gridRange = pow( 10.0, logDecade + quantum );

    if( gridType == eNoise ) {
        if( pGlobal->plot.flags.bCalibrationPlot )
            minRange = NOISE_MIN_RANGE_CALIBRATION;
        else
            minRange = minNoiseRange[ pGlobal->plot.noiseUnits ];
    } else {
        minRange = MIN_RANGE_GAINdB;
    }

    if( gridRange < minRange )
        gridRange = minRange;

    // ten divisions per grid
    division = gridRange/10.0;

    // The data fits between grids
    quantizedMin = floor( min / division ) * division;
    quantizedMax = ceil( max / division )  * division;

    // half the remaining grid squares (may be zero)
    border = floor((gridRange - (quantizedMax-quantizedMin)) / division / 2.0) * division;

    pGlobal->plot.axis[ gridType ].min = quantizedMin - border;
    pGlobal->plot.axis[ gridType ].max = pGlobal->plot.axis[ gridType ].min + gridRange;
    pGlobal->plot.axis[ gridType ].offset = 0.0;
    pGlobal->plot.axis[ gridType ].perDiv = division;

    return  rtnStatus;
}

/*!     \brief  Change the font size
 *
 * Set the font size using the current scaling
 *
 * \ingroup drawing
 *
 * \param cr        pointer to cairo context
 * \param fSize     font size (scaled)
 *
 */
void
setCairoFontSize( cairo_t *cr, gdouble fSize ) {
    cairo_matrix_t fMatrix = {0, .xx=fSize, .yy=-fSize};
    cairo_set_font_matrix(cr, &fMatrix);
}

/*!     \brief  Generate a time string (minutes and seconds of an hour only) from milliseconds since Unix epoch.
 *
 * Generate a time string (minutes and seconds of an hour only) from milliseconds since Unix epoch. 1/1/1970
 *
 * \ingroup drawing
 *
 * \param msTime     ms since 1/1/1970
 * \param bShort     TRUE if no fractional seconds returned in string
 * \return           malloced string with mm:ss (or mm:ss.ss)
 *
 */
gchar *
msTimeToString( gint64 msTime, gboolean bShort ) {
    gchar *sTime = NULL;

    GDateTime   *msTimeGD = g_date_time_new_from_unix_local ( msTime / 1000 );
    if( bShort )
        sTime = g_strdup_printf( "%02d:%02d", g_date_time_get_minute( msTimeGD ), g_date_time_get_second( msTimeGD ) );
    else
        sTime = g_strdup_printf( "%02d:%02d:%02d.%01ld", g_date_time_get_hour( msTimeGD ), g_date_time_get_minute( msTimeGD ),
                                 g_date_time_get_second( msTimeGD ), (msTime / 100) % 10  );

    g_date_time_unref( msTimeGD );
    return sTime;
}


/*!     \brief  Determine the X and Y plot scales based on the data and settings
 *
 * Determine the X and Y plot scales based on the data and settings (like autoscale or limits)
 *
 * \param pGlobal       pointer to the global data structure
 * \return              0
 */
gint
setPlotBoundaries( tGlobal *pGlobal ) {
    gint rtnStatus = 0;
    tCircularBuffer *pCircularBuffer = &pGlobal->plot.measurementBuffer;

    if( pGlobal->plot.flags.bSpotFrequencyPlot ) {
        // the ord.time us a gint64 (milliseconds)
        gdouble endTime = GINT_MSTIME_TO_DOUBLE( getItemFromCircularBuffer( pCircularBuffer, LAST_ITEM )->abscissa.time );    // in ms

        determineTimeExtremesInCircularBuffer( pCircularBuffer );

        // plot maximum is the last sample received
        // plot minimum is the maximum - TIME_PLOT_LENGTH
        pGlobal->plot.axis[ eFreqOrTime ].min = endTime - (TIME_PLOT_LENGTH * pGlobal->plot.smoothingFactor);
        pGlobal->plot.axis[ eFreqOrTime ].max = endTime;
        // the internal time grid has an offset from the edges
        pGlobal->plot.axis[ eFreqOrTime ].offset = (TIME_PLOT_LENGTH / TIME_DIVISIONS_PER_GRID * pGlobal->plot.smoothingFactor)
                                                        - fmod( endTime, (TIME_PLOT_LENGTH / TIME_DIVISIONS_PER_GRID) * pGlobal->plot.smoothingFactor );

        pGlobal->plot.axis[ eFreqOrTime ].perDiv = (TIME_PLOT_LENGTH / TIME_DIVISIONS_PER_GRID) * pGlobal->plot.smoothingFactor;
    } else
        quantizePlotFrequencyRange( pGlobal, pCircularBuffer->minAbscissa.freq / MHz(1.0), pCircularBuffer->maxAbscissa.freq / MHz(1.0) );

    if( pGlobal->flags.bAutoScaling || pGlobal->plot.flags.bCalibrationPlot ) {
        quantizePlotRange( pGlobal, pCircularBuffer->minNoise, pCircularBuffer->maxNoise, eNoise );
        quantizePlotRange( pGlobal, pCircularBuffer->minGain, pCircularBuffer->maxGain, eGain );
    } else {
        determineFixedGridDivisions( pGlobal, eNoise );
        determineFixedGridDivisions( pGlobal, eGain );
    }



    return rtnStatus;
}

/*!     \brief  Display title and time
 *
 * Display title and time on plot
 *
 * \param cr         pointer to cairo structure
 * \param pGrid      pointer to grid structure
 * \param sTitle     pointer to title string
 * \param sTime      pointer to time string
 * \param pGlobal    pointer to global data
 */
void
showTitleAndTime( cairo_t *cr, tGridParameters *pGrid, gchar *sTitle, gchar *sTime, tGlobal *pGlobal)
{
    cairo_save( cr ); {
        cairo_reset_clip( cr );
        //cairo_set_matrix (cr, &pGrid->initialMatrix);
        gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorTitle ] );

        if( pGlobal->flags.bShowHPlogo ) {
            /*
            cairo_move_to( cr, pGrid->leftGridPosn, pGrid->areaHeight * 0.945 );
            cairo_renderHewlettPackardLogo(cr, TRUE, FALSE, 1.0, pGrid->gridHeight * 0.05 );
            cairo_move_to(cr, pGrid->leftGridPosn * 1.9, pGrid->areaHeight * 0.955 );
            */
            cairo_move_to( cr, pGrid->areaWidth * 0.84, pGrid->areaHeight * 0.950 );
            cairo_renderHewlettPackardLogo(cr, TRUE, FALSE, 1.0, pGrid->gridHeight * 0.030 );
            cairo_select_font_face(cr, LABEL_FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            rightJustifiedCairoText( cr, pGlobal->flags.bbHP8970Bmodel == 0 ? "8970A" : "8970B", pGrid->rightGridPosn, pGrid->areaHeight * 0.955 , FALSE );

            cairo_move_to(cr, pGrid->leftGridPosn * 1.9, pGrid->areaHeight * 0.955 );
        } else {
            cairo_move_to(cr, pGrid->leftGridPosn, pGrid->areaHeight * 0.955 );
        }
        cairo_move_to(cr, pGrid->leftGridPosn, pGrid->areaHeight * 0.955 );

        // Title
        cairo_select_font_face(cr, LABEL_FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        setCairoFontSize(cr, pGrid->fontSize * 1.3); // initially 10 pixels
        cairo_show_text (cr, sTitle);

        // Time
        if( pGlobal->flags.bShowTime ) {
            cairo_select_font_face(cr, LABEL_FONT, CAIRO_FONT_SLANT_ITALIC, CAIRO_FONT_WEIGHT_NORMAL);
            setCairoFontSize(cr, pGrid->fontSize * 0.9); // initially 10 pixels
            rightJustifiedCairoText( cr,  sTime, pGrid->rightGridPosn, pGrid->fontSize * 1.0, FALSE );
        }

    } cairo_restore( cr );
}

gchar *sNoiseLabel[ eMAX_NOISE_UNITS ] = {"Noise Figure", "Noise Factor", "Y Factor", "Y Factor", "Temperature"};
gchar *sNoiseUnits[ eMAX_NOISE_UNITS ] = {"dB", "", "dB", "", "K"};

/*!     \brief  Plot grid
 *
 * Draw the grid
 *
 * \param cr            pointer to cairo structure
 * \param pGrid         pointer to grid settings
 * \param pGlobal       pointer to the global data structure
 * \return              0
 */
gint
plotGrid( cairo_t *cr, tGridParameters *pGrid, tGlobal *pGlobal ) {

#define NOMINAL_NO_DIVISIONS    10
    gint i;
    gboolean bAdditionalLines = FALSE;
    gdouble dash[] = { pGrid->gridHeight / 200.0, pGrid->gridHeight / 200.0 };

    gchar sLegend[ SHORT_STRING ];
    gdouble freqOrTime, noise, gain;
    gdouble pixelsPerUnit;
    gboolean bSpotFreqency = pGlobal->plot.flags.bSpotFrequencyPlot;

    tAxis *pFreqAxis = &pGlobal->plot.axis[ eFreqOrTime ];
    tAxis *pNoiseAxis = &pGlobal->plot.axis[ eNoise ];
    tAxis *pGainAxis = &pGlobal->plot.axis[ eGain ];

    gdouble x, y;
    gint dp = 0;

    setPlotBoundaries( pGlobal );

    gboolean bDifferentGrids = fabs((pNoiseAxis->max - pNoiseAxis->min) / pNoiseAxis->perDiv
                                        - (pGainAxis->max - pGainAxis->min) / pGainAxis->perDiv)
                                                > VERY_SMALL
                                                || pNoiseAxis->offset > VERY_SMALL
                                                || pGlobal->plot.axis[ eGain  ].offset > VERY_SMALL;
    cairo_save(cr);
    {
        setCairoFontSize(cr, pGrid->fontSize);

        // box containing grid
        gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorGrid   ] );
        cairo_set_line_width (cr, pGrid->areaWidth / 2000.0 );
        cairo_new_path( cr );
        cairo_rectangle ( cr, pGrid->leftGridPosn, pGrid->bottomGridPosn,
                          pGrid->gridWidth, pGrid->gridHeight );
        cairo_stroke( cr );

        cairo_select_font_face (cr, LABEL_FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

//  X frequency or time (spot frequency) grid

        if( bSpotFreqency ) {
            pixelsPerUnit = pGrid->gridWidth / (TIME_PLOT_LENGTH * pGlobal->plot.smoothingFactor);
        } else {
            pixelsPerUnit = pGrid->gridWidth / (pFreqAxis->max - pFreqAxis->min);
            bAdditionalLines = (pFreqAxis->max - pFreqAxis->min) / pFreqAxis->perDiv < 10;
        }

        if( bSpotFreqency ) {
            x = pGrid->leftGridPosn + pFreqAxis->offset * pixelsPerUnit;
            i=0;
        } else {
            x = pGrid->leftGridPosn + pFreqAxis->perDiv * pixelsPerUnit/2.0;
            i=1;
        }

// Inner vertical frequency divisions
        for( ;x < pGrid->rightGridPosn; x += (pFreqAxis->perDiv * pixelsPerUnit/2.0), i++) {

            if( ( !bAdditionalLines && ODD(i) ) || fabs( round(x) - round( pGrid->rightGridPosn ) )  < 1.0 )
                continue;

            cairo_new_path( cr );
            cairo_move_to( cr, x, pGrid->bottomGridPosn );
            cairo_rel_line_to( cr, 0, pGrid->gridHeight );
            if( bAdditionalLines && ODD(i) )
                cairo_set_dash( cr, dash, 2, 0.0 );
            else
                cairo_set_dash( cr, dash, 0, 0.0 );
            cairo_stroke( cr );
        }

// Show logo, title and time
        showTitleAndTime( cr, pGrid, pGlobal->plot.sTitle, pGlobal->plot.sDateTime, pGlobal );

        gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorFrequency   ] );

        modf( pFreqAxis->min / pFreqAxis->perDiv, &freqOrTime );

        if( bSpotFreqency ) {
            x = pGrid->leftGridPosn + pFreqAxis->offset * pixelsPerUnit;
        	freqOrTime = (freqOrTime + 1.0) * pFreqAxis->perDiv;
        } else {
        	freqOrTime = pFreqAxis->min + pFreqAxis->perDiv;
            x = pGrid->leftGridPosn + pFreqAxis->perDiv * pixelsPerUnit;
        }
// Internal frequency annotations
        for(; x < pGrid->rightGridPosn;
                    x += (pFreqAxis->perDiv * pixelsPerUnit),
                    		freqOrTime += pFreqAxis->perDiv ) {

            if( freqOrTime < 0.0 )
                continue;

            if( x < pGrid->leftGridPosn + (pFreqAxis->perDiv * pixelsPerUnit) * 0.4
                    || x > pGrid->rightGridPosn - (pFreqAxis->perDiv * pixelsPerUnit) * 0.4 )
                continue;

            if( bSpotFreqency ) {
                gchar * sTime = msTimeToString( (gint64)(freqOrTime * 1000.0), TRUE );
                g_snprintf( sLegend, SHORT_STRING, "%s", sTime );
                g_free( sTime );
            }
            else {
                g_snprintf( sLegend, SHORT_STRING, "%.*lf", pFreqAxis->perDiv < 1.0 ? 1:0, freqOrTime );
            }

            if( bSpotFreqency || (x > pGrid->leftGridPosn + pGrid->fontSize * 2.0
                    && x < pGrid->rightGridPosn - pGrid->fontSize * 2.0) )
                centreJustifiedCairoText(cr, sLegend, x, pGrid->bottomGridPosn - 1.6 * pGrid->fontSize, 0.0 );
        }
// left hand frequency/time annotation (min)
        if( bSpotFreqency ) {
            gchar * sTime = msTimeToString( (gint64)(pFreqAxis->min * 1000.0), TRUE );
            g_snprintf( sLegend, SHORT_STRING, "%s", sTime );
            g_free( sTime );
        } else {
            g_snprintf( sLegend, SHORT_STRING, "%.*lf", pFreqAxis->perDiv < 1.0 ? 1:0,
                    pFreqAxis->min );
        }
        centreJustifiedCairoText(cr, sLegend, pGrid->leftGridPosn, pGrid->bottomGridPosn - 1.6 * pGrid->fontSize, 1.0 );
// right hand frequency/time annotation (max)
        if( bSpotFreqency ) {
            gchar * sTime = msTimeToString( (gint64)(pFreqAxis->max * 1000.0), TRUE );
            g_snprintf( sLegend, SHORT_STRING, "%s", sTime );
            g_free( sTime );
        } else {
            g_snprintf( sLegend, SHORT_STRING, "%.*lf", pFreqAxis->perDiv < 1.0 ? 1:0, pFreqAxis->max );
        }
        centreJustifiedCairoText(cr, sLegend, pGrid->rightGridPosn, pGrid->bottomGridPosn - 1.6 * pGrid->fontSize, 1.0 );

// X legend (Frequency or Time)
        setCairoFontSize(cr, pGrid->fontSize * 1.2);
        centreJustifiedCairoText(cr, bSpotFreqency ? "Time (mm:ss)" : "Frequency (MHz)", pGrid->leftGridPosn + pGrid->gridWidth / 2.0,
                                 pGrid->bottomGridPosn - 4.0 * pGrid->fontSize, 0.0 );
        setCairoFontSize(cr, pGrid->fontSize);
        if( bSpotFreqency ) {
            g_snprintf( sLegend, SHORT_STRING, "Frequency: %.0lf MHz", pGlobal->plot.freqSpotMHz );
            leftJustifiedCairoText(cr, sLegend, pGrid->leftGridPosn, pGrid->bottomGridPosn - 4.0 * pGrid->fontSize, 1.0 );
        }


// Y (left) noise grid

        if( pNoiseAxis->perDiv < 0.1 )
            dp = 2;
        else if ( pNoiseAxis->perDiv < 1.0 )
            dp = 1;
        else
            dp = 0;

        gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorGrid   ] );
        pixelsPerUnit = pGrid->gridHeight / (pNoiseAxis->max - pNoiseAxis->min);
        bAdditionalLines = (pNoiseAxis->max - pNoiseAxis->min)
                                                                / pNoiseAxis->perDiv < 9.001;

// Noise: horizontal grid lines
        for( y = pGrid->bottomGridPosn + pNoiseAxis->offset * pixelsPerUnit, i = 0;
                y < pGrid->topGridPosn; y += (pNoiseAxis->perDiv * pixelsPerUnit/2.0), i++) {

            if( (y == pGrid->bottomGridPosn) || (!bAdditionalLines && ODD(i))  )
                continue;

            cairo_new_path( cr );
            cairo_move_to( cr, pGrid->leftGridPosn, y );
            cairo_rel_line_to( cr, pGrid->gridWidth, 0 );
            if( bAdditionalLines && ODD(i) )
                cairo_set_dash( cr, dash, 2, 0.0 );
            else
                cairo_set_dash( cr, dash, 0, 0.0 );
            cairo_stroke( cr );
        }
        // add a possible missing fill in semi line
        if( bAdditionalLines && pNoiseAxis->offset > pNoiseAxis->perDiv / 2.0 ) {
            cairo_set_dash( cr, dash, 2, 0.0 );
            cairo_move_to( cr, pGrid->leftGridPosn, pGrid->bottomGridPosn +
                           (pNoiseAxis->offset - pNoiseAxis->perDiv/2.0) * pixelsPerUnit );
            cairo_rel_line_to( cr, pGrid->gridWidth, 0 );
            cairo_stroke( cr );
        }

        // show min noise annotation
        gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorNoise   ] );
        g_snprintf( sLegend, SHORT_STRING, "%*.*lf", 2+2*dp, dp, pNoiseAxis->min );
        rightJustifiedCairoText(cr, sLegend, pGrid->leftGridPosn - 0.5 * pGrid->fontSize,
                                pGrid->bottomGridPosn - pGrid->fontSize * 0.3, FALSE );

        noise = pNoiseAxis->min + pNoiseAxis->offset;

// Noise (left y) annotations
        for( y = pGrid->bottomGridPosn + pNoiseAxis->offset * pixelsPerUnit; y < pGrid->topGridPosn;
                y += (pNoiseAxis->perDiv * pixelsPerUnit), noise += pNoiseAxis->perDiv ) {

            // Don't clobber the text on either extreme
            if( (y < pGrid->bottomGridPosn + 1.5 * pGrid->fontSize)
                    || y > pGrid->topGridPosn - 1.5 * pGrid->fontSize)
                continue;

            g_snprintf( sLegend, SHORT_STRING, "%*.*lf", 2+2*dp, dp, noise );
            rightJustifiedCairoText(cr, sLegend, pGrid->leftGridPosn - 0.5 * pGrid->fontSize, y - pGrid->fontSize * 0.3, FALSE  );
        }
        // max noise on grid
        g_snprintf( sLegend, SHORT_STRING, "%*.*lf", 2+2*dp, dp, pNoiseAxis->max );
        rightJustifiedCairoText(cr, sLegend, pGrid->leftGridPosn - 0.5 * pGrid->fontSize, pGrid->topGridPosn - pGrid->fontSize * 0.3, FALSE );

        cairo_matrix_t matrix;
        cairo_get_matrix ( cr, &matrix );
        cairo_translate( cr, 0 + pGrid->fontSize * 1.6, pGrid->bottomGridPosn + pGrid->gridHeight / 2.0 );
        cairo_rotate(  cr, M_PI/2.0 );
        if( sNoiseUnits[ pGlobal->plot.noiseUnits ][0] != 0 )
            g_snprintf( sLegend, SHORT_STRING, "%s (%s)", sNoiseLabel[ pGlobal->plot.noiseUnits ],
                    sNoiseUnits[ pGlobal->plot.noiseUnits ] );
        else
            g_snprintf( sLegend, SHORT_STRING, "%s", sNoiseLabel[ pGlobal->plot.noiseUnits ] );

        setCairoFontSize(cr, pGrid->fontSize * 1.2);
        centreJustifiedCairoText(cr, sLegend, 0.0, 0.0, 0.0 );
        setCairoFontSize(cr, pGrid->fontSize);
        cairo_set_matrix ( cr, &matrix );


// Y (right) gain grid

        if( pGlobal->plot.flags.bValidGainData ) {
            if( pGainAxis->perDiv < 0.1 )
                dp = 2;
            else if ( pGainAxis->perDiv < 1.0 )
                dp = 1;
            else
                dp = 0;

            gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorGridGain   ] );
            pixelsPerUnit = pGrid->gridHeight / (pGainAxis->max - pGainAxis->min);

            bAdditionalLines = (pGainAxis->max - pGainAxis->min)
                    / pGainAxis->perDiv < 9.001;

// Gain: horizontal lines (between min and max)
            for( y = pGrid->bottomGridPosn + pGainAxis->offset * pixelsPerUnit, i = 0;
                    bDifferentGrids && y < pGrid->topGridPosn; y += (pGainAxis->perDiv * pixelsPerUnit/2.0), i++) {

                if( (y == pGrid->bottomGridPosn) || (!bAdditionalLines && ODD(i))  )
                    continue;

                cairo_new_path( cr );
                cairo_move_to( cr, pGrid->leftGridPosn, y );
                cairo_rel_line_to( cr, pGrid->gridWidth, 0 );

                if( bAdditionalLines && ODD(i) )
                    cairo_set_dash( cr, dash, 2, 0.0 );
                else
                    cairo_set_dash( cr, dash, 0, 0.0 );
                cairo_stroke( cr );
            }
            // add a possible missing fill in semi-line
            if( bAdditionalLines && pGainAxis->offset > pGainAxis->perDiv / 2.0 ) {
                cairo_set_dash( cr, dash, 2, 0.0 );
                cairo_move_to( cr, pGrid->leftGridPosn, pGrid->bottomGridPosn +
                               (pGainAxis->offset - pGainAxis->perDiv/2.0) * pixelsPerUnit );
                cairo_rel_line_to( cr, pGrid->gridWidth, 0 );
                cairo_stroke( cr );
            }

            gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorGain   ] );
            gdouble dummy;
            if( dp == 0 && (modf( pGainAxis->min, &dummy ) > VERY_SMALL
                            || modf( pGainAxis->max, &dummy ) > VERY_SMALL ))
                dp = 1;

            g_snprintf( sLegend, SHORT_STRING, "%*.*lf", 2+2*dp, dp, pGainAxis->min );
            leftJustifiedCairoText(cr, sLegend, pGrid->rightGridPosn + 0.5 * pGrid->fontSize, pGrid->bottomGridPosn - pGrid->fontSize * 0.3, FALSE );

            noise = pGainAxis->min + pGainAxis->offset;

// gain (right Y) annotations
            for( y = pGrid->bottomGridPosn + pGainAxis->offset * pixelsPerUnit; y < pGrid->topGridPosn;
                    y += (pGainAxis->perDiv * pixelsPerUnit), gain += pGainAxis->perDiv ) {

                // Don't clobber the text on either extreme
                if( (y < pGrid->bottomGridPosn + 1.5 * pGrid->fontSize)
                        || y > pGrid->topGridPosn - 1.5 * pGrid->fontSize)
                    continue;

                g_snprintf( sLegend, SHORT_STRING, "%*.*lf", 2+2*dp, dp, gain );
                leftJustifiedCairoText(cr, sLegend, pGrid->rightGridPosn + 0.5 * pGrid->fontSize, y - pGrid->fontSize * 0.3, FALSE  );
            }

            g_snprintf( sLegend, SHORT_STRING, "%*.*lf", 2+2*dp, dp, pGainAxis->max );
            leftJustifiedCairoText(cr, sLegend, pGrid->rightGridPosn + 0.5 * pGrid->fontSize, pGrid->topGridPosn - pGrid->fontSize * 0.3, FALSE );

            cairo_get_matrix ( cr, &matrix );
            cairo_translate( cr, 0 + pGrid->areaWidth - pGrid->fontSize * 1.5, pGrid->bottomGridPosn + pGrid->gridHeight / 2.0 );
            cairo_rotate(  cr, -M_PI/2.0 );
            setCairoFontSize(cr, pGrid->fontSize * 1.2);
            centreJustifiedCairoText(cr, "Gain (dB)", 0.0, 0.0, 0.0 );
            cairo_set_matrix ( cr, &matrix );
        }

    }
    cairo_restore( cr );

    return 0;
}

/*!     \brief  Mirror the Cairo text vertically
 *
 * The drawing space is fliped to move 0,0 to the bottom left. This has the
 * side effect of also flipping the way text is rendered.
 * This routine flips the text back to normal.
 *
 * \ingroup drawing
 *
 * \param cr    pointer to cairo context
 *
 */
void
flipCairoText( cairo_t *cr )
{
    cairo_matrix_t font_matrix;
    cairo_get_font_matrix( cr, &font_matrix );
    font_matrix.yy = -font_matrix.yy ;
    cairo_set_font_matrix( cr, &font_matrix );
}

/*!     \brief  Mirror the Cairo coordinate system vertically
 *
 * The drawing space is fliped to move 0,0 to the bottom left.
 *
 * \ingroup drawing
 *
 * \param cr        pointer to cairo context
 * \param pGrid    pointer to parameters calculated for the grid setup
 *
 */
void
flipVertical( cairo_t *cr, tGridParameters *pGrid ) {
    cairo_matrix_t font_matrix;
    // first make 0, 0 the bottom left corner and sane direction
    cairo_translate( cr, 0.0, pGrid->areaHeight);
    cairo_scale( cr, 1.0, -1.0);
    // we need to flip the font back (otherwise it will be upside down)
    cairo_get_font_matrix( cr, &font_matrix );
    font_matrix.yy = -font_matrix.yy ;
    cairo_set_font_matrix( cr, &font_matrix );
}

/*!     \brief  Calculate the grid parameters
 *
 * Calculate the grid paramaters
 *
 * \ingroup drawing
 *
 * \param pGrid      pointer to parameters calculated for the grid setup
 * \param areaWidth  width in cairo units (points) of the drawing area
 * \param areaHeight height in cairo units (points) of the drawing area
 * \param bSuppressLiveMarker if we are not displaying the live marker (printing but do not have a frozen live marker)
 *
 */
void
setGrid( tGridParameters *pGrid, gint areaWidth, gint areaHeight, gboolean bSuppressLiveMarker ) {
    pGrid->areaWidth = areaWidth;
    pGrid->areaHeight = areaHeight;

    pGrid->gridWidth = areaWidth * 0.83;
    pGrid->gridHeight = areaHeight * 0.83;

    pGrid->leftGridPosn = (areaWidth - pGrid->gridWidth) / 2.0;
    pGrid->rightGridPosn = pGrid->leftGridPosn + pGrid->gridWidth;

    pGrid->bottomGridPosn = areaHeight * 0.10;
    pGrid->topGridPosn = pGrid->bottomGridPosn + pGrid->gridHeight;

    pGrid->fontSize = areaWidth / 70.0;

    pGrid->bSuppressLiveMarker = bSuppressLiveMarker;
}

/*!     \brief  Interpolate the trace between measuring points
 *
 * Interpolate the trace between measuring points, The live marker may be between measurement points but
 * we still wish to show the interpolated value.
 *
 * \ingroup drawing
 *
 * \param pGlobal         pointer to global data
 * \param targetX         x position that we wish to get the interpolated ordinate for
 * \param freqOrTimeScale scale value for the abscissa
 * \param freqOrTime      frequency or time x
 * \param whichGrid       noise or gain to return
 * \return                noise or gain interploated value
 */
tCoordinate
interpolate( tGlobal *pGlobal, gdouble targetX, gdouble freqOrTimeScale, tAbscissa freqOrTime, tGridAxes whichGrid ) {
    gdouble targetOrdinate;
    tCircularBuffer *pDataBuffer = &pGlobal->plot.measurementBuffer;
    gint    nearestIndex = 0;

    gdouble nearestOrdinate, testOrdinate;

    tCoordinate interpolatedPoint = {INVALID, 0.0};
    // We start our search for the nearest frequency or time between the first and last
    // items in the circular buffer
    gint start = 0, end = nItemsInCircularBuffer( pDataBuffer ) - 1 , mid = 0;

    // If the x position of the mouse is not in range, ignore
    if( freqOrTime == eFreqAbscissa ) {
        targetOrdinate = (targetX / freqOrTimeScale + pGlobal->plot.axis[ eFreqOrTime ].min) * MHz(1.0);
        if( targetOrdinate < getItemFromCircularBuffer( pDataBuffer, 0 )->abscissa.freq
        		|| targetOrdinate > getItemFromCircularBuffer( pDataBuffer, end )->abscissa.freq )
        	return interpolatedPoint;
    } else {
        targetOrdinate = (targetX / freqOrTimeScale + pGlobal->plot.axis[ eFreqOrTime ].min);
        if( targetOrdinate < GINT_MSTIME_TO_DOUBLE( getItemFromCircularBuffer( pDataBuffer, 0 )->abscissa.time )
        		|| targetOrdinate > GINT_MSTIME_TO_DOUBLE( getItemFromCircularBuffer( pDataBuffer, end )->abscissa.time ) )
            return interpolatedPoint;
    }

    // do the binary search (max log n times)
    while(start <= end){
        mid = (start + end) / 2;
        if( freqOrTime == eFreqAbscissa ) {
            nearestOrdinate = getItemFromCircularBuffer( pDataBuffer, nearestIndex )->abscissa.freq;
            testOrdinate    = getItemFromCircularBuffer( pDataBuffer, mid )->abscissa.freq;
        } else {
            nearestOrdinate = GINT_MSTIME_TO_DOUBLE( getItemFromCircularBuffer( pDataBuffer, nearestIndex )->abscissa.time);
            testOrdinate    = GINT_MSTIME_TO_DOUBLE( getItemFromCircularBuffer( pDataBuffer, mid )->abscissa.time);
        }

        if( fabs(testOrdinate - targetOrdinate) < fabs(nearestOrdinate - targetOrdinate)){
            nearestIndex = mid;
        } else if( fabs(testOrdinate - targetOrdinate) == fabs(nearestOrdinate - targetOrdinate) ){
            if( testOrdinate > nearestOrdinate )
                nearestIndex = mid;
        }

        if(testOrdinate == targetOrdinate ){
            nearestIndex = mid;
             break;
        } else if( testOrdinate < targetOrdinate ){
             start = mid + 1;
        } else{
             end = mid - 1;
        }
    }

    gdouble noiseInterpolation, gainInterpolation;
    gdouble fraction;
    tNoiseAndGain *pMeasurementNearest = getItemFromCircularBuffer( pDataBuffer, nearestIndex );

    // we have found the nearest sample, now interpolate between the samples or either side of the target
    if( freqOrTime == eFreqAbscissa )
        nearestOrdinate = pMeasurementNearest->abscissa.freq;
    else
        nearestOrdinate = GINT_MSTIME_TO_DOUBLE( pMeasurementNearest->abscissa.time );

    if( nearestOrdinate > targetOrdinate ) {
        tNoiseAndGain *pMeasurementBeforeNearest = getItemFromCircularBuffer( pDataBuffer, nearestIndex - 1 );
        if( freqOrTime == eFreqAbscissa )
            fraction = (targetOrdinate  - pMeasurementBeforeNearest->abscissa.freq) /
                                   (nearestOrdinate - pMeasurementBeforeNearest->abscissa.freq);
        else
            fraction = (targetOrdinate  - GINT_MSTIME_TO_DOUBLE( pMeasurementBeforeNearest->abscissa.time )) /
                                   (nearestOrdinate - GINT_MSTIME_TO_DOUBLE( pMeasurementBeforeNearest->abscissa.time ));

        noiseInterpolation = (pMeasurementBeforeNearest->noise * (1.0 - fraction))
                                + (fraction * pMeasurementNearest->noise);
        gainInterpolation = (pMeasurementBeforeNearest->gain * (1.0 - fraction))
                                + (fraction * pMeasurementNearest->gain);
    } else if( nearestOrdinate < targetOrdinate ) {
        tNoiseAndGain *pMeasurementAfterNearest = getItemFromCircularBuffer( pDataBuffer, nearestIndex + 1 );
        if( freqOrTime == eFreqAbscissa )
            fraction = (targetOrdinate - nearestOrdinate) /
                       (pMeasurementAfterNearest->abscissa.freq - nearestOrdinate);
        else
            fraction = (targetOrdinate - nearestOrdinate) /
                       (GINT_MSTIME_TO_DOUBLE(pMeasurementAfterNearest->abscissa.time) - nearestOrdinate);

        noiseInterpolation = (pMeasurementNearest->noise * (1.0 - fraction))
                                        + (fraction * pMeasurementAfterNearest->noise);
        gainInterpolation = (pMeasurementNearest->gain * (1.0 - fraction))
                                        + (fraction * pMeasurementAfterNearest->gain);
    } else {
        noiseInterpolation = pMeasurementNearest->noise;
        gainInterpolation  = pMeasurementNearest->gain;
    }

    interpolatedPoint.x = targetOrdinate;
    if( whichGrid == eNoise )
        interpolatedPoint.y = noiseInterpolation;
    else
        interpolatedPoint.y = gainInterpolation;
    return( interpolatedPoint );
}

/*!     \brief  Draw a floating number aligned to the decimal point
 *
 * Draw a floating number aligned to the decimal point
 *
 * \param cr            pointer to cairo structure
 * \param x             x location of DB in Cairo drawing units
 * \param y             y location of DB in Cairo drawing units
 * \param num           the floating point number
 * \param nAfterDB      digits to show after DP
 */
void
centerTextOnDP( cairo_t *cr, gdouble x, gdouble y, gdouble num, gint nAfterDP ) {
    gdouble fractional, integer;
    gchar str[ SHORT_STRING ];

    cairo_save(cr);
    fractional = modf(num, &integer);

    g_snprintf( str, SHORT_STRING, "%d", (gint)integer );
    rightJustifiedCairoText( cr, str, x, y, TRUE);
    g_snprintf( str, SHORT_STRING, ".%0*d", nAfterDP, (gint)(fractional * pow( 10.0, nAfterDP) )  );
    leftJustifiedCairoText( cr, str, x, y, TRUE);
    cairo_restore( cr );

    cairo_move_to( cr, x, y );
}

/*!     \brief  Clip the ordinate of the data point to just outside the grid
 *
 * Clip the ordinate of the data point to just outside the grid
 *
 * \param data            pointer to cairo structure
 * \param minimum         minimum y value
 * \param maximum         maximum
 * \return                the clipped value
 */
gdouble
clipData( gdouble data, gdouble minimum, gdouble maximum ) {
    gdouble margin = (maximum - minimum) / 150.0;

    if( data > maximum )
        data = maximum + margin;
    else if ( data < minimum )
        data = minimum - margin;

    return data;
}
#define UNIT_OFFSET_TEK 1.5
#define UNIT_OFFSET_FdB 2

/*!     \brief  Plot gain vs frequency onto drawing area
 *
 * Plot gain vs frequency onto drawing area
 *
 * \param cr            pointer to cairo structure
 * \param pGrid         pointer to grid parameters
 * \param pGlobal       pointer to the global data structure
 */
void
plotGainTrace( cairo_t *cr, tGridParameters  *pGrid, gpointer gpGlobal ) {
    tGlobal *pGlobal = (tGlobal *)gpGlobal;
    tAxis *pGainAxis = &pGlobal->plot.axis[ eGain ];
    tAxis *pFreqOrTimeAxis = &pGlobal->plot.axis[ eFreqOrTime ];

    gdouble freqOrTimeScale  = pGrid->gridWidth / ( pFreqOrTimeAxis->max - pFreqOrTimeAxis->min );
    gdouble gainScale = pGrid->gridHeight /( pGainAxis->max - pGainAxis->min );

    tCircularBuffer *pDataBuffer = &pGlobal->plot.measurementBuffer;

    gboolean bSpotFreqency = pGlobal->plot.flags.bSpotFrequencyPlot;

    cairo_save(cr);
    {
        setCairoFontSize(cr, pGrid->fontSize);

        // box containing grid
        cairo_new_path( cr );

        gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorGain   ] );
        cairo_set_line_width (cr, pGrid->areaWidth / 1000.0 );

        cairo_rectangle( cr, pGrid->leftGridPosn, 0.0, pGrid->gridWidth, pGrid->areaHeight );
        cairo_clip( cr );

        cairo_translate(cr, pGrid->leftGridPosn, pGrid->bottomGridPosn);

        tNoiseAndGain *pMeasurement;
        gint nMeasurements = nItemsInCircularBuffer( pDataBuffer );

        gdouble xPos;

        // plot the gain trace

        for( gint i=0; i < nMeasurements; i++ ) {
            pMeasurement = getItemFromCircularBuffer( pDataBuffer, i );
            if( bSpotFreqency )
                xPos = ( ((gdouble)pMeasurement->abscissa.time) / 1000.0 - pFreqOrTimeAxis->min ) * freqOrTimeScale;
            else
                xPos = (pMeasurement->abscissa.freq/MHz( 1.0 ) - pFreqOrTimeAxis->min ) * freqOrTimeScale;
            // Ignore what is off screen ...
            if( i == 0 || xPos <= 0.0 ) {
                cairo_move_to( cr, xPos, (clipData( pMeasurement->gain, pGainAxis->min, pGainAxis->max ) - pGainAxis->min ) * gainScale );
                continue;
            }

        	if( pMeasurement->gain < ERROR_INDICATOR_HP8970)
        		cairo_line_to( cr, xPos, (clipData( pMeasurement->gain, pGainAxis->min, pGainAxis->max ) - pGainAxis->min ) * gainScale );
        }
        cairo_stroke( cr );

        if( pGlobal->flags.bLiveMarkerActive && (pGlobal->flags.bHoldLiveMarker || !pGrid->bSuppressLiveMarker) ) {
            cairo_reset_clip( cr );
            gdouble xLM = pGlobal->liveMarkerPosnRatio.x * pGrid->areaWidth - pGrid->leftGridPosn;
            gdouble yLM = (pGrid->areaHeight - pGlobal->liveMarkerPosnRatio.y * pGrid->areaHeight ) - pGrid->bottomGridPosn;

            if( xLM > 0.0 && xLM < pGrid->gridWidth ) {
                tCoordinate intercept = interpolate( pGlobal, xLM, freqOrTimeScale, bSpotFreqency ? eTimeAbscissa : eFreqAbscissa, eGain  );

                if( intercept.x != INVALID ) {
                    // The x position of the live marker was already calculated, so no need to change it
			      //xLM = (intercept.x * unitScale - pGlobal->plot.axis[ eFreqOrTime ].min ) * freqOrTimeScale;
					yLM = (intercept.y - pGainAxis->min ) * gainScale;
					// red circle with some transparency
					cairo_arc (cr, xLM, yLM, pGrid->gridWidth / 100.0, 0.0, 2 * M_PI);
					cairo_set_line_width (cr, pGrid->areaWidth / 1000.0 );
					cairo_set_source_rgba (cr, 1.0, 0.0, 0.0, 0.3);
					cairo_stroke (cr);
					// red inner solid point
					cairo_arc (cr, xLM, yLM, pGrid->gridWidth / 400.0, 0.0, 2 * M_PI);
					cairo_set_line_width (cr, 0.0 );
					cairo_set_source_rgba (cr, 1.0, 0.0, 0.0, 1.0);
					cairo_fill_preserve (cr);
					cairo_stroke (cr);
					// red line on the right hand side (gain annotations)
					cairo_set_line_width (cr, pGrid->areaWidth / 700.0 );
					cairo_move_to ( cr, pGrid->gridWidth, yLM );
					cairo_rel_line_to ( cr, pGrid->gridWidth / 80.0, 0.0 );
					cairo_stroke ( cr );
					// Live marker annotation of gain in bottom left
					gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorGain   ] );
					cairo_select_font_face (cr, LABEL_FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

					gdouble yPosLiveMkrText = 0.5 * pGrid->fontSize;
					gdouble xPosLiveMkrText = pGrid->gridWidth * 0.16;

					setCairoFontSize (cr, pGrid->fontSize * 0.9); // initially 10 pixels
					// Gain: name
					rightJustifiedCairoText ( cr, "Gain:", xPosLiveMkrText - 2.8 * pGrid->fontSize, yPosLiveMkrText, TRUE);
					// value
					centerTextOnDP ( cr, xPosLiveMkrText, yPosLiveMkrText, intercept.y, 2 );
					cairo_rel_move_to( cr, pGrid->fontSize * (pGlobal->plot.noiseUnits == eTeK ? UNIT_OFFSET_TEK : UNIT_OFFSET_FdB), 0.0 );
					cairo_show_text (cr, "dB");
				}
            }
        }
    }
    cairo_restore(cr);
}

/*!     \brief  Plot noise figure/factor vs frequency onto drawing area
 *
 * Plot noise figure vs frequency onto drawing area
 *
 * \param cr            pointer to cairo structure
 * \param pGrid         pointer to grid parameters
 * \param pGlobal       pointer to the global data structure
 */
void
plotNoiseTrace( cairo_t *cr, tGridParameters  *pGrid, gpointer gpGlobal ) {
    tGlobal *pGlobal = (tGlobal *)gpGlobal;
    tAxis *pNoiseAxis = &pGlobal->plot.axis[ eNoise ];
    tAxis *pFreqOrTimeAxis = &pGlobal->plot.axis[ eFreqOrTime ];

    gdouble ordinateScaling  = pGrid->gridWidth / ( pFreqOrTimeAxis->max - pFreqOrTimeAxis->min );
    gdouble noiseScale = pGrid->gridHeight /( pNoiseAxis->max - pNoiseAxis->min );

    tCircularBuffer *pDataBuffer = &pGlobal->plot.measurementBuffer;

    gboolean bSpotFreqency = pGlobal->plot.flags.bSpotFrequencyPlot;

    cairo_save(cr);
    {
        setCairoFontSize(cr, pGrid->fontSize);

        // box containing grid
        cairo_new_path( cr );

        gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorNoise   ] );
        cairo_set_line_width (cr, pGrid->areaWidth / 1000.0 );

        cairo_rectangle( cr, pGrid->leftGridPosn, 0.0, pGrid->gridWidth, pGrid->areaHeight );
        cairo_clip( cr );

        // origin at the bottom of the grid (not screen)
        cairo_translate(cr, pGrid->leftGridPosn, pGrid->bottomGridPosn);

        tNoiseAndGain *pMeasurement;
        gint nMeasurements = nItemsInCircularBuffer( pDataBuffer );

        gdouble xPos;
        // plot the noise trace
        for( int i=0; i < nMeasurements; i++ ) {
            pMeasurement = getItemFromCircularBuffer( pDataBuffer, i );
            if( bSpotFreqency )
                xPos = ( GINT_MSTIME_TO_DOUBLE( pMeasurement->abscissa.time) - pFreqOrTimeAxis->min ) * ordinateScaling;
            else
                xPos = (pMeasurement->abscissa.freq/MHz(1.0) - pFreqOrTimeAxis->min ) * ordinateScaling;

            // Ignore what is off screen ...
            if( i == 0 || xPos <= 0.0 ) {
                cairo_move_to( cr, xPos, (clipData( pMeasurement->noise, pNoiseAxis->min, pNoiseAxis->max ) - pNoiseAxis->min ) * noiseScale );
                continue;
            }

        	if( pMeasurement->noise < ERROR_INDICATOR_HP8970)
        		cairo_line_to( cr, xPos, (clipData( pMeasurement->noise, pNoiseAxis->min, pNoiseAxis->max ) - pNoiseAxis->min ) * noiseScale );
        }
        cairo_stroke( cr );

        // Live marker
        if( pGlobal->flags.bLiveMarkerActive && (pGlobal->flags.bHoldLiveMarker || !pGrid->bSuppressLiveMarker) ) {
            cairo_reset_clip( cr );

            // translate the live marker to the current screen geometry
            // with the origin at lower left of the plot grid (
            gdouble xLM = (pGlobal->liveMarkerPosnRatio.x * pGrid->areaWidth) - pGrid->leftGridPosn;
            gdouble yLM = (pGrid->areaHeight - pGlobal->liveMarkerPosnRatio.y * pGrid->areaHeight ) - pGrid->bottomGridPosn;

            // only valid if within the grid
            if( xLM > 0.0 && xLM < pGrid->gridWidth ) {
                // do a binary search for the ordinate (freq. or time) corresponding to the mouse x position on the grid,
                // and interpolate the coordinate (noise and gain) from the nearest samples.
                tCoordinate intercept = interpolate( pGlobal, xLM, ordinateScaling, bSpotFreqency ? eTimeAbscissa : eFreqAbscissa, eNoise  );

                // only valid if the data set covers the ordinate position.
                if( intercept.x != INVALID ) {
                    // Update xLM and yLM to the interpolated values (xLM doesn't change)
			     // xLM = (intercept.x * unitScale - pFreqOrTimeAxis->min ) * ordinateScaling;
					yLM = (intercept.y - pNoiseAxis->min ) * noiseScale;

					cairo_arc (cr, xLM, yLM, pGrid->gridWidth / 100.0, 0.0, 2 * M_PI);
					cairo_set_line_width (cr, pGrid->areaWidth / 1000.0 );
					cairo_set_source_rgba (cr, 1.0, 0.0, 0.0, 0.3);
					cairo_stroke (cr);
					cairo_arc (cr, xLM, yLM, pGrid->gridWidth / 400.0, 0.0, 2 * M_PI);
					cairo_set_line_width (cr, 0.0 );
					cairo_set_source_rgba (cr, 1.0, 0.0, 0.0, 1.0);
					cairo_fill_preserve (cr);
					cairo_stroke (cr);

					cairo_set_line_width (cr, pGrid->areaWidth / 700.0 );
					cairo_move_to ( cr, xLM, 0.0 );
					cairo_rel_line_to ( cr, 0.0, -pGrid->gridWidth / 80.0 );
					cairo_move_to ( cr, 0.0, yLM );
					cairo_rel_line_to ( cr, -pGrid->gridWidth / 80.0, 0.0 );
					cairo_stroke ( cr );

                    cairo_select_font_face(cr, LABEL_FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
                    setCairoFontSize(cr, pGrid->fontSize * 0.9); // initially 10 pixels

                    // Show the values (freq or time and noise) of the live marker, positions in the lower left
                    gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorFrequency   ] );

                    gdouble yPosLiveMkrText = 3.0 * pGrid->fontSize;        // just right of the left edge of the grid
                    gdouble xPosLiveMkrText = pGrid->gridWidth * 0.16;      // above the bottom of the grid with room for three rows of text

                    // Live Marker ordinate label
                    rightJustifiedCairoText( cr, pGlobal->plot.flags.bSpotFrequencyPlot ? "Time:" : "Frequency:",
                            xPosLiveMkrText - 2.8 * pGrid->fontSize, yPosLiveMkrText, TRUE);
                    // ordinate value ... frequency or time
                    if( pGlobal->plot.flags.bSpotFrequencyPlot ) {
                        // Time like 11:06:43.6 (hours, minutes & seconds (decimal seconds))
                        gchar *sTime = msTimeToString( (gint64)(intercept.x * 1000.0), FALSE );
                        rightJustifiedCairoText( cr, sTime, xPosLiveMkrText + 1.75 * pGrid->fontSize, yPosLiveMkrText, TRUE );
                        g_free( sTime );
                    } else {
                        // Frequency centered on the decimal point
                        centerTextOnDP( cr, xPosLiveMkrText, yPosLiveMkrText, intercept.x/MHz(1.0), 2 );
                        leftJustifiedCairoText( cr, "MHz",
                                                xPosLiveMkrText + pGrid->fontSize * (pGlobal->plot.noiseUnits == eTeK ? UNIT_OFFSET_TEK : UNIT_OFFSET_FdB),
                                                yPosLiveMkrText, TRUE);
                    }

                    gchar sLegend[ SHORT_STRING ];
                    g_snprintf( sLegend, SHORT_STRING, "%s:", sNoiseLabel[ pGlobal->plot.noiseUnits ]);

                    yPosLiveMkrText = 1.75 * pGrid->fontSize;  // spacing is 1.25 * font size

                    gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorNoise   ] );
                    rightJustifiedCairoText( cr, sLegend, xPosLiveMkrText - 2.8 * pGrid->fontSize, yPosLiveMkrText, TRUE);

                    centerTextOnDP( cr, xPosLiveMkrText, yPosLiveMkrText, intercept.y, pGlobal->plot.noiseUnits == eTeK ? 1:3 );
                    cairo_rel_move_to( cr, pGrid->fontSize * (pGlobal->plot.noiseUnits == eTeK ? UNIT_OFFSET_TEK : UNIT_OFFSET_FdB), 0.0 );

                    if( sNoiseUnits[ pGlobal->plot.noiseUnits ] != 0 )
                        cairo_show_text (cr, sNoiseUnits[ pGlobal->plot.noiseUnits ]);
                }
            }
        }
    }
    cairo_restore(cr);

}

/*!     \brief  Plot noise figure and gain onto drawing area
 *
 * Plot noise figure and gain onto drawing area
 * \param cr            pointer to cairo structure
 * \param areaWidth     width
 * \param areaHeight    height
 * \param bSuppressLiveMarker whether to suppress the live marker (say when printing)
 * \param pGlobal       pointer to the global data structure
 * \return           TRUE
 */
gboolean
plotNoiseFigureAndGain (cairo_t *cr, gint areaWidth, gint areaHeight, tGlobal *pGlobal, gboolean bSuppressLiveMarker)
{
    tGridParameters grid;

    cairo_font_options_t *pFontOptions = cairo_font_options_create();
    cairo_get_font_options (cr, pFontOptions);
    cairo_font_options_set_hint_style( pFontOptions, CAIRO_HINT_STYLE_NONE );
    cairo_font_options_set_hint_metrics( pFontOptions, CAIRO_HINT_METRICS_OFF );
    cairo_set_font_options (cr, pFontOptions);
    cairo_font_options_destroy( pFontOptions );

    setGrid( &grid, areaWidth, areaHeight, bSuppressLiveMarker );

    // clear the screen
    if( pGlobal->flags.bPreviewModeDiagram == FALSE
            && (pGlobal->plot.flags.bValidNoiseData || pGlobal->plot.flags.bValidGainData) ) {
        cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0 );
        cairo_paint( cr );

        flipVertical( cr, &grid );
        plotGrid( cr, &grid, pGlobal );

        if( pGlobal->plot.flags.bValidNoiseData )
            plotNoiseTrace( cr, &grid, pGlobal );
        if( pGlobal->plot.flags.bValidGainData )
            plotGainTrace( cr, &grid, pGlobal );

    } else {
        drawHPlogo ( cr, areaWidth / 2.0, areaHeight * 0.90, areaWidth / 1000.0, pGlobal->flags.bbHP8970Bmodel );
        drawModeDiagram( cr, pGlobal->HP8970settings.mode, pGlobal->flags.bbHP8970Bmodel, areaWidth, areaHeight, 0.70 );
    }
    return TRUE;
}

/*!     \brief  Signal received to draw the drawing area
 *
 * Draw the plot for area A
 *
 * \param widget        pointer to GtkDrawingArea widget
 * \param cr            pointer to cairo structure
 * \param areaWidth     width
 * \param areaHeight    height
 * \param pGlobal       pointer to the global data structure
 */
void
CB_DrawingArea_Draw (GtkDrawingArea *widget, cairo_t *cr,
				gint areaWidth, gint areaHeight, gpointer gpGlobal)
{
	tGlobal *pGlobal = (tGlobal *)gpGlobal;
	plotNoiseFigureAndGain (cr, areaWidth, areaHeight, pGlobal, FALSE);

}
