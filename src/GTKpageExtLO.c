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

/*! \file GTKpageGPIB.c
 *  \brief Handle GUI elements on the GtkNotebook page for GPIB
 *
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

void warnFrequencyRangeOutOfBounds( tGlobal *pGlobal );

/*!     \brief  Callback External LO page - LO Setup string
 *
 * Callback External LO page - LO Setup string
 *
 * \param  wDeviceName  pointer to GtkEditBuffer of the GtkEntry widget
 * \param  udata        user data
 */
static void
CB_edit_LO_Setup ( GtkEditable *wDeviceName, gpointer udata )
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(gtk_widget_get_root(GTK_WIDGET(wDeviceName))), "data");
    const gchar *sDeviceName = gtk_editable_get_text( wDeviceName );

    g_free( pGlobal->HP8970settings.sExtLOsetup );
    pGlobal->HP8970settings.sExtLOsetup = g_strdup( sDeviceName );
}

/*!     \brief  Callback External LO page - LO set frequency string
 *
 * Callback External LO page - LO set frequency string
 *
 * \param  wDeviceName  pointer to GtkEditBuffer of the GtkEntry widget
 * \param  udata        user data
 */
static void
CB_edit_LO_Freq ( GtkEditable *wLOfreqCmd, gpointer udata )
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(gtk_widget_get_root(GTK_WIDGET(wLOfreqCmd))), "data");
    const gchar *sLOfreqCmd = gtk_editable_get_text( wLOfreqCmd );

    g_free( pGlobal->HP8970settings.sExtLOsetFreq );
    pGlobal->HP8970settings.sExtLOsetFreq = g_strdup( sLOfreqCmd );

    pGlobal->flags.bNoLOcontrol = (sLOfreqCmd == NULL || strlen( sLOfreqCmd ) == 0);
}

/*!     \brief  Callback External LO page - IF frequency
 *
 * Callback External LO page - IF frequency
 *
 * \param  wLO_IFfreq   pointer to GtkSpinButton
 * \param  udata        user data
 */
void
CB_spin_IFfreq(   GtkSpinButton* wLO_IFfreq, gpointer udata ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wLO_IFfreq), "data");

    pGlobal->HP8970settings.extLOfreqIF = gtk_spin_button_get_value( wLO_IFfreq );

    UPDATE_8970_SETTING( pGlobal, pGlobal->HP8970settings.updateFlags.each.bExternalLO );

    warnFrequencyRangeOutOfBounds( pGlobal );
}

/*!     \brief  Callback External LO page - LO frequency
 *
 * Callback External LO page - LO frequency
 *
 * \param  wLO_LOfreq   pointer to GtkSpinButton
 * \param  udata        user data
 */
void
CB_spin_LOfreq(   GtkSpinButton* wLO_LOfreq, gpointer udata ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wLO_LOfreq), "data");

    pGlobal->HP8970settings.extLOfreqLO = gtk_spin_button_get_value( wLO_LOfreq );

    UPDATE_8970_SETTING( pGlobal, pGlobal->HP8970settings.updateFlags.each.bExternalLO);

    warnFrequencyRangeOutOfBounds( pGlobal );
}

/*!     \brief  Callback External LO page - Settling Time
 *
 * Callback External LO page - Settling Time
 *
 * \param  wLO_SettlingTime   pointer to GtkSpinButton
 * \param  udata        user data
 */
static void
CB_spin_SettlingTime(   GtkSpinButton* wLO_SettlingTime, gpointer udata ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wLO_SettlingTime), "data");

    pGlobal->HP8970settings.settlingTime_ms = gtk_spin_button_get_value( wLO_SettlingTime );
}

/*!     \brief  Callback External LO page - Sideband
 *
 * Callback External LO page - Sideband
 *
 * \param  wComboSideband  pointer to GtkComboBoxText
 * \param  udata       user data
 */
static void
CB_combo_Sideband ( GtkComboBox* wComboSideband, gpointer udata ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wComboSideband), "data");
    const gchar *sActiveID;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    // The IDs are strings "0", "1" or "2" - convert to number (enum)
    if( (sActiveID = gtk_combo_box_get_active_id ( GTK_COMBO_BOX( wComboSideband ) )) )
        pGlobal->HP8970settings.extLOsideband = (tSideband)(sActiveID[0] - '0') ;
    else
        pGlobal->HP8970settings.extLOsideband = eDSB;
#pragma GCC diagnostic pop

    UPDATE_8970_SETTING( pGlobal, pGlobal->HP8970settings.updateFlags.each.bExternalLO);

    warnFrequencyRangeOutOfBounds( pGlobal );
}

/*!     \brief  Some settings on the LO page are mode specific
 *
 * Some settings on the LO page are mode specific, so reduce confusion
 * by disabling the ones that don't apply to the mode
 *
 * \param  pGlobal   pointer to global data
 * \param  mode      mode of NF meter
 */
void
enablePageExtLOwidgets( tGlobal *pGlobal, tMode mode ) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    gpointer wIFfreq = WLOOKUP( pGlobal, "LO_frm_FixedIF_Freq");
    gpointer wLOfreq = WLOOKUP( pGlobal, "LO_frm_FixedLO_Freq");
    gpointer wLOsideband = WLOOKUP( pGlobal, "LO_frm_sideband");

    gpointer wSideband = WLOOKUP( pGlobal, "LO_combo_sideband");
    GtkTreeIter iter;
    GtkTreeModel *sidebandListStore = gtk_combo_box_get_model (wSideband);
    gboolean bValid;
    gchar *sID;

    gboolean bIFfreqEn = TRUE, bLOfreqEn = TRUE, bLOsidebandEn = TRUE;;
    switch( mode ) {
        case eMode1_0:
        default:
            bIFfreqEn = FALSE; bLOfreqEn = FALSE;
            bLOsidebandEn = FALSE;
            break;
        case eMode1_1:
        case eMode1_3:
            bIFfreqEn = TRUE; bLOfreqEn = FALSE;
            bLOsidebandEn = TRUE;
            break;
        case eMode1_2:
        case eMode1_4:
            bIFfreqEn = FALSE; bLOfreqEn = TRUE;
            bLOsidebandEn = TRUE;
            break;
    }

    bValid = gtk_tree_model_get_iter_first (sidebandListStore, &iter);
    if (mode == eMode1_2 && bValid ) {
       gtk_tree_model_get (sidebandListStore, &iter, 1, &sID, -1);
       if (g_strcmp0 (sID, "0") == 0)
           gtk_combo_box_text_remove (wSideband, 0);
       g_free (sID);
       if( pGlobal->HP8970settings.extLOsideband == eDSB ) {
            pGlobal->HP8970settings.extLOsideband = eLSB;
            gtk_combo_box_set_active( GTK_COMBO_BOX(wSideband), 0 );
            UPDATE_8970_SETTING( pGlobal, pGlobal->HP8970settings.updateFlags.each.bExternalLO );
       }
    } else if (bValid){
        gtk_tree_model_get (sidebandListStore, &iter, 1, &sID, -1);
        if (g_strcmp0 (sID, "0") != 0)
            gtk_combo_box_text_insert( wSideband, 0, "0", "DSB (no offset)" );
        g_free (sID);
    }

    gtk_widget_set_sensitive( wIFfreq, bIFfreqEn );
    gtk_widget_set_sensitive( wLOfreq, bLOfreqEn );
    gtk_widget_set_sensitive( wLOsideband, bLOsidebandEn );
#pragma GCC diagnostic pop
}

gboolean
isMode1_2_FrequencyValid( gboolean freqMHz, tGlobal *pGlobal ) {
    gdouble LO, freqDwnConverted;
    gdouble max8970freq = maxInputFreq[ pGlobal->flags.bbHP8970Bmodel ];
    tSideband sideband = pGlobal->HP8970settings.extLOsideband;
    gboolean bValid = TRUE;

    LO = pGlobal->HP8970settings.extLOfreqLO;

    switch( sideband ) {
        case eLSB:
        default:
            freqDwnConverted = LO - freqMHz;
            break;
        case eUSB:
            freqDwnConverted = freqMHz - LO;
            break;
    }

    // LSB: LO must be +10MHz above Fstop
    if( freqDwnConverted < 10.0 || freqDwnConverted > max8970freq )
        bValid = FALSE;

    return bValid;
}
/*!     \brief  Show information on the validity of specified frequencies
 *
 * Some combinations of mode, LO and IF frequencies and sweep frequencies are invalid
 * or will cause image issues. Notify the user of the problems with such choices
 *
 * \param  pGlobal   pointer to global data
 */
void
warnFrequencyRangeOutOfBounds( tGlobal *pGlobal ) {
    gpointer wNotice = WLOOKUP( pGlobal, "lbl_LOnotice");
    gdouble lowF, highF, startF, stopF, spotF;
    gdouble downcvtStartF, downcvtStopF, downcvtSpotF;
    gdouble mode1_4_StartF = 0.0, mode1_4_StopF = 0.0;
    gdouble LO, IF, minF, maxF, imageStartF = 0.0, imageStopF = 0.0;
    gdouble max8970freq = maxInputFreq[ pGlobal->flags.bbHP8970Bmodel ];
    gboolean bExtLO = !(pGlobal->HP8970settings.mode == eMode1_0 || pGlobal->HP8970settings.mode == eMode1_4);
    tSideband sideband = pGlobal->HP8970settings.extLOsideband;

    startF = pGlobal->HP8970settings.range[ bExtLO ].freqStartMHz;
    stopF  = pGlobal->HP8970settings.range[ bExtLO ].freqStopMHz;
    spotF  = pGlobal->HP8970settings.range[ bExtLO ].freqSpotMHz;
    LO = pGlobal->HP8970settings.extLOfreqLO;
    IF = pGlobal->HP8970settings.extLOfreqIF;

    gchar *sWarning = NULL;

    gtk_widget_remove_css_class( wNotice, "info" );
    gtk_widget_remove_css_class( wNotice, "warning" );

    switch( pGlobal->HP8970settings.mode ) {
        case eMode1_1:
        case eMode1_3:
            switch( sideband ) {
                case eDSB:
                default:
                    lowF = startF;
                    highF = stopF;
                    break;
                case eLSB:
                    lowF = startF + IF;
                    highF = stopF + IF;
                    imageStartF = startF + (2 * IF);
                    imageStopF = stopF + (2 * IF);
                    break;
                case eUSB:
                    lowF = startF - IF;
                    highF = stopF - IF;
                    imageStartF = startF - (2 * IF);
                    imageStopF = stopF - (2 * IF);
                    break;
            }
            gtk_widget_add_css_class( wNotice, "info" );

            if( sideband != eDSB )
                sWarning = g_strdup_printf( "🛈\tL.O. sweep:\t%g MHz ➡ %g MHz\n"
                        "\tImage:\t\t%g MHz ➡ %g MHz ", lowF, highF, imageStartF, imageStopF);
            else
                sWarning = g_strdup_printf( "🛈\tL.O. sweep:\t%g MHz ➡ %g MHz\n", lowF, highF);
            break;
        case eMode1_2:
            if( sideband == eLSB ) {
                downcvtStartF = LO - startF;
                downcvtStopF  = LO - stopF;
                downcvtSpotF  = LO - spotF;
                maxF = LO - 10.0;
                minF = LO - max8970freq;
                imageStartF = (2 * LO) - startF;
                imageStopF  = (2 * LO) - stopF;
            } else {
                downcvtStartF = startF - LO;
                downcvtStopF  = stopF - LO;
                downcvtSpotF  = spotF - LO;
                maxF = LO + max8970freq;
                minF = LO + 10.0;
                imageStartF = (2 * LO) - startF;
                imageStopF  = (2 * LO) - stopF;
            }
            // LSB: LO must be +10MHz above Fstop
            if( downcvtStartF < 10.0 || downcvtStartF > max8970freq || downcvtStopF < 10.0 || downcvtStopF > max8970freq
                    || downcvtSpotF < 10.0 || downcvtSpotF > max8970freq ) {
                gtk_widget_add_css_class( wNotice, "warning" );
                sWarning = g_strdup_printf( "⚠️\tDownconversion to %g MHz ➡ %g MHz\n\tis beyond the range of the %s\n"
                        "👉\tFstart, Fstop and Fspot must be\n\t\t\t> %g MHz and < %g MHz",
                        downcvtStartF, downcvtStopF, sHP89709models[pGlobal->flags.bbHP8970Bmodel], minF, maxF);
            } else {
                gtk_widget_add_css_class( wNotice, "info" );
                sWarning = g_strdup_printf( "🛈\tDownconversion to:\t%g MHz ➡ %g MHz\n"
                        "\tImage:\t\t\t\t%g MHz ➡ %g MHz ", downcvtStartF, downcvtStopF, imageStartF, imageStopF);
            }
            break;
        case eMode1_4:
            switch( sideband ) {
            case eDSB:
            default:
                break;
            case eLSB:
                mode1_4_StartF = LO - startF;
                mode1_4_StopF  = LO - stopF;

                imageStartF = (2 * LO) - mode1_4_StartF;
                imageStopF  = (2 * LO) - mode1_4_StopF;
                break;
            case eUSB:
                mode1_4_StartF = LO + startF;
                mode1_4_StopF  = LO + stopF;

                imageStartF = (2 * LO) - mode1_4_StartF;
                imageStopF  = (2 * LO) - mode1_4_StopF;
                break;
            }
            gtk_widget_add_css_class( wNotice, "info" );
            if( sideband == eDSB )
                sWarning = g_strdup_printf( "🛈\tInput to converter:\n\t\t\t%g ± %g MHz ➡ %g ± %g MHz\n",
                                            LO, startF, LO, stopF);
            else
                sWarning = g_strdup_printf( "🛈\tInput to converter:\t%g MHz ➡ %g MHz\n"
                        "\tImage:\t\t\t\t%g MHz ➡ %g MHz ", mode1_4_StartF, mode1_4_StopF, imageStartF, imageStopF);

            break;
        default:
            sWarning = g_strdup( "" );
            break;
    }

    gtk_label_set_text( wNotice, sWarning );
    g_free( sWarning );

    GtkWidget *wFrStart   = WLOOKUP( pGlobal, "spin_FrStart" );
    GtkWidget *wFrSpot    = WLOOKUP( pGlobal, "spin_Frequency" );
    GtkWidget *wFrStop    = WLOOKUP( pGlobal, "spin_FrStop" );

    if( pGlobal->HP8970settings.mode == eMode1_2 ) {
        if( isMode1_2_FrequencyValid( pGlobal->HP8970settings.range[ bExtLO ].freqStartMHz, pGlobal ) )
            gtk_widget_remove_css_class( GTK_WIDGET( wFrStart ), "warning" );
        else
            gtk_widget_add_css_class( GTK_WIDGET( wFrStart ), "warning" );

        if( isMode1_2_FrequencyValid( pGlobal->HP8970settings.range[ bExtLO ].freqStopMHz, pGlobal ) )
            gtk_widget_remove_css_class( GTK_WIDGET( wFrStop ), "warning" );
        else
            gtk_widget_add_css_class( GTK_WIDGET( wFrStop ), "warning" );

        if( isMode1_2_FrequencyValid( pGlobal->HP8970settings.range[ bExtLO ].freqSpotMHz, pGlobal ) )
            gtk_widget_remove_css_class( GTK_WIDGET( wFrSpot ), "warning" );
        else
            gtk_widget_add_css_class( GTK_WIDGET( wFrSpot ), "warning" );
    } else {
        gtk_widget_remove_css_class( GTK_WIDGET( wFrStart ), "warning" );
        gtk_widget_remove_css_class( GTK_WIDGET( wFrStop ), "warning" );
        gtk_widget_remove_css_class( GTK_WIDGET( wFrSpot ), "warning" );
    }
}

/*!     \brief  Set the value of widgets on the External L.O. page
 *
 * Set the value of widgets on the External L.O. page
 *
 * \param  pGlobal      pointer to global data
 */
void
setPageExtLOwidgets( tGlobal *pGlobal ) {
    gpointer wLO_Setup = WLOOKUP( pGlobal, "LO_entry_LO_Setup");
    gpointer wLO_Freq  = WLOOKUP( pGlobal, "LO_entry_LO_Freq");
    gpointer wIFfreq = WLOOKUP( pGlobal, "LO_spin_FixedIF_Freq");
    gpointer wLOfreq = WLOOKUP( pGlobal, "LO_spin_FixedLO_Freq");
    gpointer wSettlingTime = WLOOKUP( pGlobal, "LO_spin_SettlingTime");
    gpointer wSideband = WLOOKUP( pGlobal, "LO_combo_sideband");

    g_signal_handlers_block_by_func( G_OBJECT( wLO_Setup ), G_CALLBACK( CB_edit_LO_Setup ), NULL );
    g_signal_handlers_block_by_func( G_OBJECT( wLO_Freq ), G_CALLBACK( CB_edit_LO_Freq ), NULL );
    gtk_editable_set_text( wLO_Setup, pGlobal->HP8970settings.sExtLOsetup );
    gtk_editable_set_text( wLO_Freq, pGlobal->HP8970settings.sExtLOsetFreq );

    pGlobal->flags.bNoLOcontrol = (pGlobal->HP8970settings.sExtLOsetFreq == NULL
                            || strlen( pGlobal->HP8970settings.sExtLOsetFreq ) == 0);

    g_signal_handlers_unblock_by_func( G_OBJECT( wLO_Setup ), G_CALLBACK( CB_edit_LO_Setup ), NULL );
    g_signal_handlers_unblock_by_func( G_OBJECT( wLO_Freq ),  G_CALLBACK( CB_edit_LO_Freq ), NULL );
    gtk_spin_button_set_value( wIFfreq, pGlobal->HP8970settings.extLOfreqIF );

    gtk_spin_button_set_value( wLOfreq, pGlobal->HP8970settings.extLOfreqLO );
    gtk_spin_button_set_value( wSettlingTime, pGlobal->HP8970settings.settlingTime_ms );

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    gtk_combo_box_set_active( wSideband, pGlobal->HP8970settings.extLOsideband - (pGlobal->HP8970settings.mode == eMode1_2 ? 1 : 0) );
#pragma GCC diagnostic pop
}

/*!     \brief  Initialize the widgets on the External L.O. page
 *
 * Initialize the widgets on the External L.O. page
 *
 * \param  pGlobal      pointer to global data
 */
void
initializePageExtLO( tGlobal *pGlobal ) {
    gpointer wLO_Setup = WLOOKUP( pGlobal, "LO_entry_LO_Setup");
    gpointer wLO_Freq  = WLOOKUP( pGlobal, "LO_entry_LO_Freq");
    gpointer wIFfreq = WLOOKUP( pGlobal, "LO_spin_FixedIF_Freq");
    gpointer wLOfreq = WLOOKUP( pGlobal, "LO_spin_FixedLO_Freq");
    gpointer wSettlingTime = WLOOKUP( pGlobal, "LO_spin_SettlingTime");
    gpointer wSideband = WLOOKUP( pGlobal, "LO_combo_sideband");

    setPageExtLOwidgets( pGlobal );

    gtk_spin_button_set_range( wIFfreq, 10.0, maxInputFreq[ pGlobal->flags.bbHP8970Bmodel ] );

    g_signal_connect( wLO_Setup, "changed", G_CALLBACK( CB_edit_LO_Setup ), NULL );
    g_signal_connect( wLO_Freq, "changed", G_CALLBACK( CB_edit_LO_Freq ), NULL );

    g_signal_connect( wIFfreq, "value-changed", G_CALLBACK( CB_spin_IFfreq ), NULL);
    g_signal_connect( wLOfreq, "value-changed", G_CALLBACK( CB_spin_LOfreq ), NULL);
    g_signal_connect( wSettlingTime, "value-changed", G_CALLBACK( CB_spin_SettlingTime ), NULL);

    g_signal_connect( wSideband, "changed", G_CALLBACK( CB_combo_Sideband ), NULL);
}
