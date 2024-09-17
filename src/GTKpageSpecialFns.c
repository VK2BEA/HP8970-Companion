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

/*! \file GTKpageHP8970.c
 *  \brief Handle GUI elements on the GtkNotebook page for HP8970
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

/*!     \brief  Callback for selection of noise units
 *
 * Callback for selection of noise units
 *
 * \param  wNoiseUnit     pointer to GtkDropDown
 * \param  udata          user data
 */
static void
CB_drop_NoiseUnits( GtkDropDown * wNoiseUnits, gpointer udata ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wNoiseUnits ), "data");

    pGlobal->HP8970settings.noiseUnits = gtk_drop_down_get_selected( wNoiseUnits );
    UPDATE_8970_SETTING( pGlobal, pGlobal->HP8970settings.updateFlags.each.bNoiseUnits);
}


/*!     \brief  Callback for changing the input gain for calibration
 *
 * Callback for changing the input gain for calibration
 *
 * \param  wInputGainCal     pointer to GtkDropDown
 * \param  udata             user data
 */
static void
CB_drop_InputGainCalibration( GtkDropDown * wInputGainCal, gpointer udata ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wInputGainCal ), "data");

    pGlobal->HP8970settings.inputGainCal = gtk_drop_down_get_selected( wInputGainCal );
}


/*!     \brief  Callback for Cold Temperature spin button
 *
 * Callback for Cold Temperature spin button
 *
 * \param  wSpinColdT     pointer to GtkSpinButton
 * \param  udata          user data
 */
void
CB_spin_ColdTemp(  GtkSpinButton* wSpinColdT, gpointer udata ) {

    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wSpinColdT), "data");

    pGlobal->HP8970settings.coldTemp = gtk_spin_button_get_value( wSpinColdT );

    UPDATE_8970_SETTING( pGlobal, pGlobal->HP8970settings.updateFlags.each.bLossCompenstaion);
}

/*!     \brief  Callback for Cold Temperature spin button
 *
 * Callback for Cold Temperature spin button
 *
 * \param  wSpinLossT     pointer to GtkSpinButton
 * \param  udata          user data
 */
void
CB_spin_LossTemp(  GtkSpinButton* wSpinLossT, gpointer udata ) {

    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wSpinLossT), "data");

    pGlobal->HP8970settings.lossTemp = gtk_spin_button_get_value( wSpinLossT );

    UPDATE_8970_SETTING( pGlobal, pGlobal->HP8970settings.updateFlags.each.bLossCompenstaion);
}

/*!     \brief  Callback for loss before DUT spin button
 *
 * Callback for loss before DUT spin button
 *
 * \param  wSpinLossBeforeDUT pointer to GtkSpinButton
 * \param  udata              user data
 */
void
CB_spin_LossBeforeDUT(  GtkSpinButton* wSpinLossBeforeDUT, gpointer udata ) {

    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wSpinLossBeforeDUT), "data");

    pGlobal->HP8970settings.lossBeforeDUT = gtk_spin_button_get_value( wSpinLossBeforeDUT );

    UPDATE_8970_SETTING( pGlobal, pGlobal->HP8970settings.updateFlags.each.bLossCompenstaion);
}

/*!     \brief  Callback for loss before DUT spin button
 *
 * Callback for loss before DUT spin button
 *
 * \param  wSpinLossBeforeDUT pointer to GtkSpinButton
 * \param  udata              user data
 */
void
CB_spin_LossAfterDUT(  GtkSpinButton* wSpinLossAfterDUT, gpointer udata ) {

    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wSpinLossAfterDUT), "data");

    pGlobal->HP8970settings.lossAfterDUT = gtk_spin_button_get_value( wSpinLossAfterDUT );

    UPDATE_8970_SETTING( pGlobal, pGlobal->HP8970settings.updateFlags.each.bLossCompenstaion);
}

/*!     \brief  Change enable loss compensation setting
 *
 * Change enable loss compensation setting
 *
 * \param  wChkEnableLossCompensation   pointer auto scale check button widget
 * \param  user_data                    unused
 */
void CB_chk_LossCompensationEnable ( GtkCheckButton *wChkEnableLossCompensation, gpointer user_data
) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wChkEnableLossCompensation), "data");
    pGlobal->HP8970settings.switches.bLossCompensation = gtk_check_button_get_active( wChkEnableLossCompensation );

    UPDATE_8970_SETTING( pGlobal, pGlobal->HP8970settings.updateFlags.each.bLossCompenstaion);
}


/*!     \brief  Refresh widgets on the HP8970 page
 *
 * Refresh widgets on the HP8970 page
 *
 * \param  pGlobal      pointer to global data
 */
void
refreshPageHP8970( tGlobal *pGlobal ) {
    gpointer wDropNoiseUnits = pGlobal->widgets[ eW_drop_NoiseUnits ];
    gpointer wSpinColdTemp = pGlobal->widgets[ eW_spin_ColdT ];
    gpointer wSpinLossTemp = pGlobal->widgets[ eW_spin_LossT ];
    gpointer wSpinLossBeforeDUT = pGlobal->widgets[ eW_spin_LossBefore ];
    gpointer wSpinLossAfterDUT = pGlobal->widgets[ eW_spin_LossAfter ];
    gpointer wSpinLossCompenstaionEnable = pGlobal->widgets[ eW_chk_LossOn ];
    gpointer wDropInputGainCalibration = pGlobal->widgets[ eW_drop_InputGainCalibration ];

    gtk_drop_down_set_selected ( wDropNoiseUnits, pGlobal->HP8970settings.noiseUnits );
    gtk_check_button_set_active ( wSpinLossCompenstaionEnable, pGlobal->HP8970settings.switches.bLossCompensation );

    gtk_spin_button_set_value( wSpinColdTemp, pGlobal->HP8970settings.coldTemp );
    gtk_spin_button_set_value( wSpinLossTemp, pGlobal->HP8970settings.lossTemp );
    gtk_spin_button_set_value( wSpinLossBeforeDUT, pGlobal->HP8970settings.lossBeforeDUT );
    gtk_spin_button_set_value( wSpinLossAfterDUT, pGlobal->HP8970settings.lossAfterDUT );
    gtk_drop_down_set_selected ( wDropInputGainCalibration, pGlobal->HP8970settings.inputGainCal );

}

/*!     \brief  Initialize the widgets on the HP8970 page
 *
 * Initialize the widgets on the HP8970 page
 *
 * \param  pGlobal      pointer to global data
 */
void
initializePageHP8970( tGlobal *pGlobal ) {

    refreshPageHP8970( pGlobal );

    g_signal_connect_after( pGlobal->widgets[ eW_drop_NoiseUnits ], "notify::selected", G_CALLBACK( CB_drop_NoiseUnits ), NULL);
    g_signal_connect( pGlobal->widgets[ eW_spin_ColdT ], "value-changed", G_CALLBACK( CB_spin_ColdTemp ), NULL);
    g_signal_connect( pGlobal->widgets[ eW_spin_LossT ], "value-changed", G_CALLBACK( CB_spin_LossTemp ), NULL);
    g_signal_connect( pGlobal->widgets[ eW_spin_LossBefore ], "value-changed", G_CALLBACK( CB_spin_LossBeforeDUT ), NULL);
    g_signal_connect( pGlobal->widgets[ eW_spin_LossAfter ], "value-changed", G_CALLBACK( CB_spin_LossAfterDUT ), NULL);
    g_signal_connect( pGlobal->widgets[ eW_chk_LossOn ], "toggled", G_CALLBACK( CB_chk_LossCompensationEnable ), NULL);
    g_signal_connect_after( pGlobal->widgets[ eW_drop_InputGainCalibration ], "notify::selected", G_CALLBACK( CB_drop_InputGainCalibration ), NULL);
}
