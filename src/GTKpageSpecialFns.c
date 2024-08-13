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
 * \param  wNoiseUnit     pointer to GtkSpinButton
 * \param  udata          user data
 */
static void
CB_chk_NoiseUnits (GtkCheckButton *wNoiseUnit, gpointer identifier)
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wNoiseUnit), "data");
    gboolean bActive = gtk_check_button_get_active (wNoiseUnit);

    if( bActive ) {
        pGlobal->HP8970settings.noiseUnits = (tNoiseType)GPOINTER_TO_INT( identifier );
        UPDATE_8970_SETTING( pGlobal, pGlobal->HP8970settings.updateFlags.each.bNoiseUnits);
    }
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
    static widgetIDs noiseUnitWidgetIDs[] = {
            [eFdB]  = eW_chk_NoiseUnit_FdB,
            [eF]    = eW_chk_NoiseUnit_F,
            [eYdB]  = eW_chk_NoiseUnit_YdB,
            [eY]    = eW_chk_NoiseUnit_Y,
            [eTeK ] = eW_chk_NoiseUnit_TeK
    };

    gpointer wSpinColdTemp = pGlobal->widgets[ eW_spin_ColdT ];
    gpointer wSpinLossTemp = pGlobal->widgets[ eW_spin_LossT ];
    gpointer wSpinLossBeforeDUT = pGlobal->widgets[ eW_spin_LossBefore ];
    gpointer wSpinLossAfterDUT = pGlobal->widgets[ eW_spin_LossAfter ];
    gpointer wSpinLossCompenstaionEnable = pGlobal->widgets[ eW_chk_LossOn ];

    gtk_check_button_set_active ( pGlobal->widgets[ noiseUnitWidgetIDs[ pGlobal->HP8970settings.noiseUnits ] ], TRUE );
    gtk_check_button_set_active ( wSpinLossCompenstaionEnable, pGlobal->HP8970settings.switches.bLossCompensation );

    gtk_spin_button_set_value( wSpinColdTemp, pGlobal->HP8970settings.coldTemp );
    gtk_spin_button_set_value( wSpinLossTemp, pGlobal->HP8970settings.lossTemp );
    gtk_spin_button_set_value( wSpinLossBeforeDUT, pGlobal->HP8970settings.lossBeforeDUT );
    gtk_spin_button_set_value( wSpinLossAfterDUT, pGlobal->HP8970settings.lossAfterDUT );

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

    g_signal_connect(pGlobal->widgets[ eW_chk_NoiseUnit_FdB ], "toggled", G_CALLBACK( CB_chk_NoiseUnits ), (gpointer) eFdB);
    g_signal_connect(pGlobal->widgets[ eW_chk_NoiseUnit_F ],   "toggled", G_CALLBACK( CB_chk_NoiseUnits ), (gpointer) eF);
    g_signal_connect(pGlobal->widgets[ eW_chk_NoiseUnit_YdB ], "toggled", G_CALLBACK( CB_chk_NoiseUnits ), (gpointer) eYdB);
    g_signal_connect(pGlobal->widgets[ eW_chk_NoiseUnit_Y ],   "toggled", G_CALLBACK( CB_chk_NoiseUnits ), (gpointer) eY);
    g_signal_connect(pGlobal->widgets[ eW_chk_NoiseUnit_TeK ], "toggled", G_CALLBACK( CB_chk_NoiseUnits ), (gpointer) eTeK);
    g_signal_connect( pGlobal->widgets[ eW_spin_ColdT ], "value-changed", G_CALLBACK( CB_spin_ColdTemp ), NULL);
    g_signal_connect( pGlobal->widgets[ eW_spin_LossT ], "value-changed", G_CALLBACK( CB_spin_LossTemp ), NULL);
    g_signal_connect( pGlobal->widgets[ eW_spin_LossBefore ], "value-changed", G_CALLBACK( CB_spin_LossBeforeDUT ), NULL);
    g_signal_connect( pGlobal->widgets[ eW_spin_LossAfter ], "value-changed", G_CALLBACK( CB_spin_LossAfterDUT ), NULL);
    g_signal_connect( pGlobal->widgets[ eW_chk_LossOn ], "toggled", G_CALLBACK( CB_chk_LossCompensationEnable ), NULL);
}
