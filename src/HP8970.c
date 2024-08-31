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
/*
     use: 'glib-compile-resources --generate-source HP8970-GTK4.xml' to generate resource.c
          'sudo gtk-update-icon-cache  --ignore-theme-index /usr/local/share/icons/hicolor' to update the icon cache after icons copied
          'sudo cp us.heterodyne.hp8970.gschema.xml /usr/local/share/glib-2.0/schemas'
          'sudo glib-compile-schemas /usr/local/share/glib-2.0/schemas' for the gsettings schema

     HP8665A HPIB strings:
         Setup:             AMPL:SOUR 10DBM;STAT ON;:FREQ:MODE CW;:FM:STAT OFF;:AM:STAT OFF;
         Frequency Control: FREQ:CW %.0lfMHZ;
     */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <glib-2.0/glib.h>
#include <gpib/ib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <locale.h>
#include <time.h>
#include <errno.h>
#include <HP8970.h>
#include "messageEvent.h"
#include "GTKcallbacks.h"

tGlobal globalData =
    { 0 };

static gint optDebug = 0;
static gboolean bOptQuiet = 0;
static gint bOptDoNotEnableSystemController = 0;
static gint optDeviceID = INVALID;
static gint optControllerIndex = INVALID;
static gboolean bOptNoGPIBtimeout = 0;
static gchar **argsRemainder = NULL;

static const GOptionEntry optionEntries[] =
    {
        { "debug", 'b', 0, G_OPTION_ARG_INT, &optDebug, "Print diagnostic messages in journal (0-7)", NULL },
        { "quiet", 'q', 0, G_OPTION_ARG_NONE, &bOptQuiet, "No GUI sounds", NULL },
        { "GPIBnoSystemController", 'n', 0, G_OPTION_ARG_NONE, &bOptDoNotEnableSystemController,
                "Do not enable GPIB interface as a system controller", NULL },
        { "GPIBdeviceID", 'd', 0, G_OPTION_ARG_INT, &optDeviceID, "GPIB device ID for HPGL plotter", NULL },
        { "GPIBcontrollerIndex", 'c', 0, G_OPTION_ARG_INT, &optControllerIndex, "GPIB controller board index", NULL },
        { "noGPIBtimeout", 't', 0, G_OPTION_ARG_NONE, &bOptNoGPIBtimeout, "no GPIB timeout (for debug with HP59401A)", NULL },

        { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &argsRemainder, "", NULL },
        { NULL } };


gboolean
shrinkWrap (gpointer gpGlobal) {
    static gboolean bToggle = FALSE;
    tGlobal *pGlobal = (tGlobal*) gpGlobal;
    gtk_window_set_default_size (GTK_WINDOW(gtk_widget_get_root( pGlobal->widgets[ eW_HP8970_application ])), bToggle ? 1 : -1,
                                 bToggle ? 1 : -1);
    gtk_widget_set_size_request ( pGlobal->widgets[ eW_aspect_Plot ], 707, 500);
    bToggle = !bToggle;
    return G_SOURCE_REMOVE;
}

void
appEnter (GtkEventControllerFocus *self, gpointer gpGlobal) {
    tGlobal *pGlobal = (tGlobal*) gpGlobal;
    GtkWidget *wPlot = pGlobal->widgets[ eW_aspect_Plot ];

    gint height = gtk_widget_get_height (wPlot);
    gint width = gtk_widget_get_width (wPlot);
    g_print ("app enter: %d x %d\n", width, height);
    /*
     gtk_widget_set_size_request( pGlobal->widgets[ eW_aspect_Plot ], width, height);
     g_timeout_add ( 100, (GSourceFunc)shrinkWrap, gpGlobal );
     */
}

void
appLeave (GtkEventControllerFocus *self, gpointer gpGlobal) {
    tGlobal *pGlobal = (tGlobal*) gpGlobal;
    GtkWidget *wPlot = pGlobal->widgets[ eW_aspect_Plot ];

    gint height = gtk_widget_get_height (wPlot);
    gint width = gtk_widget_get_width (wPlot);
    if (height == 0)
        return;
    g_print ("app leave: %d x %d\n", width, height);
}



gdouble Eaton7618E_SM104[][2] = {
        {    30, 15.84 }, {   300, 15.88 },
        {  1000, 15.77 }, {  2000, 16.37 }, {  3000, 15.76 }, {  4000, 15.65 },
        {  5000, 15.67 }, {  6000, 15.42 }, {  7000, 15.75 }, {  8000, 15.46 },
        {  9000, 15.52 }, { 10000, 15.50 }, { 11000, 15.24 }, { 12000, 15.08 },
        { 13000, 15.11 }, { 14000, 14.47 }, { 15000, 14.53 }, { 16000, 15.18 },
        { 17000, 15.50 }, { 18000, 15.27 }
};

gdouble HP3463[][2] = {
        {    10, 15.00 }, {   100, 15.00 },
        {  1000, 15.00 }, {  2000, 15.00 }, {  3000, 15.00 }, {  4000, 15.00 },
        {  5000, 15.00 }, {  6000, 15.00 }, {  7000, 15.00 }, {  8000, 15.00 },
        {  9000, 15.00 }, { 10000, 15.00 }, { 11000, 15.00 }, { 12000, 15.00 },
        { 13000, 15.00 }, { 14000, 15.00 }, { 15000, 15.00 }, { 16000, 15.00 },
        { 17000, 15.00 }, { 18000, 15.00 }, { 19000, 15.00 }, { 20000, 15.00 },
        { 21000, 15.00 }, { 22000, 15.00 }, { 23000, 15.00 }, { 24000, 15.00 },
        { 25000, 15.00 }, { 26000, 15.00 }, { 26500, 15.00 }
};

gdouble maxInputFreq[ e8970_MAXmodels ] = { HP8970A_MAX_FREQ, HP8970B_MAX_FREQ, HP8970Bopt20_MAX_FREQ };
gchar *sHP89709models[] = { "HP8970A", "HP8970B", "HP8970B opt 20" };

/*!     \brief  Initialize structures
 *
 * Initialize data and structures
 *
 * \param  pGlobal      : pointer to data structure
 */

static void
initializeData (tGlobal *pGlobal) {
    gint n, i, j;
    pGlobal->sGPIBdeviceName = NULL;
    pGlobal->flags.bGPIB_UseCardNoAndPID = TRUE;

    pGlobal->GPIBdevicePID = DEFAULT_HP8970_GPIB_DEVICE_ID;
    pGlobal->GPIBcontrollerIndex = DEFAULT_GPIB_CONTROLLER_INDEX;

    for( i=0; i < eMAX_COLORS; i++ ) {
        plotElementColors[ i ] = plotElementColorsFactory[ i ];
    }

    pGlobal->HP8970settings = (tHP8970settings){
        .range[0].freqSpotMHz = HP8970A_DEFAULT_FREQ,
        .range[0].freqStartMHz = HP8970A_START_SWEEP_DEFAULT,
        .range[0].freqStopMHz = HP8970A_STOP_SWEEP_DEFAULT,
        .range[0].freqStepCalMHz = HP8970A_STEP_SWEEP_DEFAULT,
        .range[0].freqStepSweepMHz = HP8970A_STEP_SWEEP_DEFAULT,

        .range[1].freqSpotMHz = HP8970A_DEFAULT_FREQ_R2,
        .range[1].freqStartMHz = HP8970A_START_SWEEP_DEFAULT_R2,
        .range[1].freqStopMHz = HP8970A_STOP_SWEEP_DEFAULT_R2,
        .range[1].freqStepCalMHz = HP8970A_PageStep_FREQ_R2,
        .range[1].freqStepSweepMHz = HP8970A_PageStep_FREQ_R2
    };

    pGlobal->plot.measurementBuffer.maxAbscissa.freq = HP8970A_STOP_SWEEP_DEFAULT_R2 * MHz(1.0);
    pGlobal->plot.measurementBuffer.minAbscissa.freq = HP8970A_START_SWEEP_DEFAULT_R2 * MHz(1.0);
    pGlobal->plot.measurementBuffer.minNoise =  0.0;
    pGlobal->plot.measurementBuffer.maxNoise = 10.0;
    pGlobal->plot.measurementBuffer.minGain  =  0.0;
    pGlobal->plot.measurementBuffer.maxGain  = 10.0;

    bzero( pGlobal->noiseSources, sizeof( tNoiseSource ) * MAX_NOISE_SOURCES );
    n = sizeof( Eaton7618E_SM104 ) / (2 * sizeof( gdouble ));
    g_snprintf( pGlobal->noiseSources[ 0 ].name, MAX_NOISE_SOURCE_NAME_LENGTH+1, "Eaton7618E S/N 104" );
    for( j = 0; j < n; j++ ) {
        pGlobal->noiseSources[ 0 ].calibrationPoints[ j ][ 0 ] = Eaton7618E_SM104[j][0];
        pGlobal->noiseSources[ 0 ].calibrationPoints[ j ][ 1 ] = Eaton7618E_SM104[j][1];
    }

    n = sizeof( HP3463 ) / (2 * sizeof( gdouble ));
    for( i = 1; i < MAX_NOISE_SOURCES; i++  ) {
        g_snprintf( pGlobal->noiseSources[ i ].name, MAX_NOISE_SOURCE_NAME_LENGTH+1, "HP 346C %d", i );
        for( j = 0; j < n; j++ ) {
            pGlobal->noiseSources[ i ].calibrationPoints[ j ][ 0 ] = HP3463[j][0];
            pGlobal->noiseSources[ i ].calibrationPoints[ j ][ 1 ] = HP3463[j][1];
        }
    }

    pGlobal->HP8970settings.coldTemp = DEFAULT_COLD_T;
    pGlobal->HP8970settings.lossTemp = DEFAULT_COLD_T;
}


/*!     \brief  Initialize widgets
 *
 * Initialize GTK Widgets from data
 *
 * \param  pGlobal      : pointer to data structure
 */
static void
initializeWidgets (tGlobal *pGlobal) {
    // widgets related to the frequency and sweep settings on the main page
    initializeMainDialog ( pGlobal );
    initializePageGPIB   ( pGlobal );
    initializePageOptions( pGlobal );
    initializePageSource ( pGlobal );
    initializePagePlot   ( pGlobal );
    initializePageHP8970 ( pGlobal );
    initializePageExtLO  ( pGlobal );
    initializePageNotes  ( pGlobal );
}

/*!     \brief  on_activate (activate signal callback)
 *
 * Activate the main window and add it to the application(show or raise the main window)
 *
 *
 * \ingroup callbacks
 *
 * \param  app      : pointer to this GApplication
 * \param  udata    : unused
 */

static void
on_activate (GApplication *app, gpointer udata) {
    GtkBuilder *builder;
    GtkWidget *wApplicationWindow;

    tGlobal *pGlobal = (tGlobal*) udata;

    if (pGlobal->flags.bRunning) {
        // gtk_window_set_screen( GTK_WINDOW( MainWindow ),
        //                       unique_message_data_get_screen( message ) );
        gtk_widget_set_visible (
                GTK_WIDGET( gtk_widget_get_root( pGlobal->widgets[ eW_HP8970_application ])), TRUE);

        gtk_window_present (GTK_WINDOW( pGlobal->widgets[ eW_HP8970_application ] ));
        return;
    } else {
        pGlobal->flags.bRunning = TRUE;
    }

    pGlobal->widgetHashTable = g_hash_table_new (g_str_hash, g_str_equal);

    //  builder = gtk_builder_new();

    // use: 'glib-compile-resources --generate-source HP8970-GTK4.xml' to generate resource.c
    //      'sudo gtk-update-icon-cache /usr/share/icons/hicolor' to update the icon cache after icons copied
    //      'sudo cp us.heterodyne.hp8970.gschema.xml /usr/share/glib-2.0/schemas'
    //      'sudo glib-compile-schemas /usr/share/glib-2.0/schemas' for the gsettings schema

    builder = gtk_builder_new ();
    /*
     * Get the data using
     * gpointer *pGlobalData = g_object_get_data ( data, "globalData" );
     */
    GObject *dataObject = g_object_new ( G_TYPE_OBJECT, NULL);
    g_object_set_data (dataObject, "globalData", (gpointer) pGlobal);

    gtk_builder_set_current_object (builder, dataObject);
    gtk_builder_add_from_resource (builder, "/src/hp8970.ui", NULL);
    wApplicationWindow = GTK_WIDGET(gtk_builder_get_object (builder, "WID_HP8970_application"));

    // Get the widgets we will be addressing into a fast array
    buildWidgetList(pGlobal,  builder);

    // Stop a hexpand from a GtkEntry in sidebar box from causing the sidebar GtkBox itself (WID_Controls) to obtain this property.
    // The sidebar should maintain its width regardless of the size of the application window
    // See ... https://gitlab.gnome.org/GNOME/gtk/-/issues/6820
    gtk_widget_set_hexpand( GTK_WIDGET( pGlobal->widgets[ eW_Controls ] ), FALSE );

#ifdef __OPTIMIZE__
    g_timeout_add (20, (GSourceFunc) splashCreate, pGlobal);
    g_timeout_add (4000, (GSourceFunc) splashDestroy, pGlobal);
#endif

    GtkCssProvider *cssProvider = gtk_css_provider_new ();
    gtk_css_provider_load_from_resource (cssProvider, "/src/hp8970.css");
    gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                GTK_STYLE_PROVIDER(cssProvider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_object_unref (builder);

    gtk_window_set_default_icon_name ("hp8970");

    GtkColumnView *wNoiseTable = pGlobal->widgets[ eW_CV_NoiseSource ];
    createNoiseFigureColumnView (wNoiseTable, pGlobal);

    gtk_widget_set_visible (wApplicationWindow, TRUE);
    gtk_application_add_window (GTK_APPLICATION(app), GTK_WINDOW(wApplicationWindow));
    gtk_window_set_icon_name (GTK_WINDOW(wApplicationWindow), "hp8970");

    // Set the values of the widgets to match the data
    initializeWidgets (pGlobal);

    gtk_window_set_title( GTK_WINDOW( wApplicationWindow ),
                          pGlobal->flags.bbHP8970Bmodel ? "HP 8970B Noise Figure Meter" : "HP 8970A Noise Figure Meter");

    // Start GPIB thread
    pGlobal->pGThread = g_thread_new ("GPIBthread", threadGPIB, (gpointer) pGlobal);
    postDataToGPIBThread (TG_SETUP_GPIB, NULL);

    // Somehow we loose focus in something we've done.
    // We regain focus so that the keyboard will be active (F1 etc).
    gtk_widget_grab_focus( wApplicationWindow );
}


/*!     \brief  on_startup (startup signal callback)
 *
 * Setup application (get configuration and create main window (but do not show it))
 * nb: this occurs before 'activate'
 *
 * \ingroup initialize
 *
 * \param  app      : pointer to this GApplication
 * \param  udata    : unused
 */
static void
on_startup (GApplication *app, gpointer udata) {
    tGlobal *pGlobal = (tGlobal*) udata;
    gboolean bAbort = FALSE;

    LOG(G_LOG_LEVEL_INFO, "Starting");
    setenv ("IB_NO_ERROR", "1", 0);	// no noise
    logVersion ();

    initializeData ( pGlobal );
    recoverSettings( pGlobal );
    recoverConfigurations( pGlobal );

    pGlobal->flags.bNoGPIBtimeout = bOptNoGPIBtimeout;
    pGlobal->flags.bbDebug = optDebug;

    /*! We use a loop source to send data back from the
     *  GPIB threads to indicate status
     */

    pGlobal->messageQueueToMain = g_async_queue_new();
    pGlobal->messageEventSource = g_source_new( &messageEventFunctions, sizeof(GSource) );
    pGlobal->messageQueueToGPIB = g_async_queue_new();
    g_source_attach( globalData.messageEventSource, NULL );

    if (optControllerIndex != INVALID) {
        pGlobal->GPIBcontrollerIndex = optControllerIndex;
    }

    if (optDeviceID != INVALID) {
        pGlobal->GPIBdevicePID = optDeviceID;
    }

    // Initialize the settings update mutex
    g_mutex_init( &pGlobal->mUpdate );

//    pGlobal->flags.bValidGainData = FALSE;
//    pGlobal->flags.bValidNoiseData = TRUE;

    if (bAbort)
        g_application_quit (G_APPLICATION(app));
}


/*!     \brief  on_shutdown (shutdown signal callback)
 *
 * Cleanup on shutdown
 *
 * \param  app      : pointer to this GApplication
 * \param  userData : unused
 */
static void
on_shutdown (GApplication *app, gpointer userData) {
    tGlobal *pGlobal __attribute__((unused)) = (tGlobal*) userData;

    saveSettings( pGlobal );
    saveConfigurations( pGlobal );

    // cleanup
    messageEventData *messageData = g_malloc0( sizeof(messageEventData) );
    messageData->command = TG_END;
    g_async_queue_push( pGlobal->messageQueueToGPIB, messageData );

    if (pGlobal->pGThread) {
        g_thread_join (pGlobal->pGThread);
        g_thread_unref (pGlobal->pGThread);
    }

    // Destroy queue and source
    g_async_queue_unref (pGlobal->messageQueueToMain);
    g_async_queue_unref (pGlobal->messageQueueToGPIB);
    g_source_destroy (pGlobal->messageEventSource);
    g_source_unref (pGlobal->messageEventSource);

    g_mutex_clear( &pGlobal->mUpdate );

    g_free (pGlobal->sUsersJSONfilename);
    g_free (pGlobal->sUsersPDFImageFilename);
    g_free (pGlobal->sUsersPNGImageFilename);
    g_free (pGlobal->sUsersSVGImageFilename);

    g_list_free_full ( pGlobal->configurationList, freeConfigationItemContent );

    freeSVGhandles();

    LOG(G_LOG_LEVEL_INFO, "Ending");
}

/*!     \brief  Start of program
 *
 * Start of program
 *
 * \param argc	number of arguments
 * \param argv	pointer to array of arguments
 * \return		success or failure error code
 */
int
main (int argc, char *argv[]) {
    GtkApplication *app;

    GMainLoop __attribute__((unused)) *loop;

    setlocale (LC_ALL, "en_US");
    setenv ("IB_NO_ERROR", "1", 0);	// no noise from GPIB library
    g_log_set_writer_func (g_log_writer_journald, NULL, NULL);
#ifndef __OPTIMIZE__
    // When debugging always cause warnings to crash program
    g_log_set_always_fatal(G_LOG_LEVEL_CRITICAL|G_LOG_LEVEL_WARNING);
#endif
    // ensure only one instance of program runs ..
    app = gtk_application_new ("us.heterodyne.hp8970", G_APPLICATION_HANDLES_OPEN);
    g_application_add_main_option_entries (G_APPLICATION(app), optionEntries);

    g_signal_connect(app, "activate", G_CALLBACK (on_activate), (gpointer )&globalData);
    g_signal_connect(app, "startup", G_CALLBACK (on_startup ), (gpointer )&globalData);
    g_signal_connect(app, "shutdown", G_CALLBACK (on_shutdown), (gpointer )&globalData);

    gint __attribute__((unused)) status = g_application_run (G_APPLICATION(app), argc, argv);
    g_object_unref (app);

    return EXIT_SUCCESS;
}
