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
 * The schema is defined in us.heterodyne.hpgl-plotter.gschema.xml
 * It is copied to /usr/share/glib-2.0/schemas and then glib-compile-schemas is run to compile.
 * (to test the schema use: glib-compile-schemas --dry-run /path/to/us.heterodyne.hpgl-plotter.gschema.xml
 * $ sudo cp us.heterodyne.hpgl-plotter.gschema.xml /usr/share/glib-2.0/schemas
 * $ sudo glib-compile-schemas /usr/share/glib-2.0/schemas
 */
gint
saveSettings( tGlobal *pGlobal ) {

    GVariant *gvPrintSettings = NULL, *gvPageSetup = NULL,
             *gvPenColors, *gvGain, *gvNoise, *gvFreq, *gvNoiseSourceTable;
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

    g_settings_set_boolean( gs, "corrected-noise-gain", pGlobal->HP8970settings.switches.bCorrectedNFAndGain );
    g_settings_set_int    ( gs, "smoothing", (gint)round( log2( pGlobal->HP8970settings.smoothingFactor ) ) );
    g_settings_set_int    ( gs, "mode", (gint)pGlobal->HP8970settings.mode );
    g_settings_set_int    ( gs, "frequency", (gint)pGlobal->HP8970settings.range[eMode1_0].freqSpotMHz );
    g_settings_set_int    ( gs, "frequency-r2", (gint)pGlobal->HP8970settings.range[eMode1_1].freqSpotMHz );

    g_settings_set_int    ( gs, "sweep-start", (gint)pGlobal->HP8970settings.range[eMode1_0].freqStartMHz );
    g_settings_set_int    ( gs, "sweep-stop", (gint)pGlobal->HP8970settings.range[eMode1_0].freqStopMHz );
    g_settings_set_int    ( gs, "sweep-step-cal", (gint)pGlobal->HP8970settings.range[eMode1_0].freqStepCalMHz );
    g_settings_set_int    ( gs, "sweep-step-sweep", (gint)pGlobal->HP8970settings.range[eMode1_0].freqStepSweepMHz );
    g_settings_set_int    ( gs, "sweep-start-r2", (gint)pGlobal->HP8970settings.range[eMode1_1].freqStartMHz );
    g_settings_set_int    ( gs, "sweep-stop-r2", (gint)pGlobal->HP8970settings.range[eMode1_1].freqStopMHz );
    g_settings_set_int    ( gs, "sweep-step-cal-r2", (gint)pGlobal->HP8970settings.range[eMode1_1].freqStepCalMHz );
    g_settings_set_int    ( gs, "sweep-step-sweep-r2", (gint)pGlobal->HP8970settings.range[eMode1_1].freqStepSweepMHz );

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

    // GUI notebook page HP8970
    g_settings_set_int    ( gs, "noise-units", (gint)pGlobal->HP8970settings.noiseUnits );
    g_settings_set_double ( gs, "option-loss-before", pGlobal->HP8970settings.lossBeforeDUT );
    g_settings_set_double ( gs, "option-loss-after", pGlobal->HP8970settings.lossAfterDUT );
    g_settings_set_double ( gs, "option-t-loss", pGlobal->HP8970settings.lossTemp );
    g_settings_set_double ( gs, "option-t-cold", pGlobal->HP8970settings.coldTemp );
    g_settings_set_boolean( gs, "option-loss-comp-on", pGlobal->HP8970settings.switches.bLossCompensation );

    // GUI notebook page source
    // Noise Source freq/ENR tables (array of bytes)
    for (int i = 0; i < MAX_NOISE_SOURCES; i++) {
        gvNoiseSourceTable = g_variant_new_from_data( G_VARIANT_TYPE ("a(y)"), (void *)(&pGlobal->HP8970settings.noiseSources[ i ]),
                                 sizeof( tNoiseSource ), TRUE, NULL, NULL);
        g_snprintf( sName, SHORT_STRING, "noise-source-table-%d", i + 1 );
        g_settings_set_value( gs, sName, gvNoiseSourceTable ); // this consumes the GVariant
    }
    g_settings_set_int    ( gs, "noise-source-table-selected", (gint)pGlobal->HP8970settings.activeNoiseSource );

    // GUI notebook page external L.O.
    g_settings_set_string ( gs, "extlo-setup", pGlobal->HP8970settings.sExtLOsetup );
    g_settings_set_string ( gs, "extlo-set-freq", pGlobal->HP8970settings.sExtLOsetFreq );
    g_settings_set_int ( gs, "extlo-if-freq", pGlobal->HP8970settings.extLOfreqIF );
    g_settings_set_int ( gs, "extlo-lo-freq", pGlobal->HP8970settings.extLOfreqLO );
    g_settings_set_int ( gs, "extlo-settling-time", pGlobal->HP8970settings.settlingTime_ms );
    g_settings_set_int ( gs, "extlo-sideband", pGlobal->HP8970settings.extLOsideband );

    // GUI notebook page plot
    builder = g_variant_builder_new (G_VARIANT_TYPE ("a(dd)"));
    g_variant_builder_add (builder, "(dd)", pGlobal->fixedGridGain[0], pGlobal->fixedGridGain[1]);
    gvGain = g_variant_new ("a(dd)", builder);
    g_variant_builder_unref (builder);
    g_settings_set_value( gs, "grid-gain-range", gvGain ); // this consumes the GVariant

    // Unused ... placeholder
    builder = g_variant_builder_new (G_VARIANT_TYPE ("a(dd)"));
    g_variant_builder_add (builder, "(dd)", pGlobal->fixedGridFreq[0], pGlobal->fixedGridFreq[1]);
    gvFreq = g_variant_new ("a(dd)", builder);
    g_variant_builder_unref (builder);
    g_settings_set_value( gs, "grid-freq-range", gvFreq ); // this consumes the GVariant

    builder = g_variant_builder_new (G_VARIANT_TYPE ("a(dd)"));
    for (int i = 0; i < eMAX_NOISE_UNITS; i++) {
        g_variant_builder_add (builder, "(dd)", pGlobal->fixedGridNoise[i][0], pGlobal->fixedGridNoise[i][1]);
    }
    gvNoise = g_variant_new ("a(dd)", builder);
    g_variant_builder_unref (builder);
    g_settings_set_value( gs, "grid-noise-range", gvNoise ); // this consumes the GVariant

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
    g_settings_set_boolean( gs, "plot-autoscaling", pGlobal->flags.bAutoScaling );

//    g_variant_unref (gvPenColors);

    g_object_unref( gs );

    return TRUE;
}

gint
recoverSettings( tGlobal *pGlobal ) {
    GSettings *gs;
    GVariant *gvPenColors, *gvPrintSettings, *gvPageSetup,
             *gvNoise, *gvNoiseSourceTable, *gvFreq, *gvGain;
    GVariantIter iter;
    gdouble RGBAcolor[4] = {0};
    gint  pen = 0;
    gsize nBytes = 0;
    gdouble limits[2];
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

    pGlobal->HP8970settings.switches.bCorrectedNFAndGain = g_settings_get_boolean( gs, "corrected-noise-gain" );
    pGlobal->HP8970settings.smoothingFactor = (gint)pow( 2.0, g_settings_get_int( gs, "smoothing" ) );
    pGlobal->HP8970settings.mode = g_settings_get_int( gs, "mode" );

    pGlobal->HP8970settings.range[eMode1_0].freqSpotMHz = (gdouble)g_settings_get_int( gs, "frequency" );
    pGlobal->HP8970settings.range[eMode1_1].freqSpotMHz = (gdouble)g_settings_get_int( gs, "frequency-r2" );

    pGlobal->HP8970settings.range[eMode1_0].freqStartMHz = (gdouble)g_settings_get_int( gs, "sweep-start" );
    pGlobal->HP8970settings.range[eMode1_0].freqStopMHz = (gdouble)g_settings_get_int( gs, "sweep-stop" );
    pGlobal->HP8970settings.range[eMode1_0].freqStepCalMHz = (gdouble)g_settings_get_int( gs, "sweep-step-cal" );
    pGlobal->HP8970settings.range[eMode1_0].freqStepSweepMHz = (gdouble)g_settings_get_int( gs, "sweep-step-sweep" );
    pGlobal->HP8970settings.range[eMode1_1].freqStartMHz = (gdouble)g_settings_get_int( gs, "sweep-start-r2" );
    pGlobal->HP8970settings.range[eMode1_1].freqStopMHz = (gdouble)g_settings_get_int( gs, "sweep-stop-r2" );
    pGlobal->HP8970settings.range[eMode1_1].freqStepCalMHz = (gdouble)g_settings_get_int( gs, "sweep-step-cal-r2" );
    pGlobal->HP8970settings.range[eMode1_1].freqStepSweepMHz = (gdouble)g_settings_get_int( gs, "sweep-step-sweep-r2" );


    // GUI notebook page options
    pGlobal->flags.bShowTime = g_settings_get_boolean( gs, "show-time" );
    pGlobal->flags.bShowHPlogo = g_settings_get_boolean( gs, "show-hp-logo" );
    pGlobal->flags.bbHP8970Bmodel = g_settings_get_int( gs, "model-variant" );
    pGlobal->PDFpaperSize = g_settings_get_int( gs, "pdf-paper-size" );

    // GUI notebook page GPIB
    pGlobal->sGPIBdeviceName = g_settings_get_string( gs, "gpib-device-name" );
    pGlobal->GPIBcontrollerIndex  = g_settings_get_int( gs, "gpib-controller-index" );
    pGlobal->GPIBdevicePID = g_settings_get_int( gs, "gpib-device-pid" );
    pGlobal->flags.bGPIB_UseCardNoAndPID = g_settings_get_boolean( gs, "gpib-use-device-pid" );
    pGlobal->sGPIBextLOdeviceName = g_settings_get_string( gs, "gpib-extlo-device-name" );
    pGlobal->GPIB_extLO_PID = g_settings_get_int( gs, "gpib-extlo-device-pid" );
    pGlobal->flags.bGPIB_extLO_usePID = g_settings_get_boolean( gs, "gpib-extlo-use-device-pid" );

    // GUI notebook page external L.O.
    pGlobal->HP8970settings.sExtLOsetup = g_settings_get_string ( gs, "extlo-setup" );
    pGlobal->HP8970settings.sExtLOsetFreq = g_settings_get_string ( gs, "extlo-set-freq" );
    pGlobal->HP8970settings.extLOfreqIF = g_settings_get_int ( gs, "extlo-if-freq" );
    pGlobal->HP8970settings.extLOfreqLO = g_settings_get_int ( gs, "extlo-lo-freq" );
    pGlobal->HP8970settings.settlingTime_ms = g_settings_get_int ( gs, "extlo-settling-time" );
    pGlobal->HP8970settings.extLOsideband = g_settings_get_int ( gs, "extlo-sideband" );

    // GUI page HP8970
    pGlobal->HP8970settings.noiseUnits = g_settings_get_int( gs, "noise-units" );
    pGlobal->HP8970settings.lossBeforeDUT = g_settings_get_double ( gs, "option-loss-before" );
    pGlobal->HP8970settings.lossAfterDUT = g_settings_get_double ( gs, "option-loss-after" );
    pGlobal->HP8970settings.lossTemp = g_settings_get_double ( gs, "option-t-loss" );
    pGlobal->HP8970settings.coldTemp = g_settings_get_double ( gs, "option-t-cold" );
    pGlobal->HP8970settings.switches.bLossCompensation = g_settings_get_boolean( gs, "option-loss-comp-on" );

    // GUI notebook page source
    // Noise Source tables
    for (int i = 0; i < MAX_NOISE_SOURCES; i++) {
        g_snprintf( sName, SHORT_STRING, "noise-source-table-%d", i + 1 );
        gvNoiseSourceTable =  g_settings_get_value ( gs, sName );
        pNoiseSourceTable = g_variant_get_fixed_array( gvNoiseSourceTable, &nBytes, sizeof( guint8 ));
        if( nBytes > 0 && pNoiseSourceTable != NULL )
            memcpy( &pGlobal->HP8970settings.noiseSources[ i ], pNoiseSourceTable, sizeof( tNoiseSource ) );
        g_variant_unref (gvNoiseSourceTable);
    }
    pGlobal->HP8970settings.activeNoiseSource = g_settings_get_int( gs, "noise-source-table-selected" );

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
    pGlobal->flags.bAutoScaling = g_settings_get_boolean( gs, "plot-autoscaling" );
    gvGain = g_settings_get_value ( gs, "grid-gain-range" );
    g_variant_iter_init ( &iter, gvGain);
    if ( g_variant_iter_next (&iter, "(dd)", &limits[0], &limits[1])) {
        pGlobal->fixedGridGain[0] = limits[0];
        pGlobal->fixedGridGain[1] = limits[1];
    }
    g_variant_unref (gvGain);

    // Unused ... placeholder
    gvFreq = g_settings_get_value ( gs, "grid-freq-range" );
    g_variant_iter_init ( &iter, gvFreq);
    if ( g_variant_iter_next (&iter, "(dd)", &limits[0], &limits[1])) {
        pGlobal->fixedGridFreq[0] = limits[0];
        pGlobal->fixedGridFreq[1] = limits[1];
    }
    g_variant_unref (gvFreq);

    // There are different ranges for all the noise types
    // Ndb, N, Y, Ydb, TK
    gvNoise = g_settings_get_value ( gs, "grid-noise-range" );
    g_variant_iter_init ( &iter, gvNoise);
    gint i=0;
    while ( g_variant_iter_next (&iter, "(dd)", &limits[0], &limits[1]) && i < eMAX_NOISE_UNITS) {
        pGlobal->fixedGridNoise[i][0] = limits[0];
        pGlobal->fixedGridNoise[i][1] = limits[1];
        i++;
    }
    g_variant_unref (gvNoise);


    g_object_unref( gs );

    return TRUE;
}
