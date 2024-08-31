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
        gtk_spin_button_set_range( pGlobal->widgets[ eW_LO_spin_FixedIF_Freq ], HP8970A_MIN_FREQ, maxInputFreq[ pGlobal->flags.bbHP8970Bmodel ] );
        gtk_spin_button_set_range( pGlobal->widgets[ eW_spin_FrStop ], HP8970A_MIN_FREQ, maxInputFreq[ pGlobal->flags.bbHP8970Bmodel ] );
    }
    gtk_widget_queue_draw ( pGlobal->widgets[ eW_drawing_Plot ] );
}


/*!     \brief  Free the allocated content of the configuration item
 *
 * Free the allocated content of the configuration item
 *
 * \param  data             pointer to tHP8970settings structure
 */
void freeConfigationItemContent ( gpointer data ) {
    tHP8970settings *pHP8970settings = (tHP8970settings *)data;
    g_free( pHP8970settings->sConfigurationName );
    g_free( pHP8970settings->sExtLOsetFreq );
    g_free( pHP8970settings->sExtLOsetup );
}

/*!     \brief  Create a snapshot of the configuration
 *
 * Create a snapshot of the configuration
 *
 * \param  pGlobal   pointer to global data
 * \param  pSettings pointer existing configuration data or NULL
 * \param  sName     pointer to name of configuration
 * \return           pointer to the snapshot tHP8970settings
 */
tHP8970settings *
snapshotConfiguration( tGlobal *pGlobal, tHP8970settings *pSettings, gchar *sName ) {
    if( pSettings == NULL )
      pSettings = g_malloc0( sizeof( tHP8970settings ) );

    freeConfigationItemContent( pSettings );

    *pSettings = pGlobal->HP8970settings;

    pSettings->sExtLOsetFreq = g_strdup( pGlobal->HP8970settings.sExtLOsetFreq );
    pSettings->sExtLOsetup = g_strdup( pGlobal->HP8970settings.sExtLOsetup );
    pSettings->sConfigurationName = g_strdup( sName );

    return( pSettings );
}

/*!     \brief  compare the confiuration name to that in a tHP8970settings struct
 *
 * compare the configuration name to that in a settings struct
 *
 * \param  gpConfiguration  pointer to tHP8970settings structure
 * \param  gpName           pointer to the string
 * \return         0 equal -1 less than +1 greater than
 */
gint
compareFindConfiguration( gconstpointer gpConfiguration, gconstpointer gpName ) {
    gchar *sConfigurationName = (gchar *) gpName;
    tHP8970settings *pConfiguration = (tHP8970settings *) gpConfiguration;

    return g_strcmp0( sConfigurationName, pConfiguration->sConfigurationName );
}

/*!     \brief  compare the confiuration name to that in a tHP8970settings struct
 *
 * compare the confiuration name to that in a settings struct
 *
 * \param  a       pointer to the string
 * \param  b       pointer to tHP8970settings structure
 * \return         0 equal -1 less than +1 greater than
 */
gint
compareSortConfiguration( gconstpointer a, gconstpointer b ) {
    tHP8970settings *configA = (tHP8970settings *) a;
    tHP8970settings *configB = (tHP8970settings *) b;

    return g_strcmp0( configA->sConfigurationName, configB->sConfigurationName );
}


enum configOpTypes { eRestoreConfig, eSaveConfig, eDeleteConfig };

/*!     \brief  Sensitize the Restore & Delete buttons
 *
 * Sensitize the Restore & Delete buttons
 *
 * \param  pGlobal             pointer to global data
 * \param  sConfigurationName  configuration to find
 */
void
sensitizeConfigurationButtons( tGlobal *pGlobal, gchar *sConfigurationName ) {
    GList* configItem = g_list_find_custom ( pGlobal->configurationList, (gconstpointer)sConfigurationName, compareFindConfiguration );

    gtk_widget_set_sensitive( pGlobal->widgets[ eW_btn_SettingsDelete ], configItem != NULL );
    gtk_widget_set_sensitive( pGlobal->widgets[ eW_btn_SettingsRestore ], configItem != NULL );
}


/*!     \brief  Delete / Restore / Save configuration
 *
 * Delete / Restore / Save configuration
 *
 * \param  wBtn             pointer to button widget
 * \param  eOperation       unused
 */
static void
CB_btn_DeleteRestoreSaveConfiguration ( GtkButton *wBtn, gpointer eOperation ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wBtn), "data");
    gchar *sConfigurationName = NULL;
    GList* configurationItem = NULL;
    tHP8970settings *pHP8970settings = 0;
    gint posn, active;
    GtkComboBoxText *wComboConfiguration = pGlobal->widgets[ eW_combo_SettingsConfigurations ];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    sConfigurationName = gtk_combo_box_text_get_active_text( wComboConfiguration );
    // If we find an existing configuration item, overwrite it; otherwise, create one.
    if( sConfigurationName )
        configurationItem = g_list_find_custom ( pGlobal->configurationList,
                                                 (gconstpointer)sConfigurationName, compareFindConfiguration );

    // unselect text in entry
    GtkEntry *wEntry = GTK_ENTRY( gtk_combo_box_get_child( GTK_COMBO_BOX( wComboConfiguration ) ) );
    gtk_editable_select_region( GTK_EDITABLE(wEntry), -1, -1);

    switch( GPOINTER_TO_INT( eOperation ) ) {
        case eRestoreConfig:
            if ( configurationItem == NULL )
                break;
            freeConfigationItemContent( (gpointer)&pGlobal->HP8970settings );

            // This just copies the references to the strings, so we need to create new string copies
            pGlobal->HP8970settings = *(tHP8970settings *)( configurationItem->data );
            pGlobal->HP8970settings.sConfigurationName = NULL;
            pGlobal->HP8970settings.sExtLOsetFreq = g_strdup( pGlobal->HP8970settings.sExtLOsetFreq );
            pGlobal->HP8970settings.sExtLOsetup = g_strdup( pGlobal->HP8970settings.sExtLOsetup );

            setPageExtLOwidgets( pGlobal );
            refreshPageHP8970( pGlobal );
            refreshMainDialog( pGlobal );
            setFixedRangePlotWidgets( pGlobal );
            break;

        case eSaveConfig:

            if( sConfigurationName == NULL || *sConfigurationName == 0 )
                break;

            pHP8970settings = snapshotConfiguration( pGlobal,
                                                     configurationItem == NULL ? NULL : (tHP8970settings *)configurationItem->data, sConfigurationName );
            if( configurationItem == NULL ) {
                pGlobal->configurationList = g_list_insert_sorted( pGlobal->configurationList, pHP8970settings, compareSortConfiguration );
                gtk_combo_box_text_remove_all( wComboConfiguration );

                for( GList *item = g_list_first( pGlobal->configurationList ); ; item = item->next ) {
                    gtk_combo_box_text_append( wComboConfiguration, NULL,
                            ((tHP8970settings *)item->data)->sConfigurationName );
                    if( item->next == NULL )
                            break;
                }
            }
            sensitizeConfigurationButtons( pGlobal, sConfigurationName );
            saveConfigurations( pGlobal );
            break;
        case eDeleteConfig:
            active = gtk_combo_box_get_active( GTK_COMBO_BOX( wComboConfiguration ) );
            if( sConfigurationName == NULL || *sConfigurationName == 0 )
                break;

            // If we find an existing configuration item, overwrite it; otherwise, create one.
            configurationItem = g_list_find_custom ( pGlobal->configurationList,
                                                     (gconstpointer)sConfigurationName, compareFindConfiguration );
            posn = g_list_position( pGlobal->configurationList, configurationItem );
            gtk_combo_box_text_remove( wComboConfiguration, posn );

            freeConfigationItemContent( configurationItem->data );
            pGlobal->configurationList = g_list_delete_link( pGlobal->configurationList, configurationItem );
            gtk_entry_buffer_set_text( gtk_entry_get_buffer(wEntry), "", -1);
            sensitizeConfigurationButtons( pGlobal, "" );
            gtk_combo_box_set_active( GTK_COMBO_BOX( wComboConfiguration ), active <= 0 ? 0 : active-1 );
            break;
    }
#pragma GCC diagnostic pop
    g_free( sConfigurationName );
}

/*!     \brief  Select configuration
 *
 * Select the configuration to save, restore or delete
 *
 * \param  wSelect       pointer to the combo widget
 * \param  pGlobal       pointer to global data
 */
void
CB_combo_SettingsConfigurations ( GtkComboBox* wSelect, gpointer gpGlobal ) {

    tGlobal *pGlobal = (tGlobal *)gpGlobal;

    // when we type in the edit box we also get here .. check if the name is in the list
    // and enable or disable the restore and delete buttons
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    GtkEntry       *wConfigEntry = GTK_ENTRY( gtk_combo_box_get_child( wSelect ) );
    GtkEntryBuffer __attribute__((unused)) *wNSentryBuffer = gtk_entry_get_buffer( wConfigEntry );
    const gchar    *sNewName = gtk_entry_buffer_get_text( wNSentryBuffer );

    sensitizeConfigurationButtons( pGlobal, (gchar *)sNewName );

#pragma GCC diagnostic pop

}


/*!     \brief  Initialize the widgets on the Settings Notebook page
 *
 * Initialize the widgets on the Settings Notebook page
 *
 * \param  pGlobal      pointer to global data
 */
void
initializePageOptions( tGlobal *pGlobal ) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    g_signal_connect( pGlobal->widgets[ eW_btn_SettingsRestore ],  "clicked", G_CALLBACK( CB_btn_DeleteRestoreSaveConfiguration ), GINT_TO_POINTER( eRestoreConfig ));
    g_signal_connect( pGlobal->widgets[ eW_btn_SettingsSave ],  "clicked", G_CALLBACK( CB_btn_DeleteRestoreSaveConfiguration ), GINT_TO_POINTER( eSaveConfig ));
    g_signal_connect( pGlobal->widgets[ eW_btn_SettingsDelete ],  "clicked", G_CALLBACK( CB_btn_DeleteRestoreSaveConfiguration ), GINT_TO_POINTER( eDeleteConfig ));

    gtk_combo_box_text_remove_all( pGlobal->widgets[ eW_combo_SettingsConfigurations ] );

    for( GList *item = g_list_first( pGlobal->configurationList ); ; item = item->next ) {
        gtk_combo_box_text_append( pGlobal->widgets[ eW_combo_SettingsConfigurations ], NULL,
                ((tHP8970settings *)item->data)->sConfigurationName );
        if( item->next == NULL )
                break;
    }

    // This is called if either the combobox is changed or the entry is changed
    g_signal_connect( pGlobal->widgets[ eW_combo_SettingsConfigurations ], "changed", G_CALLBACK( CB_combo_SettingsConfigurations ), pGlobal);

    gtk_check_button_set_active ( pGlobal->widgets[ eW_chk_SettingsHPlogo ], pGlobal->flags.bShowHPlogo );
    gtk_check_button_set_active ( pGlobal->widgets[ eW_chk_SettingsTime ], pGlobal->flags.bShowTime );

    gtk_check_button_set_active (  pGlobal->widgets[ PDFpageSizeWidgetNames[ pGlobal->PDFpaperSize % N_PAPER_SIZES ] ], TRUE );
    gtk_check_button_set_active ( pGlobal->widgets[ VariantWidgetNames[ pGlobal->flags.bbHP8970Bmodel % N_VARIANTS ] ], TRUE );

    gtk_combo_box_set_active( GTK_COMBO_BOX( pGlobal->widgets[ eW_combo_SettingsConfigurations ] ), 0 );

    for( gint i=0; i < N_PAPER_SIZES; i++ )
        g_signal_connect( pGlobal->widgets[ PDFpageSizeWidgetNames[ i ] ], "toggled", G_CALLBACK( CB_chk_PDFpageSize ), GINT_TO_POINTER(i));
    for( gint i=0; i < N_VARIANTS; i++ )
        g_signal_connect( pGlobal->widgets[ VariantWidgetNames[ i ] ], "toggled", G_CALLBACK( CB_chk_Variants ), GINT_TO_POINTER(i));

    g_signal_connect(pGlobal->widgets[ eW_chk_SettingsHPlogo ],   "toggled", G_CALLBACK( CB_chk_SettingsHPlogo ), NULL);
    g_signal_connect(pGlobal->widgets[ eW_chk_SettingsTime ],   "toggled", G_CALLBACK( CB_chk_SettingsTime ), NULL);
#pragma GCC diagnostic pop
}
