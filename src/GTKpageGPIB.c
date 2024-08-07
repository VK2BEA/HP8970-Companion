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

/*!     \brief  Callback GPIB device PID spin
 *
 * Callback GPIB device PID spin
 *
 * \param  wPID      pointer to GtkSpinButton
 * \param  udata     user data
 */
void
CB_spin_GPIB_PID(   GtkSpinButton* wPID,
                   gpointer udata ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wPID), "data");

    pGlobal->GPIBdevicePID = gtk_spin_button_get_value( wPID );
    if( pGlobal->flags.bGPIB_UseCardNoAndPID ) {
        postDataToGPIBThread (TG_ABORT, NULL);
        postDataToGPIBThread (TG_REINITIALIZE_GPIB, NULL);
    }
}

/*!     \brief  Callback GPIB controller index spin
 *
 * Callback GPIB controller index spin
 *
 * \param  wControllerIndex  pointer to GtkSpinButton
 * \param  udata             user data
 */
void
CB_spin_GPIB_ControllerIndex(   GtkSpinButton* wControllerIndex,
                   gpointer udata ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wControllerIndex), "data");

    pGlobal->GPIBdevicePID = gtk_spin_button_get_value( wControllerIndex );
    if( pGlobal->flags.bGPIB_UseCardNoAndPID ){
        postDataToGPIBThread (TG_ABORT, NULL);
        postDataToGPIBThread (TG_REINITIALIZE_GPIB, NULL);
    }
}

/*!     \brief  Callback for 'use GPIB device name' check button
 *
 * Callback for 'use GPIB device name' check button
 *
 * \param  wUseName  pointer to GtkCheckButton
 * \param  udata        user data
 */
void
CB_chk_GPIB_useName (GtkCheckButton *wUseName, gpointer identifier) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wUseName), "data");
    pGlobal->flags.bGPIB_UseCardNoAndPID = !gtk_check_button_get_active (wUseName);
    postDataToGPIBThread (TG_ABORT, NULL);
    postDataToGPIBThread (TG_REINITIALIZE_GPIB, NULL);
}

/*!     \brief  Callback for GPIB device name GtkEntry widget
 *
 * Callback for GPIB device name GtkEntry widget
 *
 * \param  wDeviceName  pointer to GtkEditBuffer of the GtkEntry widget
 * \param  udata        user data
 */
void
CB_edit_GPIBdeviceName ( GtkEditable *wDeviceName, gpointer udata )
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(gtk_widget_get_root(GTK_WIDGET(wDeviceName))), "data");
    const gchar *sDeviceName = gtk_editable_get_text( wDeviceName );

    g_free( pGlobal->sGPIBdeviceName );
    pGlobal->sGPIBdeviceName = g_strdup( sDeviceName );

    if( !pGlobal->flags.bGPIB_UseCardNoAndPID ){
        postDataToGPIBThread (TG_ABORT, NULL);
        postDataToGPIBThread (TG_REINITIALIZE_GPIB, NULL);
    }

}

/*!     \brief  Callback GPIB LO PID spin
 *
 * Callback GPIB LO PID spin (WID_spin_opt_GPIB_PID_LO)
 *
 * \param  wLO_PID  pointer to GtkSpinButton
 * \param  udata             user data
 */
void
CB_spin_opt_GPIB_PID_LO(   GtkSpinButton* wLO_PID, gpointer udata ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wLO_PID), "data");

    pGlobal->GPIB_extLO_PID = gtk_spin_button_get_value( wLO_PID );
    if( pGlobal->flags.bNoLOcontrol == FALSE && pGlobal->flags.bGPIB_extLO_usePID)
        postDataToGPIBThread (TG_SETUP_EXT_LO_GPIB, NULL);
}

/*!     \brief  Callback for GPIB external LO device name GtkEntry widget
 *
 * Callback for GPIB external LO device name GtkEntry widget
 *
 * \param  wDeviceName  pointer to GtkEditBuffer of the GtkEntry widget
 * \param  udata        user data
 */
void
CB_edit_opt_LO_GPIB_name ( GtkEditable *wDeviceName, gpointer udata )
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(gtk_widget_get_root(GTK_WIDGET(wDeviceName))), "data");
    const gchar *sDeviceName = gtk_editable_get_text( wDeviceName );

    g_free( pGlobal->sGPIBextLOdeviceName );
    pGlobal->sGPIBextLOdeviceName = g_strdup( sDeviceName );
    if( pGlobal->flags.bNoLOcontrol == FALSE && !pGlobal->flags.bGPIB_extLO_usePID)
        postDataToGPIBThread (TG_SETUP_EXT_LO_GPIB, NULL);
}


/*!     \brief  Callback for 'use GPIB external LO device name' check button
 *
 * Callback for 'use GPIB external LO device name' check button
 *
 * \param  wUseLOname  pointer to GtkCheckButton
 * \param  udata        user data
 */
void
CB_chk_use_LO_GPIBdeviceName (GtkCheckButton *wUseLOname, gpointer identifier) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wUseLOname), "data");
    pGlobal->flags.bGPIB_extLO_usePID = !gtk_check_button_get_active (wUseLOname);
    if( pGlobal->flags.bNoLOcontrol == FALSE )
        postDataToGPIBThread (TG_SETUP_EXT_LO_GPIB, NULL);
}

/*!     \brief  Initialize the widgets on the GPIB page
 *
 * Initialize the widgets on the GPIB page
 *
 * \param  pGlobal      pointer to global data
 */
void
initializePageGPIB( tGlobal *pGlobal ) {
    gtk_spin_button_set_value( WLOOKUP( pGlobal, "spin_opt_ControllerIdx"), pGlobal->GPIBcontrollerIndex );
    gtk_spin_button_set_value( WLOOKUP( pGlobal, "spin_opt_GPIB_PID"), pGlobal->GPIBdevicePID );
    gtk_spin_button_set_value( WLOOKUP( pGlobal, "spin_opt_GPIB_PID_LO"), pGlobal->GPIB_extLO_PID );
    gtk_entry_buffer_set_text(
                gtk_entry_get_buffer( WLOOKUP( pGlobal, "entry_opt_GPIB_name") ),
                pGlobal->sGPIBdeviceName, -1 );
    gtk_entry_buffer_set_text(
                gtk_entry_get_buffer( WLOOKUP( pGlobal, "entry_opt_LO_GPIB_name") ),
                pGlobal->sGPIBextLOdeviceName, -1 );
    // gboolean bUseGPIBcontrollerName = gtk_check_button_get_active ( WLOOKUP( pGlobal, "cbutton_ControlerNameNotIdx" ) );
    gtk_check_button_set_active ( WLOOKUP( pGlobal, "chk_useGPIBdeviceName" ), !pGlobal->flags.bGPIB_UseCardNoAndPID );
    gtk_check_button_set_active ( WLOOKUP( pGlobal, "chk_use_LO_GPIBdeviceName" ), !pGlobal->flags.bGPIB_extLO_usePID );

    g_signal_connect(WLOOKUP( pGlobal, "spin_opt_GPIB_PID" ), "value-changed", G_CALLBACK( CB_spin_GPIB_PID ), NULL);
    g_signal_connect(WLOOKUP( pGlobal, "spin_opt_ControllerIdx" ), "value-changed", G_CALLBACK( CB_spin_GPIB_ControllerIndex ), NULL);
    g_signal_connect(WLOOKUP( pGlobal, "chk_useGPIBdeviceName" ),  "toggled", G_CALLBACK( CB_chk_GPIB_useName ), NULL);
    g_signal_connect(gtk_editable_get_delegate(GTK_EDITABLE(WLOOKUP( pGlobal, "entry_opt_GPIB_name") )), "changed",
                     G_CALLBACK( CB_edit_GPIBdeviceName ), NULL);

    g_signal_connect(WLOOKUP( pGlobal, "spin_opt_GPIB_PID_LO" ), "value-changed", G_CALLBACK( CB_spin_opt_GPIB_PID_LO ), NULL);
    g_signal_connect(WLOOKUP( pGlobal, "chk_use_LO_GPIBdeviceName" ),  "toggled", G_CALLBACK( CB_chk_use_LO_GPIBdeviceName ), NULL);
    g_signal_connect(gtk_editable_get_delegate(GTK_EDITABLE(WLOOKUP( pGlobal, "entry_opt_LO_GPIB_name") )), "changed",
                     G_CALLBACK( CB_edit_opt_LO_GPIB_name ), NULL);
}

