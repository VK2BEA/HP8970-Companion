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
#include <gpib/ib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <locale.h>
#include <errno.h>
#include <HP8970.h>
#include "messageEvent.h"

#include <stdio.h>
#include <glib-object.h>
#include <json-glib/json-glib.h>
#include <json-glib/json-builder.h>
#include <json-glib/json-generator.h>

/*!     \brief  Suggest a filename based on previous filenames or a synthesized one
 *
 * Suggest a filename based on previous filenames or a synthesized one & save previously selected name
 *
 * \param  pGlobal          pointer to global data
 * \param  chosenFileName   filename to strip suffix and save or NULL if asking for a suggestion
 * \param  suffix of filename (i.e. .json, .pdf, .svg, .png
 * \return pointer to filename string (caller must free)
 */
gchar *
suggestFilename( tGlobal *pGlobal, gchar *chosenFileName, gchar *suffix ) {
    static gchar *selected = NULL;
    static gchar *synthesized = NULL;
    gchar *filename = NULL;

    // Saving a selected filename
    if( chosenFileName != NULL ) {
        GString *filenameLower = g_string_ascii_down( g_string_new( chosenFileName ) );
        gchar *dot = strrchr( filenameLower->str, '.' );

        // If we used the synthesized name .. then don't save it as a selected name
        if( synthesized != NULL
                && g_ascii_strncasecmp( chosenFileName, synthesized, strlen( synthesized) ) != 0 ) {
            g_free( selected );
            selected = g_strdup( chosenFileName );
            if( dot ) {
                if( g_strcmp0( dot+1, suffix ) == 0 ) {
                    selected[ dot - filenameLower->str ] = 0;
                }
            }
        }
        g_string_free( filenameLower, TRUE );
    } else {
        // We are asking for a name .. if already selected, give that
        // otherwise synthesize one (adding suffix)
        GDateTime *now = g_date_time_new_now_local ();
        g_free( synthesized );
        synthesized = g_date_time_format( now, pGlobal->flags.bbHP8970Bmodel ?
                    "HP8970B.%d%b%y.%H%M%S" : "HP8970A.%d%b%y.%H%M%S" );
        g_date_time_unref (now);
        if( selected != NULL )
            filename = g_strdup_printf( "%s.%s", selected, suffix );
        else
            filename = g_strdup_printf( "%s.%s", synthesized, suffix );
    }

    // This must be freed by the calling program
    return filename;
}

/*!     \brief  Retrieve plot in JSON form from a file
 *
 * Retrieve plot in JSON form from a file
 *
 * \param  filePath         pointer to the filename and path
 * \param  pGlobal          pointer to global data
 * \return 0 if OK or ERROR
 */
gint
retrievePlot( gchar *filePath, tGlobal *pGlobal ) {
    gboolean bOK = FALSE;
    GError *error = NULL;

    GFile *file = g_file_new_for_path( filePath );
    GFileInputStream *fileStream = g_file_read ( file, NULL, &error );
    g_object_unref( file );

    JsonParser *parser = json_parser_new ();
    json_parser_load_from_stream (parser, G_INPUT_STREAM( fileStream ), NULL, &error);
    JsonReader *reader = json_reader_new (json_parser_get_root (parser));

    do {
        if( json_reader_read_member (reader, "HP8970") == FALSE )
            break;

#if 0
        if( json_reader_read_member (reader, "version") == FALSE )
            break;
        const gchar *sVer = json_reader_get_string_value( reader );
        json_reader_end_member (reader);    // version
#endif

        if( json_reader_read_member (reader, "settings") == FALSE )
            break;
// settings
        if( json_reader_read_member (reader, "smoothing") == TRUE )
            pGlobal->plot.smoothingFactor = json_reader_get_int_value ( reader );
        json_reader_end_member (reader);

        if( json_reader_read_member (reader, "noiseUnits") == TRUE )
            pGlobal->plot.noiseUnits = json_reader_get_int_value ( reader );
        json_reader_end_member (reader);

        if( json_reader_read_member (reader, "noisePlotAutoscale") == TRUE )
            pGlobal->HP8970settings.switches.bAutoScaling = json_reader_get_boolean_value ( reader );
        json_reader_end_member (reader);

        if( json_reader_read_member (reader, "noisePlotScaleMin") == TRUE )
            pGlobal->HP8970settings.fixedGridNoise[ pGlobal->plot.noiseUnits ][ 0 ] = json_reader_get_double_value ( reader );
        json_reader_end_member (reader);
        if( json_reader_read_member (reader, "noisePlotScaleMax") == TRUE )
            pGlobal->HP8970settings.fixedGridNoise[ pGlobal->plot.noiseUnits ][ 1 ] = json_reader_get_double_value ( reader );
        json_reader_end_member (reader);
        if( json_reader_read_member (reader, "gainPlotScaleMin") == TRUE )
            pGlobal->HP8970settings.fixedGridGain[ 0 ] = json_reader_get_double_value ( reader );
        json_reader_end_member (reader);
        if( json_reader_read_member (reader, "gainPlotScaleMax") == TRUE )
            pGlobal->HP8970settings.fixedGridGain[ 1 ] = json_reader_get_double_value ( reader );
        json_reader_end_member (reader);

        if( json_reader_read_member (reader, "correctedNFAndGain") == TRUE )
            pGlobal->plot.flags.bDataCorrectedNFAndGain = json_reader_get_boolean_value ( reader );
        json_reader_end_member (reader);

        if( json_reader_read_member (reader, "spotFrequencyPlot") == TRUE )
            pGlobal->plot.flags.bSpotFrequencyPlot = json_reader_get_boolean_value ( reader );
        json_reader_end_member (reader);

        g_free( pGlobal->plot.sTitle );
        pGlobal->plot.sTitle = NULL;
        if( json_reader_read_member (reader, "title") == TRUE ) {
            g_free( pGlobal->plot.sTitle );
            pGlobal->plot.sTitle = g_strdup( json_reader_get_string_value ( reader ) );
        }
        json_reader_end_member (reader);

        if( json_reader_read_member (reader, "plotSpotFrequency") == TRUE )
            pGlobal->plot.spotFrequency = json_reader_get_double_value ( reader );
        json_reader_end_member (reader);

        g_free( pGlobal->plot.sDateTime );
        pGlobal->plot.sDateTime = NULL;
        if( json_reader_read_member (reader, "dateTime") == TRUE ) {
            pGlobal->plot.sDateTime = g_strdup( json_reader_get_string_value ( reader ) );
        }
        json_reader_end_member (reader);

        g_free( pGlobal->plot.sNotes );
        pGlobal->plot.sNotes = NULL;
        if( json_reader_read_member (reader, "notes") == TRUE ) {
            pGlobal->plot.sNotes = g_strdup( json_reader_get_string_value ( reader ) );
        }
        json_reader_end_member (reader);

// shadow
        if( json_reader_read_member (reader, "freqSpotMHz") == TRUE )
            pGlobal->plot.freqSpotMHz = json_reader_get_double_value ( reader );
        else
            pGlobal->plot.freqSpotMHz = 0.0;
        json_reader_end_member (reader);

        if( json_reader_read_member (reader, "freqStartMHz") == TRUE )
            pGlobal->plot.freqStartMHz = json_reader_get_double_value ( reader );
        else
            pGlobal->plot.freqStartMHz = 0.0;
        json_reader_end_member (reader);

        if( json_reader_read_member (reader, "freqStopMHz") == TRUE )
            pGlobal->plot.freqStopMHz = json_reader_get_double_value ( reader );
        else
            pGlobal->plot.freqStopMHz = 0.0;
        json_reader_end_member (reader);

        if( json_reader_read_member (reader, "freqStepCalMHz") == TRUE )
            pGlobal->plot.freqStepCalMHz = json_reader_get_double_value ( reader );
        else
            pGlobal->plot.freqStepCalMHz = 0.0;
        json_reader_end_member (reader);

        if( json_reader_read_member (reader, "freqStepSweepMHz") == TRUE )
            pGlobal->plot.freqStepSweepMHz = json_reader_get_double_value ( reader );
        else
            pGlobal->plot.freqStepSweepMHz = 0.0;
        json_reader_end_member (reader);

        if( json_reader_read_member (reader, "mode") == TRUE )
            pGlobal->plot.mode = json_reader_get_int_value ( reader );
        else
            pGlobal->plot.mode = eMode1_0;
        json_reader_end_member (reader);

        if( json_reader_read_member (reader, "extLOfreqIF") == TRUE )
            pGlobal->plot.extLOfreqIF = json_reader_get_int_value ( reader );
        else
            pGlobal->plot.extLOfreqIF = 0;
        json_reader_end_member (reader);

        if( json_reader_read_member (reader, "extLOfreqLO") == TRUE )
            pGlobal->plot.extLOfreqLO = json_reader_get_int_value ( reader );
        else
            pGlobal->plot.extLOfreqLO = 0;
        json_reader_end_member (reader);

        if( json_reader_read_member (reader, "settlingTime_ms") == TRUE )
            pGlobal->plot.settlingTime_ms = json_reader_get_int_value ( reader );
        else
            pGlobal->plot.settlingTime_ms = 0;
        json_reader_end_member (reader);

        if( json_reader_read_member (reader, "sExtLOsetup") == TRUE ) {
            g_free( pGlobal->plot.sExtLOsetup );
            pGlobal->plot.sExtLOsetup = g_strdup( json_reader_get_string_value ( reader ) );
        }
        json_reader_end_member (reader);

        if( json_reader_read_member (reader, "sExtLOsetFreq") == TRUE ) {
            g_free( pGlobal->plot.sExtLOsetFreq );
            pGlobal->plot.sExtLOsetFreq = g_strdup( json_reader_get_string_value ( reader ) );
        }
        json_reader_end_member (reader);

        if( json_reader_read_member (reader, "extLOsideband") == TRUE )
            pGlobal->plot.extLOsideband = json_reader_get_int_value ( reader );
        else
            pGlobal->plot.extLOsideband = eDSB;
        json_reader_end_member (reader);

        if( json_reader_read_member (reader, "lossBeforeDUT") == TRUE )
            pGlobal->plot.lossBeforeDUT = json_reader_get_double_value ( reader );
        else
            pGlobal->plot.lossBeforeDUT = 0.0;
        json_reader_end_member (reader);

        if( json_reader_read_member (reader, "lossAfterDUT") == TRUE )
            pGlobal->plot.lossAfterDUT = json_reader_get_int_value ( reader );
        else
            pGlobal->plot.lossAfterDUT = 0.0;
        json_reader_end_member (reader);

        if( json_reader_read_member (reader, "lossTemp") == TRUE )
            pGlobal->plot.lossTemp = json_reader_get_double_value ( reader );
        else
            pGlobal->plot.lossTemp = 0.0;
        json_reader_end_member (reader);

        if( json_reader_read_member (reader, "coldTemp") == TRUE )
            pGlobal->plot.coldTemp = json_reader_get_double_value ( reader );
        else
            pGlobal->plot.coldTemp = DEFAULT_COLD_T;
        json_reader_end_member (reader);

        if( json_reader_read_member (reader, "lossCompensationOn") == TRUE )
            pGlobal->plot.flags.bLossCompensation = json_reader_get_boolean_value ( reader );
        else
            pGlobal->plot.flags.bLossCompensation = 0;
        json_reader_end_member (reader);

// end settings
        json_reader_end_member (reader);    // settings

        if( json_reader_read_member (reader, "points")  ) {

            gint nPoints = json_reader_count_elements( reader );
            initCircularBuffer( &pGlobal->plot.measurementBuffer, nPoints+1, pGlobal->plot.spotFrequency ? eTimeAbscissa : eFreqAbscissa );

            for( int i=0; i < nPoints; i++ ) {
                // point array 1 of N
                // move to element i (array with four elements)
                tNoiseAndGain measurement;
                json_reader_read_element (reader, i);

                // frequency
                json_reader_read_element (reader, 0);

                if( pGlobal->plot.flags.bSpotFrequencyPlot )
                    measurement.abscissa.time = json_reader_get_int_value( reader );
                else
                    measurement.abscissa.freq = json_reader_get_double_value( reader );

                json_reader_end_element (reader);

                // gain
                json_reader_read_element (reader, 1);
                measurement.gain = json_reader_get_double_value( reader );
                json_reader_end_element (reader);

                // noise
                json_reader_read_element (reader, 2);
                measurement.noise = json_reader_get_double_value( reader );
                json_reader_end_element (reader);

                // flags
                json_reader_read_element (reader, 3);
                measurement.flags.all = (guint32)json_reader_get_int_value( reader );
                json_reader_end_element (reader);
                addItemToCircularBuffer( &pGlobal->plot.measurementBuffer, &measurement, TRUE );
                // end of point
                json_reader_end_element (reader);
            }
            json_reader_end_member (reader);    // points
        }


        json_reader_end_member (reader);    // HP8970
        bOK = TRUE;
    } while FALSE;

    g_object_unref( parser );
    g_object_unref( reader );
    g_object_unref( fileStream );

    // we will display 60 seconds * the smoothing factor
    if( pGlobal->plot.flags.bSpotFrequencyPlot )
        pGlobal->plot.measurementBuffer.idxTimeBeforeTail = findTimeDeltaInCircularBuffer(&pGlobal->plot.measurementBuffer,
                                                                       TIME_PLOT_LENGTH * pGlobal->plot.smoothingFactor  );
    else
        pGlobal->plot.measurementBuffer.idxTimeBeforeTail = 0;

    gtk_widget_set_sensitive( pGlobal->widgets[ eW_btn_CSV ], pGlobal->plot.flags.bValidNoiseData );

    return bOK ? 0 : ERROR;
}

/*!     \brief  Save plot in JSON form to a file
 *
 * Save plot in JSON form to a file
 *
 * \param  filePath         pointer to the filename and path
 * \param  pGlobal          pointer to global data
 * \return 0 if OK or ERROR
 */
gint
savePlot( gchar *filePath, tGlobal *pGlobal ) {
    gboolean bOK = TRUE;
    GError *error = NULL;

    JsonBuilder *builder = json_builder_new ();

    json_builder_begin_object (builder);    //start HP8970

    json_builder_set_member_name (builder, "HP8970");
    json_builder_begin_object (builder);    //start HP8970

    json_builder_set_member_name (builder, "version");
    json_builder_add_string_value ( builder, VERSION );

        json_builder_set_member_name (builder, "settings");
        json_builder_begin_object( builder );   // begin settings
            json_builder_set_member_name (builder, "smoothing");
            json_builder_add_int_value ( builder, pGlobal->plot.smoothingFactor );
            json_builder_set_member_name (builder, "noiseUnits");
            json_builder_add_int_value ( builder, pGlobal->plot.noiseUnits );

            json_builder_set_member_name (builder, "noisePlotAutoscale");
            json_builder_add_boolean_value ( builder, pGlobal->HP8970settings.switches.bAutoScaling );
            json_builder_set_member_name (builder, "noisePlotScaleMin");
            json_builder_add_double_value ( builder, pGlobal->HP8970settings.fixedGridNoise[ pGlobal->plot.noiseUnits ][ 0 ] );
            json_builder_set_member_name (builder, "noisePlotScaleMax");
            json_builder_add_double_value ( builder, pGlobal->HP8970settings.fixedGridNoise[ pGlobal->plot.noiseUnits ][ 1 ] );
            json_builder_set_member_name (builder, "gainPlotScaleMin");
            json_builder_add_double_value ( builder, pGlobal->HP8970settings.fixedGridGain[ 0 ] );
            json_builder_set_member_name (builder, "gainPlotScaleMax");
            json_builder_add_double_value ( builder, pGlobal->HP8970settings.fixedGridGain[ 1 ] );

            json_builder_set_member_name (builder, "correctedNFAndGain");
            json_builder_add_boolean_value ( builder, pGlobal->plot.flags.bDataCorrectedNFAndGain );
            json_builder_set_member_name (builder, "spotFrequencyPlot");
            json_builder_add_boolean_value ( builder, pGlobal->plot.flags.bSpotFrequencyPlot );
            if( pGlobal->plot.sTitle ) {
                json_builder_set_member_name (builder, "title");
                json_builder_add_string_value ( builder, pGlobal->plot.sTitle );
            }
            json_builder_set_member_name (builder, "plotSpotFrequency");
            json_builder_add_double_value ( builder, pGlobal->plot.spotFrequency );

            if( pGlobal->plot.sDateTime ) {
                json_builder_set_member_name (builder, "dateTime");
                json_builder_add_string_value ( builder, pGlobal->plot.sDateTime );
            }
            if( pGlobal->plot.sNotes ) {
                json_builder_set_member_name (builder, "notes");
                json_builder_add_string_value ( builder, pGlobal->plot.sNotes );
            }

            // Save current settings (not needed for plot)
            json_builder_set_member_name (builder, "freqSpotMHz");
            json_builder_add_double_value ( builder, pGlobal->plot.freqSpotMHz );
            json_builder_set_member_name (builder, "freqStartMHz");
            json_builder_add_double_value ( builder, pGlobal->plot.freqStartMHz );
            json_builder_set_member_name (builder, "freqStopMHz");
            json_builder_add_double_value ( builder, pGlobal->plot.freqStopMHz );
            json_builder_set_member_name (builder, "freqStepCalMHz");
            json_builder_add_double_value ( builder, pGlobal->plot.freqStepCalMHz );
            json_builder_set_member_name (builder, "freqStepSweepMHz");
            json_builder_add_double_value ( builder, pGlobal->plot.freqStepSweepMHz );

            json_builder_set_member_name (builder, "mode");
            json_builder_add_int_value ( builder, pGlobal->plot.mode );
            json_builder_set_member_name (builder, "extLOfreqIF");
            json_builder_add_int_value ( builder, pGlobal->plot.extLOfreqIF );
            json_builder_set_member_name (builder, "extLOfreqLO");
            json_builder_add_int_value ( builder, pGlobal->plot.extLOfreqLO );
            json_builder_set_member_name (builder, "settlingTime_ms");
            json_builder_add_int_value ( builder, pGlobal->plot.settlingTime_ms );

            if( pGlobal->plot.sExtLOsetup ) {
                json_builder_set_member_name (builder, "sExtLOsetup");
                json_builder_add_string_value ( builder, pGlobal->plot.sExtLOsetup );
            }
            if( pGlobal->plot.sExtLOsetFreq ) {
                json_builder_set_member_name (builder, "sExtLOsetFreq");
                json_builder_add_string_value ( builder, pGlobal->plot.sExtLOsetFreq );
            }
            json_builder_set_member_name (builder, "extLOsideband");
            json_builder_add_int_value ( builder, pGlobal->plot.extLOsideband );

            json_builder_set_member_name (builder, "lossBeforeDUT");
            json_builder_add_double_value ( builder, pGlobal->plot.lossBeforeDUT );
            json_builder_set_member_name (builder, "lossAfterDUT");
            json_builder_add_double_value ( builder, pGlobal->plot.lossAfterDUT );
            json_builder_set_member_name (builder, "lossTemp");
            json_builder_add_double_value ( builder, pGlobal->plot.lossTemp );
            json_builder_set_member_name (builder, "coldTemp");
            json_builder_add_double_value ( builder, pGlobal->plot.coldTemp );

            json_builder_set_member_name (builder, "lossCompensationOn");
            json_builder_add_boolean_value ( builder, pGlobal->plot.flags.bLossCompensation );

        json_builder_end_object( builder );     // end settings



        if( pGlobal->plot.flags.bValidNoiseData ) {
            json_builder_set_member_name (builder, "points");
            json_builder_begin_array(builder);      // begin points array
            gint  nPoints = nItemsInCircularBuffer( &pGlobal->plot.measurementBuffer );
            for( int i=0; i < nPoints; i++ ) {
                tNoiseAndGain *pMeasurement = getItemFromCircularBuffer( &pGlobal->plot.measurementBuffer, i );
                json_builder_begin_array(builder);
                if( pGlobal->plot.flags.bSpotFrequencyPlot )
                    json_builder_add_int_value ( builder, pMeasurement->abscissa.time );
                else
                    json_builder_add_double_value ( builder, pMeasurement->abscissa.freq );
                json_builder_add_double_value ( builder, pMeasurement->gain );
                json_builder_add_double_value ( builder, pMeasurement->noise );
                json_builder_add_int_value ( builder, pMeasurement->flags.all );
                json_builder_end_array(builder);
            }
            json_builder_end_array(builder);        // end points array
        }

    json_builder_end_object (builder);   //end HP8970
    json_builder_end_object (builder);

    JsonGenerator *gen = json_generator_new ();
    JsonNode * root = json_builder_get_root (builder);
    json_generator_set_root (gen, root);

    GFile *file = g_file_new_for_path( filePath );
    GFileOutputStream *fileStream = g_file_replace ( file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error );
    g_object_unref( file );
    if( error != NULL ) {
        bOK = FALSE;
        g_clear_error (&error);
    } else {
    json_generator_to_stream( gen, G_OUTPUT_STREAM( fileStream ), NULL, &error );
        if( error != NULL ) {
            bOK = FALSE;
            g_clear_error (&error);
        }
    }

    g_object_unref( fileStream );
    // gchar *str = json_generator_to_data (gen, NULL);

    /* Print JSON string */
    // g_print("%s\n", str);

    json_node_free (root);
    g_object_unref (gen);
    g_object_unref (builder);

    return bOK ? 0 : ERROR;
}

static gchar *sSuggestedHPGLfilename = NULL;

/*!     \brief  Callback when saving file from system file selection dialog
 *
 * Callback when saving file from system file selection dialog
 *
 * \param  source_object     GtkFileDialog object
 * \param  res               result of opening file for write
 * \param  gpGlobal          pointer to global data
 */
// Call back when file is selected
static void
CB_JSONsave( GObject *source_object, GAsyncResult *res, gpointer gpGlobal ) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG (source_object);
    tGlobal *pGlobal = (tGlobal *)gpGlobal;

    GFile *file;
    GError *err = NULL;
    GtkAlertDialog *alert_dialog;

    if (((file = gtk_file_dialog_save_finish (dialog, res, &err)) != NULL) ) {
        gchar *sChosenFilename = g_file_get_path( file );
        gchar *selectedFileBasename =  g_file_get_basename( file );

        // If there is no plot, copy the settings and save that with the NULL plot
        // so that the settings will be saved.
        if( pGlobal->plot.flags.bValidNoiseData == FALSE && pGlobal->plot.flags.bValidGainData == FALSE )
            snapshotSettings( pGlobal );
        if( savePlot( sChosenFilename, pGlobal  ) != 0 )  {
            alert_dialog = gtk_alert_dialog_new ("Cannot open file for writing:\n%s", sChosenFilename);
            gtk_alert_dialog_show (alert_dialog, NULL);
            g_object_unref (alert_dialog);
        }

        suggestFilename( pGlobal, selectedFileBasename, "json" );
        g_free( sSuggestedHPGLfilename );

        GFile *dir = g_file_get_parent( file );
        gchar *sChosenDirectory = g_file_get_path( dir );
        g_free( pGlobal->sLastDirectory );
        pGlobal->sLastDirectory = sChosenDirectory;

        g_object_unref( dir );
        g_object_unref( file );
        g_free( sChosenFilename );
    } else {
        /*
        alert_dialog = gtk_alert_dialog_new ("%s", err->message);   // Dismissed by user
        gtk_alert_dialog_show (alert_dialog, GTK_WINDOW (win));
        g_object_unref (alert_dialog);
        g_clear_error (&err);
        */
   }
}

/*!     \brief  Callback for save JSON plot button
 *
 * Callback for save JSON plot button
 *
 * \param  wBtnRecallJSONplot   GtkFileDialog object
 * \param  user_data            user data (unused)
 */
void
CB_btn_SaveJSON ( GtkButton* wBtnSaveJSONplot, gpointer gpRightClick ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wBtnSaveJSONplot ), "data");

    GtkFileDialog *fileDialogSave = gtk_file_dialog_new ();
    GtkWidget *win = gtk_widget_get_ancestor (GTK_WIDGET (wBtnSaveJSONplot), GTK_TYPE_WINDOW);
    GDateTime *now = g_date_time_new_now_local ();

    // If this was a right click, copy over the settings to the plot and exit
    if( GPOINTER_TO_INT( gpRightClick ) == TRUE ) {
        snapshotSettings( pGlobal );
        return;
    }

    g_autoptr (GListModel) filters = (GListModel *)g_list_store_new (GTK_TYPE_FILE_FILTER);
    g_autoptr (GtkFileFilter) filter = NULL;
    filter = gtk_file_filter_new ();
    gtk_file_filter_add_pattern (filter, "*.[Jj][Ss][Oo][Nn]");
    gtk_file_filter_set_name (filter, "JSON");

    sSuggestedHPGLfilename = g_date_time_format( now, pGlobal->flags.bbHP8970Bmodel ?
            "HP8970B.%d%b%y.%H%M%S.json" : "HP8970A.%d%b%y.%H%M%S.json" );

    g_list_store_append ( (GListStore*)filters, filter);

    // All files
    filter = gtk_file_filter_new ();
    gtk_file_filter_add_pattern (filter, "*");
    gtk_file_filter_set_name (filter, "All Files");
    g_list_store_append ( (GListStore*) filters, filter);

    gtk_file_dialog_set_filters (fileDialogSave, G_LIST_MODEL (filters));

    gchar *fn = suggestFilename( pGlobal, NULL, "json" );
    GFile *fPath = g_file_new_build_filename( pGlobal->sLastDirectory, fn, NULL );
    gtk_file_dialog_set_initial_file( fileDialogSave, fPath );

    gtk_file_dialog_save ( fileDialogSave, GTK_WINDOW (win), NULL, CB_JSONsave, pGlobal);

    g_free( fn );
    g_object_unref (fileDialogSave);
    g_object_unref( fPath );
    g_date_time_unref( now );
}

// Right click on the save JSON button
void
CB_rightClickGesture_SaveJSON (GtkGesture *gesture, int n_press, double x, double y, gpointer *wBtnSaveHPGL)
{
    CB_btn_SaveJSON ( GTK_BUTTON( wBtnSaveHPGL ), GINT_TO_POINTER(1) );
}

/*!     \brief  Callback when opening file from system file selection dialog
 *
 * Callback when opening file from system file selection dialog
 *
 * \param  source_object     GtkFileDialog object
 * \param  res               result of opening file for write
 * \param  gpGlobal          pointer to global data
 */
static void
CB_JSONopen( GObject *source_object, GAsyncResult *res, gpointer gpGlobal ) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG (source_object);
    tGlobal *pGlobal = (tGlobal *)gpGlobal;

    GFile *file;
    GError *err = NULL;
    GtkAlertDialog *alert_dialog;

    if (((file = gtk_file_dialog_open_finish (dialog, res, &err)) != NULL) ) {
        gchar *sChosenFilename = g_file_get_path( file );
        gchar *selectedFileBasename =  g_file_get_basename( file );

#define TBUF_SIZE   10000
        initCircularBuffer( &pGlobal->plot.measurementBuffer, 0, eTimeAbscissa );
        pGlobal->plot.flags.bValidGainData = FALSE;
        pGlobal->plot.flags.bValidNoiseData = FALSE;

        if( retrievePlot( sChosenFilename, pGlobal ) == 0 ) {
            gint  nPoints = nItemsInCircularBuffer( &pGlobal->plot.measurementBuffer );
            for( int i=0; i < nPoints; i++ ) {
                tNoiseAndGain *pMeasurement = getItemFromCircularBuffer( &pGlobal->plot.measurementBuffer, i );
                if( !pMeasurement->flags.each.bGainInvalid )
                    pGlobal->plot.flags.bValidGainData = TRUE;
                if( !pMeasurement->flags.each.bNoiseInvalid )
                    pGlobal->plot.flags.bValidNoiseData = TRUE;
                // Update the minimum and maximum values
                updateBoundaries( pMeasurement->abscissa.freq,  &pGlobal->plot.measurementBuffer.minAbscissa.freq,
                                  &pGlobal->plot.measurementBuffer.maxAbscissa.freq );
            }
            gtk_widget_queue_draw ( pGlobal->widgets[ eW_drawing_Plot ] );
            // This has the side effect of redrawing
            GtkEditable *wTitle = gtk_editable_get_delegate(GTK_EDITABLE( pGlobal->widgets[ eW_entry_Title ] ));
            g_signal_handlers_block_by_func( G_OBJECT( wTitle ),    CB_edit_Title,    NULL );
            gtk_editable_set_text( wTitle, pGlobal->plot.sTitle == 0 ? "" : pGlobal->plot.sTitle );
            g_signal_handlers_unblock_by_func( G_OBJECT( wTitle ),    CB_edit_Title,    NULL );

            GtkTextBuffer *wNotes = gtk_text_view_get_buffer(GTK_TEXT_VIEW( pGlobal->widgets[ eW_textView_Notes ] ));
            g_signal_handlers_block_by_func( G_OBJECT( wNotes ), G_CALLBACK( CB_notes_changed ), pGlobal);
            gtk_text_buffer_set_text( wNotes, pGlobal->plot.sNotes == NULL ? "" : pGlobal->plot.sNotes, -1 );
            g_signal_handlers_unblock_by_func( G_OBJECT( wNotes ), G_CALLBACK( CB_notes_changed ), pGlobal );

            gtk_notebook_set_current_page( pGlobal->widgets[ eW_note_Controls ], ePageNotes );

            setFixedRangePlotWidgets( pGlobal );

            // Recover settings .. read to snapshot, now copy to controls
            // Save current settings (not needed for plot)
            gboolean bExtLO = !(pGlobal->plot.mode == eMode1_0 || pGlobal->plot.mode == eMode1_4);
            if( pGlobal->plot.freqSpotMHz != 0.0 )
                pGlobal->HP8970settings.range[ bExtLO ].freqSpotMHz = pGlobal->plot.freqSpotMHz;
            if( pGlobal->plot.freqStartMHz != 0.0 )
                pGlobal->HP8970settings.range[ bExtLO ].freqStartMHz = pGlobal->plot.freqStartMHz;
            if( pGlobal->plot.freqStopMHz != 0.0 )
                pGlobal->HP8970settings.range[ bExtLO ].freqStopMHz = pGlobal->plot.freqStopMHz;
            if( pGlobal->plot.freqStepCalMHz != 0.0 )
                pGlobal->HP8970settings.range[ bExtLO ].freqStepCalMHz = pGlobal->plot.freqStepCalMHz;
            if( pGlobal->plot.freqStepSweepMHz != 0.0 )
                pGlobal->HP8970settings.range[ bExtLO ].freqStepSweepMHz = pGlobal->plot.freqStepSweepMHz;

            pGlobal->HP8970settings.mode = pGlobal->plot.mode;

            if( pGlobal->plot.extLOfreqIF != 0 )
                pGlobal->HP8970settings.extLOfreqIF = pGlobal->plot.extLOfreqIF;
            if( pGlobal->plot.extLOfreqLO != 0 )
                pGlobal->HP8970settings.extLOfreqLO = pGlobal->plot.extLOfreqLO;

            pGlobal->HP8970settings.settlingTime_ms = pGlobal->plot.settlingTime_ms;

            if( pGlobal->plot.sExtLOsetup != NULL ) {
                g_free( pGlobal->HP8970settings.sExtLOsetup );
                pGlobal->HP8970settings.sExtLOsetup = g_strdup( pGlobal->plot.sExtLOsetup );
            }
            if( pGlobal->plot.sExtLOsetFreq != NULL ) {
                g_free( pGlobal->HP8970settings.sExtLOsetFreq );
                pGlobal->HP8970settings.sExtLOsetFreq = g_strdup( pGlobal->plot.sExtLOsetFreq );
            }

            pGlobal->HP8970settings.extLOsideband = pGlobal->plot.extLOsideband;

            pGlobal->HP8970settings.lossBeforeDUT = pGlobal->plot.lossBeforeDUT;
            pGlobal->HP8970settings.lossAfterDUT  = pGlobal->plot.lossAfterDUT;
            pGlobal->HP8970settings.lossTemp  = pGlobal->plot.lossTemp;
            pGlobal->HP8970settings.coldTemp  = pGlobal->plot.coldTemp;

            pGlobal->HP8970settings.switches.bLossCompensation = pGlobal->plot.flags.bLossCompensation;

            setPageExtLOwidgets( pGlobal );
            refreshPageHP8970( pGlobal );
            refreshMainDialog( pGlobal );

            suggestFilename( pGlobal, selectedFileBasename, "json" );

        } else {
            alert_dialog = gtk_alert_dialog_new ("Cannot open file for reading:\n%s", sChosenFilename);
            gtk_alert_dialog_show (alert_dialog, NULL);
            g_object_unref (alert_dialog);
        }

        GFile *dir = g_file_get_parent( file );
        gchar *sChosenDirectory = g_file_get_path( dir );
        g_free( pGlobal->sLastDirectory );
        pGlobal->sLastDirectory = sChosenDirectory;

        g_object_unref( dir );
        g_object_unref( file );
        g_free( sChosenFilename );
    } else {
        alert_dialog = gtk_alert_dialog_new ("%s", err->message);
         // gtk_alert_dialog_show (alert_dialog, GTK_WINDOW (win));
        g_object_unref (alert_dialog);
        g_clear_error (&err);
   }
}

/*!     \brief  Callback for restore JSON plot button
 *
 * Callback for restore JSON plot button
 *
 * \param  wBtnRecallJSONplot   GtkFileDialog object
 * \param  user_data            user data (unused)
 */
void
CB_btn_RestoreJSON ( GtkButton* wBtnRecallJSONplot, gpointer user_data ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wBtnRecallJSONplot ), "data");

    pGlobal->flags.bPreviewModeDiagram = FALSE;

    GtkFileDialog *fileDialogRecall = gtk_file_dialog_new ();
    GtkWidget *win = gtk_widget_get_ancestor (GTK_WIDGET (wBtnRecallJSONplot), GTK_TYPE_WINDOW);
    GDateTime *now = g_date_time_new_now_local ();

    g_autoptr (GListModel) filters = (GListModel *)g_list_store_new (GTK_TYPE_FILE_FILTER);
    g_autoptr (GtkFileFilter) filter = NULL;
    filter = gtk_file_filter_new ();
    gtk_file_filter_add_pattern (filter, "*.[Jj][Ss][Oo][Nn]");
    gtk_file_filter_set_name (filter, "JSON");
    g_list_store_append ( (GListStore*)filters, filter);

    // All files
    filter = gtk_file_filter_new ();
    gtk_file_filter_add_pattern (filter, "*");
    gtk_file_filter_set_name (filter, "All Files");
    g_list_store_append ( (GListStore*) filters, filter);

    gtk_file_dialog_set_filters (fileDialogRecall, G_LIST_MODEL (filters));

    gchar *fn = suggestFilename( pGlobal, NULL, "json" );
    GFile *fPath = g_file_new_build_filename( pGlobal->sLastDirectory, fn, NULL );
    gtk_file_dialog_set_initial_file( fileDialogRecall, fPath );

    gtk_file_dialog_open ( fileDialogRecall, GTK_WINDOW (win), NULL, CB_JSONopen, pGlobal);

    g_free( fn );
    g_object_unref (fileDialogRecall);
    g_object_unref( fPath );
    g_date_time_unref( now );
}


