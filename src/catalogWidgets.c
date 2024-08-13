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


#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib-2.0/glib.h>
#include "HP8970.h"
#include "widgetID.h"


/*!     \brief  Find a widget identified by its name (and add some data to the gobject)
 *
 * Find a widget identified by its name (and add some data to the gobject)
 *
 *
 *
 * \param  pGlobal    : pointer to global data
 * \param  builder    : pointer to GtkBuilder containing compiled widgets
 * \param  sWidgetName: name of widget to find
 * \return pointer to widget found (or zero if not found)
 */
static GtkWidget *
getWidget( tGlobal *pGlobal, GtkBuilder *builder, const gchar *sWidgetName ) {
    GtkWidget *widget = GTK_WIDGET( gtk_builder_get_object ( builder, sWidgetName ) );
    if (GTK_IS_WIDGET(widget)) {
#define WIDGET_ID_PREFIX        "WID_"
#define WIDGET_ID_PREXIX_LEN    (sizeof("WID_")-1)
        gint sequence = (sWidgetName[ WIDGET_ID_PREXIX_LEN] - '1');
        if (sequence < 0 || sequence > 9)
            sequence = INVALID;
        g_object_set_data (G_OBJECT(widget), "sequence", (gpointer) (guint64) sequence);
        g_object_set_data (G_OBJECT(widget), "data", (gpointer) pGlobal);
    }
    return widget;
}

/*!     \brief  Build a random access array of widgets that we need to address
 *
 * Build a random access array of widgets that we need to address. This will be faster to
 * asccess them than if they were in a hash table.
 *
 * \param  pGlobal    : pointer to global data
 * \param  builder    : pointer to GtkBuilder containing compiled widgets
 */
void
buildWidgetList( tGlobal *pGlobal,  GtkBuilder *builder ) {
    pGlobal->widgets[ eW_aspect_Plot ]              = getWidget( pGlobal, builder, "WID_aspect_Plot" );
    pGlobal->widgets[ eW_box_Spot ]                 = getWidget( pGlobal, builder, "WID_box_Spot" );
    pGlobal->widgets[ eW_btn_Calibrate ]            = getWidget( pGlobal, builder, "WID_btn_Calibrate" );
    pGlobal->widgets[ eW_btn_ColorReset ]           = getWidget( pGlobal, builder, "WID_btn_ColorReset" );
    pGlobal->widgets[ eW_btn_CSV ]                  = getWidget( pGlobal, builder, "WID_btn_CSV" );
    pGlobal->widgets[ eW_btn_PDF ]                  = getWidget( pGlobal, builder, "WID_btn_PDF" );
    pGlobal->widgets[ eW_btn_PNG ]                  = getWidget( pGlobal, builder, "WID_btn_PNG" );
    pGlobal->widgets[ eW_btn_Print ]                = getWidget( pGlobal, builder, "WID_btn_Print" );
    pGlobal->widgets[ eW_btn_RestoreJSON ]          = getWidget( pGlobal, builder, "WID_btn_RestoreJSON" );
    pGlobal->widgets[ eW_btn_SaveJSON ]             = getWidget( pGlobal, builder, "WID_btn_SaveJSON" );
    pGlobal->widgets[ eW_btn_SVG ]                  = getWidget( pGlobal, builder, "WID_btn_SVG" );
    pGlobal->widgets[ eW_btn_Sweep ]                = getWidget( pGlobal, builder, "WID_btn_Sweep" );
    pGlobal->widgets[ eW_cbutton_ControlerNameNotIdx ] = getWidget( pGlobal, builder, "WID_cbutton_ControlerNameNotIdx" );
    pGlobal->widgets[ eW_chk_AutoScale ]            = getWidget( pGlobal, builder, "WID_chk_AutoScale" );
    pGlobal->widgets[ eW_chk_Correction ]           = getWidget( pGlobal, builder, "WID_chk_Correction" );
    pGlobal->widgets[ eW_chk_LossOn ]               = getWidget( pGlobal, builder, "WID_chk_LossOn" );
    pGlobal->widgets[ eW_chk_NoiseUnit_F ]          = getWidget( pGlobal, builder, "WID_chk_NoiseUnit_F" );
    pGlobal->widgets[ eW_chk_NoiseUnit_FdB ]        = getWidget( pGlobal, builder, "WID_chk_NoiseUnit_FdB" );
    pGlobal->widgets[ eW_chk_NoiseUnit_TeK ]        = getWidget( pGlobal, builder, "WID_chk_NoiseUnit_TeK" );
    pGlobal->widgets[ eW_chk_NoiseUnit_Y ]          = getWidget( pGlobal, builder, "WID_chk_NoiseUnit_Y" );
    pGlobal->widgets[ eW_chk_NoiseUnit_YdB ]        = getWidget( pGlobal, builder, "WID_chk_NoiseUnit_YdB" );
    pGlobal->widgets[ eW_chk_Settings8970A ]        = getWidget( pGlobal, builder, "WID_chk_Settings8970A" );
    pGlobal->widgets[ eW_chk_Settings8970B ]        = getWidget( pGlobal, builder, "WID_chk_Settings8970B" );
    pGlobal->widgets[ eW_chk_Settings8970Bopt20 ]   = getWidget( pGlobal, builder, "WID_chk_Settings8970Bopt20" );
    pGlobal->widgets[ eW_chk_SettingsHPlogo ]       = getWidget( pGlobal, builder, "WID_chk_SettingsHPlogo" );
    pGlobal->widgets[ eW_chk_SettingsTime ]         = getWidget( pGlobal, builder, "WID_chk_SettingsTime" );
    pGlobal->widgets[ eW_chk_useGPIBdeviceName ]    = getWidget( pGlobal, builder, "WID_chk_useGPIBdeviceName" );
    pGlobal->widgets[ eW_chk_use_LO_GPIBdeviceName ]= getWidget( pGlobal, builder, "WID_chk_use_LO_GPIBdeviceName" );
    pGlobal->widgets[ eW_color_Freq ]               = getWidget( pGlobal, builder, "WID_color_Freq" );
    pGlobal->widgets[ eW_color_Gain ]               = getWidget( pGlobal, builder, "WID_color_Gain" );
    pGlobal->widgets[ eW_color_Grid ]               = getWidget( pGlobal, builder, "WID_color_Grid" );
    pGlobal->widgets[ eW_color_GridGain ]           = getWidget( pGlobal, builder, "WID_color_GridGain" );
    pGlobal->widgets[ eW_color_Noise ]              = getWidget( pGlobal, builder, "WID_color_Noise" );
    pGlobal->widgets[ eW_color_Title ]              = getWidget( pGlobal, builder, "WID_color_Title" );
    pGlobal->widgets[ eW_combo_Mode ]               = getWidget( pGlobal, builder, "WID_combo_Mode" );
    pGlobal->widgets[ eW_combo_Smoothing ]          = getWidget( pGlobal, builder, "WID_combo_Smoothing" );
    pGlobal->widgets[ eW_Controls ]                 = getWidget( pGlobal, builder, "WID_Controls" );
    pGlobal->widgets[ eW_CV_NoiseSource ]           = getWidget( pGlobal, builder, "WID_CV_NoiseSource" );
    pGlobal->widgets[ eW_drawing_Plot ]             = getWidget( pGlobal, builder, "WID_drawing_Plot" );
    pGlobal->widgets[ eW_entry_opt_GPIB_name ]      = getWidget( pGlobal, builder, "WID_entry_opt_GPIB_name" );
    pGlobal->widgets[ eW_entry_opt_LO_GPIB_name ]   = getWidget( pGlobal, builder, "WID_entry_opt_LO_GPIB_name" );
    pGlobal->widgets[ eW_entry_Title ]              = getWidget( pGlobal, builder, "WID_entry_Title" );
    pGlobal->widgets[ eW_frame_NoiseRange ]         = getWidget( pGlobal, builder, "WID_frame_NoiseRange" );
    pGlobal->widgets[ eW_frm_Mode ]                 = getWidget( pGlobal, builder, "WID_frm_Mode" );
    pGlobal->widgets[ eW_frm_Sweep ]                = getWidget( pGlobal, builder, "WID_frm_Sweep" );
    pGlobal->widgets[ eW_HP8970_application ]       = getWidget( pGlobal, builder, "WID_HP8970_application" );
    pGlobal->widgets[ eW_lbl_LOnotice ]             = getWidget( pGlobal, builder, "WID_lbl_LOnotice" );
    pGlobal->widgets[ eW_lbl_version ]              = getWidget( pGlobal, builder, "WID_lbl_version" );
    pGlobal->widgets[ eW_lbl_Status ]               = getWidget( pGlobal, builder, "WID_lbl_Status" );
    pGlobal->widgets[ eW_lbl_Status_LO ]            = getWidget( pGlobal, builder, "WID_lbl_Status_LO" );
    pGlobal->widgets[ eW_LO_combo_sideband ]        = getWidget( pGlobal, builder, "WID_LO_combo_sideband" );
    pGlobal->widgets[ eW_LO_entry_LO_Freq ]         = getWidget( pGlobal, builder, "WID_LO_entry_LO_Freq" );
    pGlobal->widgets[ eW_LO_entry_LO_Setup ]        = getWidget( pGlobal, builder, "WID_LO_entry_LO_Setup" );
    pGlobal->widgets[ eW_LO_frm_FixedIF_Freq ]      = getWidget( pGlobal, builder, "WID_LO_frm_FixedIF_Freq" );
    pGlobal->widgets[ eW_LO_frm_FixedLO_Freq ]      = getWidget( pGlobal, builder, "WID_LO_frm_FixedLO_Freq" );
    pGlobal->widgets[ eW_LO_frm_sideband ]          = getWidget( pGlobal, builder, "WID_LO_frm_sideband" );
    pGlobal->widgets[ eW_LO_spin_FixedIF_Freq ]     = getWidget( pGlobal, builder, "WID_LO_spin_FixedIF_Freq" );
    pGlobal->widgets[ eW_LO_spin_FixedLO_Freq ]     = getWidget( pGlobal, builder, "WID_LO_spin_FixedLO_Freq" );
    pGlobal->widgets[ eW_LO_spin_SettlingTime ]     = getWidget( pGlobal, builder, "WID_LO_spin_SettlingTime" );
    pGlobal->widgets[ eW_note_Controls ]            = getWidget( pGlobal, builder, "WID_note_Controls" );
    pGlobal->widgets[ eW_NS_btn_Add ]               = getWidget( pGlobal, builder, "WID_NS_btn_Add" );
    pGlobal->widgets[ eW_NS_btn_Delete ]            = getWidget( pGlobal, builder, "WID_NS_btn_Delete" );
    pGlobal->widgets[ eW_NS_btn_Save ]              = getWidget( pGlobal, builder, "WID_NS_btn_Save" );
    pGlobal->widgets[ eW_NS_btn_Upload ]            = getWidget( pGlobal, builder, "WID_NS_btn_Upload" );
    pGlobal->widgets[ eW_NS_combo_Source ]          = getWidget( pGlobal, builder, "WID_NS_combo_Source" );
    pGlobal->widgets[ eW_page_GPIB ]                = getWidget( pGlobal, builder, "WID_page_GPIB" );
    pGlobal->widgets[ eW_page_Notes ]               = getWidget( pGlobal, builder, "WID_page_Notes" );
    pGlobal->widgets[ eW_page_Options ]             = getWidget( pGlobal, builder, "WID_page_Options" );
    pGlobal->widgets[ eW_page_Plot ]                = getWidget( pGlobal, builder, "WID_page_Plot" );
    pGlobal->widgets[ eW_page_Settings ]            = getWidget( pGlobal, builder, "WID_page_Settings" );
    pGlobal->widgets[ eW_page_SigGen ]              = getWidget( pGlobal, builder, "WID_page_SigGen" );
    pGlobal->widgets[ eW_page_Source ]              = getWidget( pGlobal, builder, "WID_page_Source" );
    pGlobal->widgets[ eW_scroll_NoiseSource ]       = getWidget( pGlobal, builder, "WID_scroll_NoiseSource" );
    pGlobal->widgets[ eW_chk_SettingsA4 ]           = getWidget( pGlobal, builder, "WID_chk_SettingsA4" );
    pGlobal->widgets[ eW_chk_SettingsLetter ]       = getWidget( pGlobal, builder, "WID_chk_SettingsLetter" );
    pGlobal->widgets[ eW_chk_SettingsA3 ]           = getWidget( pGlobal, builder, "WID_chk_SettingsA3" );
    pGlobal->widgets[ eW_chk_SettingsTabloid ]      = getWidget( pGlobal, builder, "WID_chk_SettingsTabloid" );
    pGlobal->widgets[ eW_spin_ColdT ]               = getWidget( pGlobal, builder, "WID_spin_ColdT" );
    pGlobal->widgets[ eW_spin_Frequency ]           = getWidget( pGlobal, builder, "WID_spin_Frequency" );
    pGlobal->widgets[ eW_spin_FrStart ]             = getWidget( pGlobal, builder, "WID_spin_FrStart" );
    pGlobal->widgets[ eW_spin_FrStep_Cal ]          = getWidget( pGlobal, builder, "WID_spin_FrStep_Cal" );
    pGlobal->widgets[ eW_spin_FrStep_Sweep ]        = getWidget( pGlobal, builder, "WID_spin_FrStep_Sweep" );
    pGlobal->widgets[ eW_spin_FrStop ]              = getWidget( pGlobal, builder, "WID_spin_FrStop" );
    pGlobal->widgets[ eW_spin_GainMax ]             = getWidget( pGlobal, builder, "WID_spin_GainMax" );
    pGlobal->widgets[ eW_spin_GainMin ]             = getWidget( pGlobal, builder, "WID_spin_GainMin" );
    pGlobal->widgets[ eW_spin_LossAfter ]           = getWidget( pGlobal, builder, "WID_spin_LossAfter" );
    pGlobal->widgets[ eW_spin_LossBefore ]          = getWidget( pGlobal, builder, "WID_spin_LossBefore" );
    pGlobal->widgets[ eW_spin_LossT ]               = getWidget( pGlobal, builder, "WID_spin_LossT" );
    pGlobal->widgets[ eW_spin_NoiseMax ]            = getWidget( pGlobal, builder, "WID_spin_NoiseMax" );
    pGlobal->widgets[ eW_spin_NoiseMin ]            = getWidget( pGlobal, builder, "WID_spin_NoiseMin" );
    pGlobal->widgets[ eW_spin_opt_ControllerIdx ]   = getWidget( pGlobal, builder, "WID_spin_opt_ControllerIdx" );
    pGlobal->widgets[ eW_spin_opt_GPIB_PID ]        = getWidget( pGlobal, builder, "WID_spin_opt_GPIB_PID" );
    pGlobal->widgets[ eW_spin_opt_GPIB_PID_LO ]     = getWidget( pGlobal, builder, "WID_spin_opt_GPIB_PID_LO" );
    pGlobal->widgets[ eW_Splash ]                   = getWidget( pGlobal, builder, "WID_Splash" );
    pGlobal->widgets[ eW_textView_Notes ]           = getWidget( pGlobal, builder, "WID_textView_Notes" );
    pGlobal->widgets[ eW_tgl_Spot ]                 = getWidget( pGlobal, builder, "WID_tgl_Spot" );
}
