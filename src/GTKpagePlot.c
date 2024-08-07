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

/*! \file GTKpageOptions.c
 *  \brief Handle GUI elements on the GtkNotebook page for Options
 *
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <cairo/cairo.h>
#include <glib-2.0/glib.h>
#include <HP8970.h>
#include "messageEvent.h"
#include <math.h>

struct {
    gdouble min, max, step, page;
    gchar *unit;
} noiseSpinParams[ eMAX_NOISE_UNITS ] = {
        [eFdB]      = {0.00,   40.0, 0.1,   2.0, "FdB"},
        [eF]        = {1.00, 9999.0, 1.0, 100.0,   "F"},
        [eYdB]      = {0.00,   20.0, 0.1,   1.0, "YdB"},
        [eY]        = {1.00, 1000.0, 1.0, 100.0,   "Y"},
        [eTeK]      = {0.00, 9999.0, 1.0, 100.0, "TeK"}
};

void CB_ColorNotify ( GObject* self, GParamSpec* pspec, gpointer gpColor ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(self, "data");
    plotElementColors[ GPOINTER_TO_INT( gpColor ) ] = *gtk_color_dialog_button_get_rgba( GTK_COLOR_DIALOG_BUTTON( self ));

    gtk_widget_queue_draw ( WLOOKUP ( pGlobal, "drawing_Plot") );
}

/*!     \brief  Reset colors
 *
 * Reset colors
 *
 * \param  wButtonReset      pointer to reset button widget
 * \param  user_data         unused
 */
void
CB_ColorReset ( GtkButton* wBtnColorReset, gpointer user_data ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wBtnColorReset), "data");

    gpointer wColorTitle = WLOOKUP( pGlobal, "color_Title");
    gpointer wColorGrid  = WLOOKUP( pGlobal, "color_Grid");
    gpointer wColorGridGain  = WLOOKUP( pGlobal, "color_GridGain");
    gpointer wColorNoise = WLOOKUP( pGlobal, "color_Noise");
    gpointer wColorGain  = WLOOKUP( pGlobal, "color_Gain");
    gpointer wColorFreq  = WLOOKUP( pGlobal, "color_Freq");

    for( gint pen = 0; pen < eMAX_COLORS; pen++ ) {
        plotElementColors[ pen ] = plotElementColorsFactory[ pen ];
    }

    gtk_color_dialog_button_set_rgba( wColorTitle,  &plotElementColors[ eColorTitle ] );
    gtk_color_dialog_button_set_rgba( wColorGrid,  &plotElementColors[ eColorGrid ] );
    gtk_color_dialog_button_set_rgba( wColorGridGain,  &plotElementColors[ eColorGridGain ] );
    gtk_color_dialog_button_set_rgba( wColorNoise,  &plotElementColors[ eColorNoise ] );
    gtk_color_dialog_button_set_rgba( wColorGain,  &plotElementColors[ eColorGain ] );
    gtk_color_dialog_button_set_rgba( wColorFreq,  &plotElementColors[ eColorFrequency ] );

    gtk_widget_queue_draw ( WLOOKUP ( pGlobal, "drawing_Plot") );

}

/*!     \brief  Change auto scale setting
 *
 * Change auto scale setting
 *
 * \param  wChkAutoScale     pointer auto scale check button widget
 * \param  user_data         unused
 */
void CB_chk_AutoScale ( GtkCheckButton *wChkAutoScale, gpointer user_data
) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wChkAutoScale), "data");
    pGlobal->flags.bAutoScaling = gtk_check_button_get_active( WLOOKUP( pGlobal, "chk_AutoScale") );

    gtk_widget_queue_draw ( WLOOKUP ( pGlobal, "drawing_Plot") );
}

/*!     \brief  Callback for Gain Maximum spin button
 *
 * Callback for Gain Maximum spin button
 *
 * \param  wSpinGainMax   pointer to GtkSpinButton
 * \param  udata          user data
 */
void
CB_spin_GainMax(  GtkSpinButton* wSpinGainMax, gpointer udata ) {

    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wSpinGainMax), "data");
    gpointer wSpinGainMin  = WLOOKUP( pGlobal, "spin_GainMin");
    gpointer wChkAuto      = WLOOKUP( pGlobal, "chk_AutoScale");

    gdouble gainMax = gtk_spin_button_get_value( wSpinGainMax );

    if( gainMax - MIN_GAIN_RANGE < MIN_GAIN ) {
        g_signal_handlers_block_by_func( G_OBJECT(wSpinGainMax), CB_spin_GainMax, udata );
        gtk_spin_button_set_value( wSpinGainMax, MIN_GAIN_RANGE );
        g_signal_handlers_unblock_by_func( G_OBJECT(wSpinGainMax), CB_spin_GainMax, udata );
        pGlobal->fixedGridGain[ 1 ] = MIN_GAIN_RANGE;
    } else {
        pGlobal->fixedGridGain[ 1 ] = gainMax;
    }

    if( pGlobal->fixedGridGain[ 1 ] - MIN_GAIN_RANGE < pGlobal->fixedGridGain[ 0 ] ) {
        gtk_spin_button_set_value( wSpinGainMin, pGlobal->fixedGridGain[ 1 ] - MIN_GAIN_RANGE );
    }

    gtk_check_button_set_active( wChkAuto, FALSE );
    gtk_widget_queue_draw ( WLOOKUP ( pGlobal, "drawing_Plot") );
}

/*!     \brief  Callback for Gain Minimum spin button
 *
 * Callback for Gain Minimum spin button
 *
 * \param  wSpinGainMin   pointer to GtkSpinButton
 * \param  udata          user data
 */
void
CB_spin_GainMin(  GtkSpinButton* wSpinGainMin, gpointer udata ) {

    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wSpinGainMin), "data");
    gpointer wSpinGainMax  = WLOOKUP( pGlobal, "spin_GainMax");
    gpointer wChkAuto      = WLOOKUP( pGlobal, "chk_AutoScale");

    gdouble gainMin = gtk_spin_button_get_value( wSpinGainMin );

    if( gainMin + MIN_GAIN_RANGE > MAX_GAIN ) {
        g_signal_handlers_block_by_func( G_OBJECT(wSpinGainMax), CB_spin_GainMin, udata );
        gtk_spin_button_set_value( wSpinGainMax, MAX_GAIN - MIN_GAIN_RANGE );
        g_signal_handlers_unblock_by_func( G_OBJECT(wSpinGainMax), CB_spin_GainMin, udata );
        pGlobal->fixedGridGain[ 0 ] = MAX_GAIN - MIN_GAIN_RANGE;
    } else {
        pGlobal->fixedGridGain[ 0 ] = gainMin;
    }

    if( pGlobal->fixedGridGain[ 1 ]  < pGlobal->fixedGridGain[ 0 ] + MIN_GAIN_RANGE )
        gtk_spin_button_set_value( wSpinGainMax, pGlobal->fixedGridGain[ 0 ] + MIN_GAIN_RANGE );

    gtk_check_button_set_active( wChkAuto, FALSE );
    gtk_widget_queue_draw ( WLOOKUP ( pGlobal, "drawing_Plot") );
}


/*!     \brief  Callback for Noise Maximum spin button
 *
 * Callback for Noise Maximum spin button
 *
 * \param  wSpinNoiseMax  pointer to GtkSpinButton
 * \param  udata          user data
 */
void
CB_spin_NoiseMax(  GtkSpinButton* wSpinNoiseMax, gpointer udata ) {

    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wSpinNoiseMax), "data");
    gpointer wSpinNoiseMin  = WLOOKUP( pGlobal, "spin_NoiseMin");
    gpointer wChkAuto      = WLOOKUP( pGlobal, "chk_AutoScale");
    tNoiseType noiseUnits = pGlobal->plot.noiseUnits;

    gdouble step = noiseSpinParams[ noiseUnits ].step;

    gdouble noise = gtk_spin_button_get_value( wSpinNoiseMax );

    if( noise - step < gtk_spin_button_get_value( wSpinNoiseMin ) ) {
        gtk_spin_button_set_value( wSpinNoiseMin, noise - step );
    }

    pGlobal->fixedGridNoise[ noiseUnits ][ 1 ] = noise;

    gtk_check_button_set_active( wChkAuto, FALSE );
    gtk_widget_queue_draw ( WLOOKUP ( pGlobal, "drawing_Plot") );
}

/*!     \brief  Callback for Noise Minimum spin button
 *
 * Callback for Noise Minimum spin button
 *
 * \param  wSpinNoiseMin  pointer to GtkSpinButton
 * \param  udata          user data
 */
void
CB_spin_NoiseMin(  GtkSpinButton* wSpinNoiseMin, gpointer udata ) {

    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wSpinNoiseMin), "data");
    gpointer wSpinNoiseMax  = WLOOKUP( pGlobal, "spin_NoiseMax");
    gpointer wChkAuto      = WLOOKUP( pGlobal, "chk_AutoScale");
    tNoiseType noiseUnits = pGlobal->plot.noiseUnits;

    gdouble step = noiseSpinParams[ noiseUnits ].step;

    gdouble noise = gtk_spin_button_get_value( wSpinNoiseMin );

    if( noise + step > gtk_spin_button_get_value( wSpinNoiseMax ) ) {
        gtk_spin_button_set_value( wSpinNoiseMax, noise + step );
    }

    pGlobal->fixedGridNoise[ noiseUnits ][ 0 ] = noise;

    gtk_check_button_set_active( wChkAuto, FALSE );
    gtk_widget_queue_draw ( WLOOKUP ( pGlobal, "drawing_Plot") );
}



/*!     \brief  Set the parameters of the spin button for manual range noise plot
 *
 * Set the parameters of the spin button for manual range noise plot
 *
 * \param  pGlobal      pointer to global data
 * \param  noiseUnits   units of the noise plot data
 */
void
setSpinNoiseRange( tGlobal *pGlobal ) {
    gpointer wSpinNoiseMin  = WLOOKUP( pGlobal, "spin_NoiseMin");
    gpointer wSpinNoiseMax  = WLOOKUP( pGlobal, "spin_NoiseMax");
    gpointer wFrmNoiseRange = WLOOKUP( pGlobal, "frame_NoiseRange");

    gtk_spin_button_set_range( wSpinNoiseMin, noiseSpinParams[ pGlobal->plot.noiseUnits ].min,
                               noiseSpinParams[ pGlobal->plot.noiseUnits ].max - noiseSpinParams[ pGlobal->plot.noiseUnits ].step);
    gtk_spin_button_set_range( wSpinNoiseMax, noiseSpinParams[ pGlobal->plot.noiseUnits ].min + noiseSpinParams[ pGlobal->plot.noiseUnits ].step,
                               noiseSpinParams[ pGlobal->plot.noiseUnits ].max  );

    gtk_spin_button_set_increments( wSpinNoiseMin, noiseSpinParams[ pGlobal->plot.noiseUnits ].step,
                                    noiseSpinParams[ pGlobal->plot.noiseUnits ].page );
    gtk_spin_button_set_increments( wSpinNoiseMax, noiseSpinParams[ pGlobal->plot.noiseUnits ].step,
                                    noiseSpinParams[ pGlobal->plot.noiseUnits ].page );

    gchar *sNoiseFrameLabel = g_strdup_printf( "Noise Range (%s)", noiseSpinParams[ pGlobal->plot.noiseUnits ].unit );
    gtk_frame_set_label( wFrmNoiseRange, sNoiseFrameLabel );
    g_free( sNoiseFrameLabel );
}

/*!     \brief  Set the parameters of the spin button for manual range noise plot
 *
 * Set the parameters of the spin button for manual range noise plot
 *
 * \param  pGlobal      pointer to global data
 * \param  noiseUnits   units of the noise plot data
 */
void
setSpinGainRange( tGlobal *pGlobal ) {
    gpointer wSpinGainMin  = WLOOKUP( pGlobal, "spin_GainMin");
    gpointer wSpinGainMax  = WLOOKUP( pGlobal, "spin_GainMax");

    gtk_spin_button_set_range( wSpinGainMin, MIN_GAIN, MAX_GAIN - MIN_GAIN_RANGE );
    gtk_spin_button_set_range( wSpinGainMax, MIN_GAIN + MIN_GAIN_RANGE, MAX_GAIN );
    gtk_spin_button_set_increments( wSpinGainMin, 0.1, 2.0 );
    gtk_spin_button_set_increments( wSpinGainMax, 0.1, 2.0 );
}

void
setFixedRangePlotWidgets( tGlobal *pGlobal ) {
    gpointer wSpinNoiseMin  = WLOOKUP( pGlobal, "spin_NoiseMin");
    gpointer wSpinNoiseMax  = WLOOKUP( pGlobal, "spin_NoiseMax");
    gpointer wSpinGainMin   = WLOOKUP( pGlobal, "spin_GainMin");
    gpointer wSpinGainMax   = WLOOKUP( pGlobal, "spin_GainMax");

    gpointer wChkAuto       = WLOOKUP( pGlobal, "chk_AutoScale");

    setSpinNoiseRange( pGlobal );
    setSpinGainRange( pGlobal );

    gtk_spin_button_set_value( wSpinNoiseMin, pGlobal->fixedGridNoise[ pGlobal->plot.noiseUnits ][ 0 ] );
    gtk_spin_button_set_value( wSpinNoiseMax, pGlobal->fixedGridNoise[ pGlobal->plot.noiseUnits ][ 1 ] );

    gtk_spin_button_set_value( GTK_SPIN_BUTTON( wSpinGainMin ),  pGlobal->fixedGridGain[ 0 ] );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON( wSpinGainMax ),  pGlobal->fixedGridGain[ 1 ] );

    gtk_check_button_set_active( wChkAuto, pGlobal->flags.bAutoScaling );
}

/*!     \brief  Initialize the widgets on the Plot page
 *
 * Initialize the widgets on the External Plot page
 *
 * \param  pGlobal      pointer to global data
 */
void
initializePagePlot( tGlobal *pGlobal ) {
    gpointer wColorTitle = WLOOKUP( pGlobal, "color_Title");
    gpointer wColorGrid  = WLOOKUP( pGlobal, "color_Grid");
    gpointer wColorGridGain  = WLOOKUP( pGlobal, "color_GridGain");
    gpointer wColorNoise = WLOOKUP( pGlobal, "color_Noise");
    gpointer wColorGain  = WLOOKUP( pGlobal, "color_Gain");
    gpointer wColorFreq  = WLOOKUP( pGlobal, "color_Freq");

    gpointer wSpinNoiseMin  = WLOOKUP( pGlobal, "spin_NoiseMin");
    gpointer wSpinNoiseMax  = WLOOKUP( pGlobal, "spin_NoiseMax");
    gpointer wSpinGainMin   = WLOOKUP( pGlobal, "spin_GainMin");
    gpointer wSpinGainMax   = WLOOKUP( pGlobal, "spin_GainMax");

    gpointer wChkAuto       = WLOOKUP( pGlobal, "chk_AutoScale");

    gtk_color_dialog_button_set_rgba( wColorTitle,  &plotElementColors[ eColorTitle ] );
    gtk_color_dialog_button_set_rgba( wColorGrid,  &plotElementColors[ eColorGrid ] );
    gtk_color_dialog_button_set_rgba( wColorGridGain,  &plotElementColors[ eColorGridGain ] );
    gtk_color_dialog_button_set_rgba( wColorNoise,  &plotElementColors[ eColorNoise ] );
    gtk_color_dialog_button_set_rgba( wColorGain,  &plotElementColors[ eColorGain ] );
    gtk_color_dialog_button_set_rgba( wColorFreq,  &plotElementColors[ eColorFrequency ] );

    setFixedRangePlotWidgets( pGlobal );

    g_signal_connect( wSpinNoiseMin, "value-changed", G_CALLBACK( CB_spin_NoiseMin ), NULL);
    g_signal_connect( wSpinNoiseMax, "value-changed", G_CALLBACK( CB_spin_NoiseMax ), NULL);
    g_signal_connect( wSpinGainMin,  "value-changed", G_CALLBACK( CB_spin_GainMin ),  NULL);
    g_signal_connect( wSpinGainMax,  "value-changed", G_CALLBACK( CB_spin_GainMax ),  NULL);

    g_signal_connect ( wColorTitle, "notify::rgba", G_CALLBACK (CB_ColorNotify), GINT_TO_POINTER( eColorTitle ));
    g_signal_connect ( wColorGrid, "notify::rgba", G_CALLBACK (CB_ColorNotify), GINT_TO_POINTER( eColorGrid ));
    g_signal_connect ( wColorGridGain, "notify::rgba", G_CALLBACK (CB_ColorNotify), GINT_TO_POINTER( eColorGridGain ));
    g_signal_connect ( wColorNoise, "notify::rgba", G_CALLBACK (CB_ColorNotify), GINT_TO_POINTER( eColorNoise ));
    g_signal_connect ( wColorGain, "notify::rgba", G_CALLBACK (CB_ColorNotify), GINT_TO_POINTER( eColorGain ));
    g_signal_connect ( wColorFreq, "notify::rgba", G_CALLBACK (CB_ColorNotify), GINT_TO_POINTER( eColorFrequency ));

    g_signal_connect ( WLOOKUP( pGlobal, "btn_ColorReset"), "clicked", G_CALLBACK (CB_ColorReset), NULL );

    g_signal_connect ( wChkAuto, "toggled", G_CALLBACK (CB_chk_AutoScale), NULL );

}
