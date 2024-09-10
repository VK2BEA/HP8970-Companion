/*
 * Copyright (c) 2022 Michael G. Katzmann
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <glib-2.0/glib.h>
#include <gtk/gtk.h>
#include <locale.h>
#include <time.h>
#include <errno.h>

#include "HP8970.h"

/*!     \brief  Check to see if the schema for the settings exists
 *
 * Check to see if the schema for the settings exists
 *
 * \param id          schema identifier
 * \return            true if exists
 */
static gboolean g_settings_schema_exist (const char * id)
{
  gboolean bReturn = FALSE;
  GSettingsSchema *gss;

  if( (gss = g_settings_schema_source_lookup (
                  g_settings_schema_source_get_default(), id, TRUE)) != NULL ) {
      bReturn = TRUE;
      g_settings_schema_unref ( gss );
  }

  return bReturn;
}

/*
 * The schema is defined in us.heterodyne.hp8970.gschema.xml
 * It is copied to /usr/share/glib-2.0/schemas and then glib-compile-schemas is run to compile.
 * (to test the schema use: glib-compile-schemas --dry-run /path/to/us.heterodyne.hp8970.gschema.xml
 * $ sudo cp us.heterodyne.hp8970.gschema.xml /usr/share/glib-2.0/schemas
 * $ sudo glib-compile-schemas /usr/share/glib-2.0/schemas
 */

/*!     \brief  Save settings in the dconf system
 *
 * Save settings in the dconf system
 *
 * \pGlobal     pointer to global data
 * \return      true if OK, False if error (no save)
 */
gint
saveSettings( tGlobal *pGlobal ) {

    GVariant *gvPrintSettings = NULL, *gvPageSetup = NULL,
             *gvPenColors, *gvNoiseSourceTable;
    GVariantBuilder *builder;
    gchar sName[ SHORT_STRING ];

    GSettings *gs;

    if( !g_settings_schema_exist( GSETTINGS_SCHEMA ) )
        return FALSE;

    gs = g_settings_new( GSETTINGS_SCHEMA );
    if( pGlobal->printSettings )
        gvPrintSettings = gtk_print_settings_to_gvariant( pGlobal->printSettings );
    if( pGlobal->pageSetup )
        gvPageSetup  = gtk_page_setup_to_gvariant( pGlobal->pageSetup );

//    g_print( "Print Settings is of type %s\n", g_variant_get_type_string( gvPrintSettings ));
    if( gvPrintSettings )
        g_settings_set_value( gs, "print-settings", gvPrintSettings ); // this consumes the GVariant

//    g_print( "Print Setup is of type %s\n", g_variant_get_type_string( gvPageSetup ));
    if( gvPageSetup )
        g_settings_set_value( gs, "page-setup", gvPageSetup );  // this consumes the GVariant

    g_settings_set_string ( gs, "last-directory", pGlobal->sLastDirectory );

    // GUI notebook page GPIB
    g_settings_set_int    ( gs, "gpib-controller-index", pGlobal->GPIBcontrollerIndex );
    g_settings_set_int    ( gs, "gpib-device-pid", pGlobal->GPIBdevicePID );
    g_settings_set_boolean( gs, "gpib-use-device-pid", pGlobal->flags.bGPIB_UseCardNoAndPID);
    g_settings_set_string ( gs, "gpib-device-name", pGlobal->sGPIBdeviceName );

    g_settings_set_string ( gs, "gpib-extlo-device-name", pGlobal->sGPIBextLOdeviceName );
    g_settings_set_int    ( gs, "gpib-extlo-device-pid", pGlobal->GPIB_extLO_PID );
    g_settings_set_boolean( gs, "gpib-extlo-use-device-pid", pGlobal->flags.bGPIB_extLO_usePID);

    // GUI notebook page options
    g_settings_set_boolean( gs, "show-hp-logo", (gint)pGlobal->flags.bShowHPlogo );
    g_settings_set_boolean( gs, "show-time", (gint)pGlobal->flags.bShowTime );
    g_settings_set_int( gs, "model-variant", pGlobal->flags.bbHP8970Bmodel );
    g_settings_set_int    ( gs, "pdf-paper-size", pGlobal->PDFpaperSize );
    g_settings_set_int    ( gs, "selected-configuration", pGlobal->selectedConfiguration );

    // GUI notebook page source
    // Noise Source freq/ENR tables (array of bytes)
    for (int i = 0; i < MAX_NOISE_SOURCES; i++) {
        gvNoiseSourceTable = g_variant_new_from_data( G_VARIANT_TYPE ("a(y)"), (void *)(&pGlobal->noiseSources[ i ]),
                                 sizeof( tNoiseSource ), TRUE, NULL, NULL);
        g_snprintf( sName, SHORT_STRING, "noise-source-table-%d", i + 1 );
        g_settings_set_value( gs, sName, gvNoiseSourceTable ); // this consumes the GVariant
    }
    g_settings_set_int    ( gs, "noise-source-table-selected", (gint)pGlobal->activeNoiseSource );

    // Trace Colors
    builder = g_variant_builder_new (G_VARIANT_TYPE ("a(dddd)"));
    for (int i = 0; i < eMAX_COLORS; i++)
    {
        g_variant_builder_add (builder, "(dddd)", plotElementColors[i].red,
                               plotElementColors[i].green, plotElementColors[i].blue, plotElementColors[i].alpha);
    }
    gvPenColors = g_variant_new ("a(dddd)", builder);
    g_variant_builder_unref (builder);
    g_settings_set_value( gs, "trace-colors", gvPenColors ); // this consumes the GVariant

    g_settings_set_string ( gs, "plot-title", pGlobal->plot.sTitle ? pGlobal->plot.sTitle : "");
    g_settings_set_string ( gs, "plot-notes", pGlobal->plot.sNotes ? pGlobal->plot.sNotes : "");
//    g_variant_unref (gvPenColors);

    g_object_unref( gs );

    return TRUE;
}

/*!     \brief  Recover settings from the dconf system
 *
 * Recover settings from the dconf system
 *
 * \pGlobal     pointer to global data
 * \return      true if OK, False if error (no recovery)
 */
gint
recoverSettings( tGlobal *pGlobal ) {
    GSettings *gs;
    GVariant *gvPenColors, *gvPrintSettings, *gvPageSetup,
             *gvNoiseSourceTable;
    GVariantIter iter;
    gdouble RGBAcolor[4] = {0};
    gint  pen = 0;
    gsize nBytes = 0;
    gconstpointer pNoiseSourceTable;
    gchar sName[ SHORT_STRING ];

    if( !g_settings_schema_exist( GSETTINGS_SCHEMA ) )
        return FALSE;

    gs = g_settings_new( GSETTINGS_SCHEMA );

    gvPrintSettings = g_settings_get_value ( gs, "print-settings" );
    // This will have been NULL .. so no need to free any old settings
    pGlobal->printSettings = gtk_print_settings_new_from_gvariant( gvPrintSettings );
    g_variant_unref (gvPrintSettings);
    // ditto
    gvPageSetup     = g_settings_get_value ( gs, "page-setup" );
    pGlobal->pageSetup = gtk_page_setup_new_from_gvariant( gvPageSetup );
    g_variant_unref (gvPageSetup);

    pGlobal->sLastDirectory = g_settings_get_string( gs, "last-directory" );

    // GUI notebook page options
    pGlobal->flags.bShowTime = g_settings_get_boolean( gs, "show-time" );
    pGlobal->flags.bShowHPlogo = g_settings_get_boolean( gs, "show-hp-logo" );
    pGlobal->flags.bbHP8970Bmodel = g_settings_get_int( gs, "model-variant" );
    pGlobal->PDFpaperSize = g_settings_get_int( gs, "pdf-paper-size" );
    pGlobal->selectedConfiguration = g_settings_get_int( gs, "selected-configuration" );

    // GUI notebook page GPIB
    pGlobal->sGPIBdeviceName = g_settings_get_string( gs, "gpib-device-name" );
    pGlobal->GPIBcontrollerIndex  = g_settings_get_int( gs, "gpib-controller-index" );
    pGlobal->GPIBdevicePID = g_settings_get_int( gs, "gpib-device-pid" );
    pGlobal->flags.bGPIB_UseCardNoAndPID = g_settings_get_boolean( gs, "gpib-use-device-pid" );
    pGlobal->sGPIBextLOdeviceName = g_settings_get_string( gs, "gpib-extlo-device-name" );
    pGlobal->GPIB_extLO_PID = g_settings_get_int( gs, "gpib-extlo-device-pid" );
    pGlobal->flags.bGPIB_extLO_usePID = g_settings_get_boolean( gs, "gpib-extlo-use-device-pid" );

    pGlobal->plot.sTitle = g_settings_get_string ( gs, "plot-title" );
    pGlobal->plot.sNotes = g_settings_get_string ( gs, "plot-notes" );

    // GUI notebook page source
    // Noise Source tables
    for (int i = 0; i < MAX_NOISE_SOURCES; i++) {
        g_snprintf( sName, SHORT_STRING, "noise-source-table-%d", i + 1 );
        gvNoiseSourceTable =  g_settings_get_value ( gs, sName );
        pNoiseSourceTable = g_variant_get_fixed_array( gvNoiseSourceTable, &nBytes, sizeof( guint8 ));
        if( nBytes > 0 && pNoiseSourceTable != NULL )
            memcpy( &pGlobal->noiseSources[ i ], pNoiseSourceTable, sizeof( tNoiseSource ) );
        g_variant_unref (gvNoiseSourceTable);
    }
    pGlobal->activeNoiseSource = g_settings_get_int( gs, "noise-source-table-selected" );

    // GUI page plot
    gvPenColors = g_settings_get_value ( gs, "trace-colors" );
    g_variant_iter_init ( &iter, gvPenColors);
    while (g_variant_iter_next (&iter, "(dddd)", &RGBAcolor[0], &RGBAcolor[1], &RGBAcolor[2], &RGBAcolor[3])) {
        plotElementColors[ pen ].red   = RGBAcolor[0];
        plotElementColors[ pen ].green = RGBAcolor[1];
        plotElementColors[ pen ].blue  = RGBAcolor[2];
        plotElementColors[ pen ].alpha = RGBAcolor[3];
        pen++;
    }
    g_variant_unref (gvPenColors);

    g_object_unref( gs );

    return TRUE;
}



/* a(ddddd) -   0:freqSpotMHz       // mode 1.0 & 1.1 etc
 *               :freqStartMHz
 *               :freqStopMHz
 *               :freqStepCalMHz
 *               :freqStepSweepMHz
 * bb       -   1:bCorrectedNFAndGain
 *              2:bLossCompensation
 *              3:bSpotFrequency
 * qyy      -   4:smoothingFactor
 *              5:noiseUnits
 *              6:mode
 * qqq      -   7:extLOfreqIF
 *              8:extLOfreqLO
 *              9:settlingTime_ms
 * ss       -   10:sExtLOsetup
 *              11:sExtLOsetFreq
 * y        -   12:extLOsideband
 * dddd     -   13:lossBeforeDUT,
 *              14:lossAfterDUT,
 *              15:lossTemp,
 *              16:coldTemp;
 *
 * b        -   17:bAutoScaling
 * a(dd)    -   18:fixedGridNoise (min)
 *                :fixedGridNoise (max)
 * (dd)     -   19:fixedGridGain (min)
 *              37:fixedGridGain (max)
 *
 */

#define CONFIG_TUPPLE "(a(ddddd)(bb)qyy(qqqssy)(dddd)ba(dd)(dd))"


GVariant *
configurationBuilder( tHP8970settings *configuration ){
    int i;
    GVariantBuilder *builder, *configBuilder;

    /* Build the array of bytes */
    configBuilder = g_variant_builder_new(G_VARIANT_TYPE(CONFIG_TUPPLE));

    // a(ddddd)

    builder = g_variant_builder_new (G_VARIANT_TYPE ("a(ddddd)"));
    for( i=0; i < 2; i++ ) {
        g_variant_builder_add (builder, "(ddddd)",
                configuration->range[i].freqSpotMHz,
                configuration->range[i].freqStartMHz,
                configuration->range[i].freqStopMHz,
                configuration->range[i].freqStepCalMHz,
                configuration->range[i].freqStepSweepMHz );
    }

    g_variant_builder_add(configBuilder, "a(ddddd)", builder );
    g_variant_builder_unref (builder);

    // bb

    g_variant_builder_add(configBuilder, "(bb)",
            configuration->switches.bCorrectedNFAndGain, configuration->switches.bLossCompensation );

    // qyy

    g_variant_builder_add(configBuilder, "q", configuration->smoothingFactor );
    g_variant_builder_add(configBuilder, "y", configuration->noiseUnits );
    g_variant_builder_add(configBuilder, "y", configuration->mode );

    // (qqqssy)
    g_variant_builder_add(configBuilder, "(qqqssy)",
            configuration->extLOfreqIF, configuration->extLOfreqLO, configuration->settlingTime_ms,
            configuration->sExtLOsetup != NULL ? configuration->sExtLOsetup : "",
            configuration->sExtLOsetFreq != NULL ? configuration->sExtLOsetFreq : "",
            configuration->extLOsideband );
    // dddd
    g_variant_builder_add(configBuilder, "(dddd)",
            configuration->lossBeforeDUT, configuration->lossAfterDUT, configuration->lossTemp, configuration->coldTemp );

    // b
    g_variant_builder_add(configBuilder, "b", configuration->switches.bAutoScaling );

    // a(dd)
    builder = g_variant_builder_new (G_VARIANT_TYPE ("a(dd)"));
    for( i=0; i < eMAX_NOISE_UNITS; i++ ) {
        g_variant_builder_add (builder, "(dd)",
                configuration->fixedGridNoise[ i ][0],
                configuration->fixedGridNoise[ i ][1] );
    }
    g_variant_builder_add(configBuilder, "a(dd)", builder );
    g_variant_builder_unref (builder);

    g_variant_builder_add(configBuilder, "(dd)", configuration->fixedGridGain[0], configuration->fixedGridGain[1] );

    // Add some more if you want
    GVariant *tupple = g_variant_builder_end(configBuilder);

    g_variant_builder_unref(configBuilder);

    return tupple;
}


/*!     \brief  Save configurations in the dconf system
 *
 * Save configurations in the dconf system
 *
 * \pGlobal     pointer to global data
 * \return      true if OK, False if error (no save)
 */
gint
saveConfigurations( tGlobal *pGlobal ) {
    GSettings *gs;
    GVariantBuilder *dictBuilder;
    GList *configurationItem = pGlobal->configurationList;
    GVariant *tupple;

    if( !g_settings_schema_exist( GSETTINGS_SCHEMA ) )
        return FALSE;

    gs = g_settings_new( GSETTINGS_SCHEMA );

    dictBuilder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));

    // Add some more if you want
    tupple = configurationBuilder( &pGlobal->HP8970settings );
    g_variant_builder_add(dictBuilder, "{sv}", "", tupple);

    while ( configurationItem ) {
        tHP8970settings *pHP8970settings = configurationItem->data;

        // Add some more if you want
        tupple = configurationBuilder( pHP8970settings );
        g_variant_builder_add(dictBuilder, "{sv}", pHP8970settings->sConfigurationName, tupple);

        configurationItem = configurationItem->next;
    }

    GVariant *dict = g_variant_builder_end(dictBuilder);
    g_variant_builder_unref(dictBuilder);

    g_settings_set_value ( gs, "configurations", dict );
    g_object_unref( gs );

    return 0;
}


/*!     \brief  Recover configurations from the dconf system
 *
 * Recover settings from the dconf system
 *
 * \pGlobal     pointer to global data
 * \return      true if OK, False if error (no recovery)
 */
gint
recoverConfigurations( tGlobal *pGlobal ) {

    GSettings *gs;

    GVariantIter iter;
    GVariant *tuple, *configurationsGV;
    gchar *sConfigurationName;
    GVariantIter *freqIter, *noiseGridLimitsIter;
    tHP8970settings *pHP8970settings;
    int i;

    if( !g_settings_schema_exist( GSETTINGS_SCHEMA ) )
        return FALSE;

    gs = g_settings_new( GSETTINGS_SCHEMA );

    configurationsGV = g_settings_get_value ( gs, "configurations" );

    g_variant_iter_init (&iter, configurationsGV);

    g_list_free_full ( pGlobal->configurationList, freeConfigurationItemContent );

    while (g_variant_iter_loop (&iter, "{sv}", &sConfigurationName, &tuple))  {
        if( g_strcmp0( sConfigurationName, "" ) == 0 )
            pHP8970settings = &pGlobal->HP8970settings;
        else
            pHP8970settings = g_malloc0( sizeof( tHP8970settings ));

        // g_print ("%s - type '%s'\n", sConfigurationName, g_variant_get_type_string (tuple));

        gboolean bCorrectedNFAndGain, bLossCompensation, bAutoScaling;
        g_variant_get( tuple, CONFIG_TUPPLE,
                &freqIter,

                &bCorrectedNFAndGain,
                &bLossCompensation,

                &pHP8970settings->smoothingFactor,
                &pHP8970settings->noiseUnits,
                &pHP8970settings->mode,

                // qqq
                &pHP8970settings->extLOfreqIF,
                &pHP8970settings->extLOfreqLO,
                &pHP8970settings->settlingTime_ms,

                // ssy
                &pHP8970settings->sExtLOsetup,
                &pHP8970settings->sExtLOsetFreq,
                &pHP8970settings->extLOsideband,
                // dddd
                &pHP8970settings->lossBeforeDUT,
                &pHP8970settings->lossAfterDUT,
                &pHP8970settings->lossTemp,
                &pHP8970settings->coldTemp,

                &bAutoScaling,

                &noiseGridLimitsIter,

                &pHP8970settings->fixedGridGain[0],
                &pHP8970settings->fixedGridGain[1]
        );

        i = 0;
        while (g_variant_iter_next (freqIter, "(ddddd)",
                &pHP8970settings->range[i].freqSpotMHz,
                &pHP8970settings->range[i].freqStartMHz,
                &pHP8970settings->range[i].freqStopMHz,
                &pHP8970settings->range[i].freqStepCalMHz,
                &pHP8970settings->range[i].freqStepSweepMHz ) && i < 2){
            i++;
        }
        g_variant_iter_free(freqIter);

        i = 0;
        while (g_variant_iter_next (noiseGridLimitsIter, "(dd)",
                &pHP8970settings->fixedGridNoise[ i ][0],
                &pHP8970settings->fixedGridNoise[ i ][1] ) && i < eMAX_NOISE_UNITS){
            i++;
        }
        g_variant_iter_free(noiseGridLimitsIter);

        pHP8970settings->switches.bCorrectedNFAndGain = bCorrectedNFAndGain;
        pHP8970settings->switches.bLossCompensation = bLossCompensation;
        pHP8970settings->switches.bAutoScaling = bAutoScaling;

        if( g_strcmp0( sConfigurationName, "" ) != 0 ) {
            pHP8970settings->sConfigurationName = g_strdup( sConfigurationName );
            pGlobal->configurationList = g_list_insert_sorted( pGlobal->configurationList, pHP8970settings, compareSortConfiguration );
        }
    }

    g_variant_unref( configurationsGV );
    g_object_unref( gs );

    return 0;
}
