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
#include <gio/gio.h>
#include <cairo/cairo.h>
#include <cairo/cairo-pdf.h>
#include <cairo/cairo-svg.h>

#include "HP8970.h"
#include "messageEvent.h"
#include <math.h>


#define PNG_WIDTH       3300

tPaperDimensions paperDimensions[ eNumPaperSizes ] = {
        {595,  842,  7.2},  // A4
        {612,  792,  7.2},  // Letter
        {842, 1190, 10.0},  // A3
        {792, 1224, 10.0}   // Tabloid
};


/*!     \brief  Write the PDF image to a file
 *
 * Determine the filename to use for the PNG file and
 * write image(s) of plot using the already retrieved data to the file.
 *
 * \param  wButton  file pointer to the open, writable file
 * \param  pGlobal  pointer to data
 */

enum eFileType { ePDF, eSVG, ePNG, eCSV };

static gchar *sSuggestedFilename = NULL;
// Call back when file is selected
static void
plotAndSaveFile( GObject *source_object, GAsyncResult *res, gpointer gpGlobal, enum eFileType fileType ) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG (source_object);
    tGlobal *pGlobal = (tGlobal *)gpGlobal;

    GFile *file;
    GError *err = NULL;
    GtkAlertDialog *alert_dialog;
    gdouble width, height;

    cairo_t *cr;
    cairo_surface_t *cs;

    if (((file = gtk_file_dialog_save_finish (dialog, res, &err)) != NULL) ) {
        gchar *sChosenFilename = g_file_get_path( file );
        gchar *selectedFileBasename =  g_file_get_basename( file );
        switch( fileType ) {
        case ePDF:
        default:
            width  = paperDimensions[pGlobal->PDFpaperSize].width;
            height = paperDimensions[pGlobal->PDFpaperSize].height;
            cs = cairo_pdf_surface_create ( sChosenFilename, width, height );
            cairo_pdf_surface_set_metadata (cs, CAIRO_PDF_METADATA_CREATOR, "Linux HP8970 Noise Figure Meter");
            suggestFilename( pGlobal, selectedFileBasename, "pdf" );
            break;
        case eSVG:
            width  = paperDimensions[pGlobal->PDFpaperSize].width;
            height = paperDimensions[pGlobal->PDFpaperSize].height;
            cs = cairo_svg_surface_create ( sChosenFilename, width, height );
            suggestFilename( pGlobal, selectedFileBasename, "svg" );
            break;
        case ePNG:
            width  = PNG_WIDTH;
            height = PNG_WIDTH / sqrt( 2.0 );
            cs = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
            suggestFilename( pGlobal, selectedFileBasename, "png" );
            break;
        }
        cr = cairo_create (cs);

        g_free( selectedFileBasename );

        // Letter and Tabloid size are not in the ratio of our data ( height = width / sqrt( 2 ) )
        // we need to adjust
        if( fileType != ePNG )  {   // we know PNG is the right aspect ratio
            // aspect ratio is sqrt( 2 )
            if( (height / width) / sqrt( 2.0 ) > 1.01 ) {// this should leave A4 and A3 untouched
                cairo_translate( cr, width - (height * sqrt(2.0)) / 2.0, 0.0  );
                width = height * sqrt( 2.0 );
            } else if( (height / width) / sqrt( 2.0 ) < 0.99 ) {   // wider
                cairo_translate( cr, 0.0, (height - width / sqrt( 2.0 )) / 2.0  );
                height = width / sqrt( 2.0 );
            }
        }

        cairo_save( cr ); {
            plotNoiseFigureAndGain( cr, width, height, pGlobal, TRUE);
        } cairo_restore( cr );

        cairo_show_page( cr );

        if( fileType  == ePNG )
            cairo_surface_write_to_png (cs, sChosenFilename );

        cairo_surface_destroy ( cs );
        cairo_destroy( cr );

        GFile *dir = g_file_get_parent( file );
        gchar *sChosenDirectory = g_file_get_path( dir );
        g_free( pGlobal->sLastDirectory );
        pGlobal->sLastDirectory = sChosenDirectory;

        g_object_unref( dir );
        g_object_unref( file );
        g_free( sChosenFilename );
   }

   if (err) {
     alert_dialog = gtk_alert_dialog_new ("%s", err->message);
     // gtk_alert_dialog_show (alert_dialog, GTK_WINDOW (win));
     g_object_unref (alert_dialog);
     g_clear_error (&err);
   }
    g_free( sSuggestedFilename );
}

// Call back when file is selected
static void
CB_PDFsave( GObject *source_object, GAsyncResult *res, gpointer gpGlobal ) {
    plotAndSaveFile( source_object, res, gpGlobal, ePDF );
}

// Call back when file is selected
static void
CB_SVGsave( GObject *source_object, GAsyncResult *res, gpointer gpGlobal ) {
    plotAndSaveFile( source_object, res, gpGlobal, eSVG );
}

// Call back when file is selected
static void
CB_PNGsave( GObject *source_object, GAsyncResult *res, gpointer gpGlobal ) {
    plotAndSaveFile( source_object, res, gpGlobal, ePNG );
}

// Call back when file is selected
static void
CB_CSVsave( GObject *source_object, GAsyncResult *res, gpointer gpGlobal ) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG (source_object);
    tGlobal *pGlobal = (tGlobal *)gpGlobal;

    GFile *file;
    GError *err = NULL;
    GtkAlertDialog *alert_dialog;
    gint nMeasurements = nItemsInCircularBuffer( &pGlobal->plot.measurementBuffer );
    gboolean bSpotFreqency = pGlobal->plot.flags.bSpotFrequencyPlot;
    tNoiseAndGain *pMeasurement;

    gchar *sFreqOrTime, *sNoise;
    gchar sNoiseU[ SHORT_STRING ];

    if (((file = gtk_file_dialog_save_finish (dialog, res, &err)) != NULL) ) {
        gchar *sChosenFilename = g_file_get_path( file );
        gchar *selectedFileBasename =  g_file_get_basename( file );

        suggestFilename( pGlobal, selectedFileBasename, "pdf" );
        g_free( selectedFileBasename );


// Open file and save....

        GFileOutputStream *oStream = g_file_replace(
                    file,NULL,FALSE, G_FILE_CREATE_NONE, NULL,NULL);

        if( bSpotFreqency )
            sFreqOrTime = "Time (s) since 1/1/1970";
        else
            sFreqOrTime = "Frequency (MHz)";

        sNoise = sNoiseLabel[ pGlobal->plot.noiseUnits ];
        if( sNoiseUnits[ pGlobal->plot.noiseUnits ] != 0 ) {
            g_snprintf( sNoiseU, SHORT_STRING, " (%s)", sNoiseUnits[ pGlobal->plot.noiseUnits ] );
        } else {
            sNoiseU[ 0 ] = 0;
        }

        g_output_stream_printf( G_OUTPUT_STREAM( oStream ), NULL, NULL, &err, "%s,%s%s,Gain (dB)\n",
                                sFreqOrTime, sNoise, sNoiseU );

        for( int i=0; i < nMeasurements; i++ ) {
            pMeasurement = getItemFromCircularBuffer( &pGlobal->plot.measurementBuffer, i );
            if( bSpotFreqency ) {
                g_output_stream_printf( G_OUTPUT_STREAM( oStream ), NULL, NULL, &err, "%.3lf,%g,%g\n",
                                        ((gdouble)pMeasurement->abscissa.time) / 1000.0, pMeasurement->noise,  pMeasurement->gain );
            } else {
                g_output_stream_printf( G_OUTPUT_STREAM( oStream ), NULL, NULL, &err, "%g,%g,%g\n",
                                        pMeasurement->abscissa.freq / MHz(1.0), pMeasurement->noise,  pMeasurement->gain );
            }
        }

        g_output_stream_close(G_OUTPUT_STREAM(oStream),NULL,NULL);
        g_object_unref(oStream);



            postInfo( "CSV saved" );



        GFile *dir = g_file_get_parent( file );
        gchar *sChosenDirectory = g_file_get_path( dir );
        g_free( pGlobal->sLastDirectory );
        pGlobal->sLastDirectory = sChosenDirectory;

        g_object_unref( dir );
        g_object_unref( file );
        g_free( sChosenFilename );
   }

   if (err) {
     alert_dialog = gtk_alert_dialog_new ("%s", err->message);
     // gtk_alert_dialog_show (alert_dialog, GTK_WINDOW (win));
     g_object_unref (alert_dialog);
     g_clear_error (&err);
   }
    g_free( sSuggestedFilename );
}


void
presentFileSaveDialog ( GtkButton *wBtn, gpointer user_data, enum eFileType fileType ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wBtn), "data");

    GtkFileDialog *fileDialogSave = gtk_file_dialog_new ();
    GtkWidget *win = gtk_widget_get_ancestor (GTK_WIDGET (wBtn), GTK_TYPE_WINDOW);

    GDateTime *now = g_date_time_new_now_local ();
    gchar *pUserFileName = NULL;

    g_autoptr (GListModel) filters = (GListModel *)g_list_store_new (GTK_TYPE_FILE_FILTER);

    g_autoptr (GtkFileFilter) filter = NULL;

    // PDF files
    filter = gtk_file_filter_new ();
    switch( fileType ) {
    case ePDF:
        gtk_file_filter_add_mime_type (filter, "application/pdf");
        gtk_file_filter_set_name (filter, "PDF");
        pUserFileName = suggestFilename( pGlobal, NULL, "pdf" );
        break;
    case eSVG:
        gtk_file_filter_add_mime_type (filter, "image/svg+xml");
        gtk_file_filter_set_name (filter, "SVG");
        pUserFileName = suggestFilename( pGlobal, NULL, "svg" );
        break;
    case ePNG:
        gtk_file_filter_add_mime_type (filter, "image/png");
        gtk_file_filter_set_name (filter, "PNG");
        pUserFileName = suggestFilename( pGlobal, NULL, "png" );
        break;
    case eCSV:
        gtk_file_filter_add_mime_type (filter, "text/csv");
        gtk_file_filter_set_name (filter, "CSV");
        pUserFileName = suggestFilename( pGlobal, NULL, "csv" );
        break;
    default: break;
    }
    g_list_store_append ( (GListStore*)filters, filter);

    // All files
    filter = gtk_file_filter_new ();
    gtk_file_filter_add_pattern (filter, "*");
    gtk_file_filter_set_name (filter, "All Files");
    g_list_store_append ( (GListStore*) filters, filter);


    gtk_file_dialog_set_filters (fileDialogSave, G_LIST_MODEL (filters));

    GFile *fPath = g_file_new_build_filename( pGlobal->sLastDirectory, pUserFileName, NULL );
    gtk_file_dialog_set_initial_file( fileDialogSave, fPath );

    switch( fileType ) {
    case ePDF:
        gtk_file_dialog_save ( fileDialogSave, GTK_WINDOW (win), NULL, CB_PDFsave, pGlobal);
        break;
    case eSVG:
        gtk_file_dialog_save ( fileDialogSave, GTK_WINDOW (win), NULL, CB_SVGsave, pGlobal);
        break;
    case ePNG:
        gtk_file_dialog_save ( fileDialogSave, GTK_WINDOW (win), NULL, CB_PNGsave, pGlobal);
        break;
    case eCSV:
        gtk_file_dialog_save ( fileDialogSave, GTK_WINDOW (win), NULL, CB_CSVsave, pGlobal);
        break;
    default:
        break;
    }

    g_free( pUserFileName );
    g_object_unref (fileDialogSave);
    g_object_unref( fPath );
    g_date_time_unref( now );
}

// Call back for button on main dialog
void
CB_btn_PDF ( GtkButton *wBtnPDF, gpointer user_data ) {
    presentFileSaveDialog ( wBtnPDF, user_data, ePDF );
}


void
CB_btn_SVG ( GtkButton *wBtbSVG, gpointer user_data ) {
    presentFileSaveDialog ( wBtbSVG, user_data, eSVG );
}

void
CB_btn_PNG ( GtkButton *wBtnPNG, gpointer user_data ) {
    presentFileSaveDialog ( wBtnPNG, user_data, ePNG );
}

void
CB_btn_CSV ( GtkButton *wBtnCSV, gpointer user_data ) {
    presentFileSaveDialog ( wBtnCSV, user_data, eCSV );
}

