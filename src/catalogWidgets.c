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
    const static gchar *sWidgetNames[ eW_N_WIDGETS ] = {
            [ eW_aspect_Plot ]                  = "WID_aspect_Plot",
            [ eW_box_Spot ]                     = "WID_box_Spot",
            [ eW_btn_Calibrate ]                = "WID_btn_Calibrate",
            [ eW_btn_ColorReset ]               = "WID_btn_ColorReset",
            [ eW_btn_CSV ]                      = "WID_btn_CSV",
            [ eW_btn_Memory ]                   = "WID_btn_Memory",
            [ eW_btn_PDF ]                      = "WID_btn_PDF",
            [ eW_btn_PNG ]                      = "WID_btn_PNG",
            [ eW_btn_Print ]                    = "WID_btn_Print",
            [ eW_btn_RestoreJSON ]              = "WID_btn_RestoreJSON",
            [ eW_btn_SaveJSON ]                 = "WID_btn_SaveJSON",
            [ eW_btn_SettingsDelete ]           = "WID_btn_SettingsDelete",
            [ eW_btn_SettingsRestore ]          = "WID_btn_SettingsRestore",
            [ eW_btn_SettingsSave ]             = "WID_btn_SettingsSave",
            [ eW_btn_SVG ]                      = "WID_btn_SVG",
            [ eW_chk_AutoScale ]                = "WID_chk_AutoScale",
            [ eW_chk_Correction ]               = "WID_chk_Correction",
            [ eW_chk_LossOn ]                   = "WID_chk_LossOn",
            [ eW_chk_Settings8970A ]            = "WID_chk_Settings8970A",
            [ eW_chk_Settings8970B ]            = "WID_chk_Settings8970B",
            [ eW_chk_Settings8970Bopt20 ]       = "WID_chk_Settings8970Bopt20",
            [ eW_chk_SettingsHPlogo ]           = "WID_chk_SettingsHPlogo",
            [ eW_chk_SettingsTime ]             = "WID_chk_SettingsTime",
            [ eW_chk_ShowMemory]                = "WID_chk_ShowMemory",
            [ eW_chk_useGPIBdeviceName ]        = "WID_chk_useGPIBdeviceName",
            [ eW_chk_use_LO_GPIBdeviceName ]    = "WID_chk_use_LO_GPIBdeviceName",
            [ eW_color_Freq ]                   = "WID_color_Freq",
            [ eW_color_Gain ]                   = "WID_color_Gain",
            [ eW_color_Grid ]                   = "WID_color_Grid",
            [ eW_color_GridGain ]               = "WID_color_GridGain",
            [ eW_color_Noise ]                  = "WID_color_Noise",
            [ eW_color_Title ]                  = "WID_color_Title",
            [ eW_combo_Mode ]                   = "WID_combo_Mode",
            [ eW_combo_SettingsConfigurations ] = "WID_combo_SettingsConfigurations",
            [ eW_Controls ]                     = "WID_Controls",
            [ eW_CV_NoiseSource ]               = "WID_CV_NoiseSource",
            [ eW_drawing_Plot ]                 = "WID_drawing_Plot",
            [ eW_drop_IF_Attenuation ]          = "WID_drop_IF_Attenuation",
            [ eW_drop_InputGainCalibration ]    = "WID_drop_InputGainCalibration",
            [ eW_drop_NoiseUnits ]              = "WID_drop_NoiseUnits",
            [ eW_drop_RF_Attenuation ]           = "WID_drop_RF_Attenuation",
            [ eW_drop_Smoothing ]               = "WID_drop_Smoothing",
            [ eW_entry_opt_GPIB_name ]          = "WID_entry_opt_GPIB_name",
            [ eW_entry_opt_LO_GPIB_name ]       = "WID_entry_opt_LO_GPIB_name",
            [ eW_entry_Title ]                  = "WID_entry_Title",
            [ eW_frm_IF_Attenuation ]           = "WID_frm_IF_Attenuation",
            [ eW_frm_InputGainCal ]             = "WID_frm_InputGainCal",
            [ eW_frame_NoiseRange ]             = "WID_frame_NoiseRange",
            [ eW_frm_Mode ]                     = "WID_frm_Mode",
            [ eW_frm_RF_Attenuation ]           = "WID_frm_RF_Attenuation",
            [ eW_frm_Sweep ]                    = "WID_frm_Sweep",
            [ eW_HP8970_application ]           = "WID_HP8970_application",
            [ eW_lbl_LOnotice ]                 = "WID_lbl_LOnotice",
            [ eW_lbl_version ]                  = "WID_lbl_version",
            [ eW_lbl_Status ]                   = "WID_lbl_Status",
            [ eW_lbl_Status_LO ]                = "WID_lbl_Status_LO",
            [ eW_LO_combo_sideband ]            = "WID_LO_combo_sideband",
            [ eW_LO_entry_LO_Freq ]             = "WID_LO_entry_LO_Freq",
            [ eW_LO_entry_LO_Setup ]            = "WID_LO_entry_LO_Setup",
            [ eW_LO_frm_FixedIF_Freq ]          = "WID_LO_frm_FixedIF_Freq",
            [ eW_LO_frm_FixedLO_Freq ]          = "WID_LO_frm_FixedLO_Freq",
            [ eW_LO_frm_sideband ]              = "WID_LO_frm_sideband",
            [ eW_LO_spin_FixedIF_Freq ]         = "WID_LO_spin_FixedIF_Freq",
            [ eW_LO_spin_FixedLO_Freq ]         = "WID_LO_spin_FixedLO_Freq",
            [ eW_LO_spin_SettlingTime ]         = "WID_LO_spin_SettlingTime",
            [ eW_note_Controls ]                = "WID_note_Controls",
            [ eW_NS_btn_Add ]                   = "WID_NS_btn_Add",
            [ eW_NS_btn_Delete ]                = "WID_NS_btn_Delete",
            [ eW_NS_btn_Save ]                  = "WID_NS_btn_Save",
            [ eW_NS_btn_Upload ]                = "WID_NS_btn_Upload",
            [ eW_NS_combo_Source ]              = "WID_NS_combo_Source",
            [ eW_page_GPIB ]                    = "WID_page_GPIB",
            [ eW_page_Notes ]                   = "WID_page_Notes",
            [ eW_page_Options ]                 = "WID_page_Options",
            [ eW_page_Plot ]                    = "WID_page_Plot",
            [ eW_page_Settings ]                = "WID_page_Settings",
            [ eW_page_SigGen ]                  = "WID_page_SigGen",
            [ eW_page_Source ]                  = "WID_page_Source",
            [ eW_scroll_NoiseSource ]           = "WID_scroll_NoiseSource",
            [ eW_chk_SettingsA4 ]               = "WID_chk_SettingsA4",
            [ eW_chk_SettingsLetter ]           = "WID_chk_SettingsLetter",
            [ eW_chk_SettingsA3 ]               = "WID_chk_SettingsA3",
            [ eW_chk_SettingsTabloid ]          = "WID_chk_SettingsTabloid",
            [ eW_spin_ColdT ]                   = "WID_spin_ColdT",
            [ eW_spin_Frequency ]               = "WID_spin_Frequency",
            [ eW_spin_FrStart ]                 = "WID_spin_FrStart",
            [ eW_spin_FrStep_Cal ]              = "WID_spin_FrStep_Cal",
            [ eW_spin_FrStep_Sweep ]            = "WID_spin_FrStep_Sweep",
            [ eW_spin_FrStop ]                  = "WID_spin_FrStop",
            [ eW_spin_GainMax ]                 = "WID_spin_GainMax",
            [ eW_spin_GainMin ]                 = "WID_spin_GainMin",
            [ eW_spin_LossAfter ]               = "WID_spin_LossAfter",
            [ eW_spin_LossBefore ]              = "WID_spin_LossBefore",
            [ eW_spin_LossT ]                   = "WID_spin_LossT",
            [ eW_spin_NoiseMax ]                = "WID_spin_NoiseMax",
            [ eW_spin_NoiseMin ]                = "WID_spin_NoiseMin",
            [ eW_spin_opt_ControllerIdx ]       = "WID_spin_opt_ControllerIdx",
            [ eW_spin_opt_GPIB_PID ]            = "WID_spin_opt_GPIB_PID",
            [ eW_spin_opt_GPIB_PID_LO ]         = "WID_spin_opt_GPIB_PID_LO",
            [ eW_Splash ]                       = "WID_Splash",
            [ eW_textView_Notes ]               = "WID_textView_Notes",
            [ eW_tgl_Spot ]                     = "WID_tgl_Spot",
            [ eW_tgl_Sweep ]                    = "WID_tgl_Sweep"
    };

    for( int i=0; i < eW_N_WIDGETS; i++ )
        pGlobal->widgets[ i ] = getWidget( pGlobal, builder, sWidgetNames[ i ] );
}
