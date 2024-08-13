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

widgetIDs PDFpageSizeWidgetNames[] = { eW_chk_SettingsA4, eW_chk_SettingsLetter, eW_chk_SettingsA3, eW_chk_SettingsTabloid};
widgetIDs VariantWidgetNames[] = { eW_chk_Settings8970A, eW_chk_Settings8970B, eW_chk_Settings8970Bopt20};


/*!     \brief  Callback for the show HP logo check button
 *
 * Callback for the show HP logo check button
 *
 * \param  wHPlogo          pointer to GtkCheckButton widget
 * \param  identifier       which of the radio buttons is pressed
 */
static void
CB_chk_SettingsHPlogo(GtkCheckButton *wHPlogo, gpointer udata) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wHPlogo), "data");

    pGlobal->flags.bShowHPlogo = gtk_check_button_get_active (wHPlogo);
    gtk_widget_queue_draw ( pGlobal->widgets[ eW_drawing_Plot ] );
}

/*!     \brief  Callback for the show time check button
 *
 * Callback for the show time check button
 *
 * \param  wTime            pointer to GtkCheckButton widget
 * \param  identifier       which of the radio buttons is pressed
 */
static void
CB_chk_SettingsTime(GtkCheckButton *wTime, gpointer udata) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wTime), "data");

    pGlobal->flags.bShowTime = gtk_check_button_get_active (wTime);
    gtk_widget_queue_draw ( pGlobal->widgets[ eW_drawing_Plot ] );
}

/*!     \brief  Callback for the PDF page size radio check buttons
 *
 * Callback for the PDF page size radio check buttons
 *
 * \param  wPDFpageSize     pointer to GtkCheckButton widget
 * \param  identifier       which of the radio buttons is pressed
 */
static void
CB_chk_PDFpageSize(GtkCheckButton *wPDFpageSize, gpointer identifier) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wPDFpageSize), "data");

    if( gtk_check_button_get_active (wPDFpageSize) )
        pGlobal->PDFpaperSize = GPOINTER_TO_INT( identifier );
}

/*!     \brief  Callback for the PDF page size radio check buttons
 *
 * Callback for the PDF page size radio check buttons
 *
 * \param  wPDFpageSize     pointer to GtkCheckButton widget
 * \param  identifier       which of the radio buttons is pressed
 */
static void
CB_chk_Variants(GtkCheckButton *wVariant, gpointer identifier) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wVariant), "data");

    if( gtk_check_button_get_active (wVariant) ) {
        pGlobal->flags.bbHP8970Bmodel = GPOINTER_TO_INT( identifier );
        gtk_spin_button_set_range( pGlobal->widgets[ eW_LO_spin_FixedIF_Freq ], 10.0, maxInputFreq[ pGlobal->flags.bbHP8970Bmodel ] );
    }
    gtk_widget_queue_draw ( pGlobal->widgets[ eW_drawing_Plot ] );
}

/*!     \brief  Initialize the widgets on the Settings Notebook page
 *
 * Initialize the widgets on the Settings Notebook page
 *
 * \param  pGlobal      pointer to global data
 */
void
initializePageOptions( tGlobal *pGlobal ) {
    gtk_check_button_set_active ( pGlobal->widgets[ eW_chk_SettingsHPlogo ], pGlobal->flags.bShowHPlogo );
    gtk_check_button_set_active ( pGlobal->widgets[ eW_chk_SettingsTime ], pGlobal->flags.bShowTime );

    gtk_check_button_set_active (  pGlobal->widgets[ PDFpageSizeWidgetNames[ pGlobal->PDFpaperSize % N_PAPER_SIZES ] ], TRUE );
    gtk_check_button_set_active ( pGlobal->widgets[ VariantWidgetNames[ pGlobal->flags.bbHP8970Bmodel % N_VARIANTS ] ], TRUE );

    for( gint i=0; i < N_PAPER_SIZES; i++ )
        g_signal_connect( pGlobal->widgets[ PDFpageSizeWidgetNames[ i ] ], "toggled", G_CALLBACK( CB_chk_PDFpageSize ), GINT_TO_POINTER(i));
    for( gint i=0; i < N_VARIANTS; i++ )
        g_signal_connect( pGlobal->widgets[ VariantWidgetNames[ i ] ], "toggled", G_CALLBACK( CB_chk_Variants ), GINT_TO_POINTER(i));

    g_signal_connect(pGlobal->widgets[ eW_chk_SettingsHPlogo ],   "toggled", G_CALLBACK( CB_chk_SettingsHPlogo ), NULL);
    g_signal_connect(pGlobal->widgets[ eW_chk_SettingsTime ],   "toggled", G_CALLBACK( CB_chk_SettingsTime ), NULL);
}
