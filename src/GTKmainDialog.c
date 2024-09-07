
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <cairo/cairo.h>
#include <glib-2.0/glib.h>
#include "messageEvent.h"
#include "GTKcallbacks.h"
#include <math.h>
#include <ctype.h>
#include <HP8970.h>

static gboolean
CB_KeyPressed (GObject *dataObject, guint keyval, guint keycode, GdkModifierType state, GtkEventControllerKey *event_controller) {

    tGlobal *pGlobal = (tGlobal*) g_object_get_data (dataObject, "data");

//  if (state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_ALT_MASK))
//      return FALSE;

    /*       ESC: abort - stop reading / writing to HP8970
     * shift ESC: abort, then reinitialize the GPIB devices
     *  ctrl ESC: abort & send a GPIB clear to the HP8970
     *   alt ESC: clear any plot
     *
     *        F1: show help screen
     *
     *        F2: send all settings to HP8970 (useful if HP8970 has been reset)
     *
     *       F12: enlarge to max screen height
     * shift F12: make default size
    */
    switch (keyval)  {
        case GDK_KEY_Escape:
            switch (state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SUPER_MASK))
                {
                case GDK_SHIFT_MASK:
                    postDataToGPIBThread (TG_ABORT, NULL);
                    postDataToGPIBThread (TG_REINITIALIZE_GPIB, NULL);
                    break;
                case GDK_CONTROL_MASK:
                    postDataToGPIBThread (TG_ABORT_CLEAR, NULL);
                    break;
                case GDK_ALT_MASK:
                    pGlobal->plot.measurementBuffer.flags.bValidNoiseData = FALSE;
                    pGlobal->plot.measurementBuffer.flags.bValidGainData  = FALSE;
                    initCircularBuffer( &pGlobal->plot.measurementBuffer, 0, eTimeAbscissa );
                    gtk_text_buffer_set_text( gtk_text_view_get_buffer(GTK_TEXT_VIEW( pGlobal->widgets[ eW_textView_Notes ] )), "", -1 );
                    gtk_editable_set_text( GTK_EDITABLE( pGlobal->widgets[ eW_entry_Title ] ), "" );
                    gtk_widget_queue_draw ( pGlobal->widgets[ eW_drawing_Plot ] );
                    break;
                case GDK_SUPER_MASK:
                    break;
                case 0:
                    postDataToGPIBThread (TG_ABORT, NULL);
                    break;
                }
            break;
        case GDK_KEY_F1:
            switch (state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SUPER_MASK))
                {
                case GDK_SHIFT_MASK:
                    break;
                case GDK_CONTROL_MASK:
                    break;
                case GDK_ALT_MASK:
                    break;
                case GDK_SUPER_MASK:
                    break;
                case 0:
                    GtkUriLauncher *launcher = gtk_uri_launcher_new ("help:hp8970");
                    gtk_uri_launcher_launch (launcher, GTK_WINDOW(gtk_widget_get_root( pGlobal->widgets[ eW_HP8970_application ] )), NULL, NULL, NULL);
                    g_object_unref (launcher);
                    break;
                }
            break;
        case GDK_KEY_F2:
            switch (state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SUPER_MASK))
                {
                case GDK_SHIFT_MASK:
                    break;
                case GDK_CONTROL_MASK:
                    break;
                case GDK_ALT_MASK:
                    break;
                case GDK_SUPER_MASK:
                    break;
                case 0:
                    pGlobal->HP8970settings.updateFlags.all = ALL_FUNCTIONS;
                    postDataToGPIBThread (TG_SEND_SETTINGS_to_HP8970, NULL);
                    break;
                }
            break;
            case GDK_KEY_F3:
                switch (state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SUPER_MASK))
                    {
                    case GDK_SHIFT_MASK:
                        break;
                    case GDK_CONTROL_MASK:
                        break;
                    case GDK_ALT_MASK:
                        break;
                    case GDK_SUPER_MASK:
                        break;
                    case 0:
                        break;
                    }
                break;
        case GDK_KEY_F11:
            switch (state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SUPER_MASK))
                {
                case GDK_SHIFT_MASK:
                    break;
                case GDK_CONTROL_MASK:
                    break;
                case GDK_ALT_MASK:
                    break;
                case GDK_SUPER_MASK:
                    break;
                case 0:
                    break;
                }
            break;
        case GDK_KEY_F12:
            switch (state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SUPER_MASK))
                {
                case GDK_SHIFT_MASK:
                    gtk_window_set_default_size (GTK_WINDOW(gtk_widget_get_root( pGlobal->widgets[ eW_HP8970_application ] )), -1, -1);
                    while (g_main_context_pending (g_main_context_default ()))
                        g_main_context_iteration (NULL, TRUE);
                    break;
                case GDK_CONTROL_MASK:
                    break;
                case GDK_ALT_MASK:
                    break;
                case GDK_SUPER_MASK:
                    break;
                case 0:
                    GtkWidget *wApplicationWindow = pGlobal->widgets[ eW_HP8970_application ];
                    GdkRectangle geometry= {0};
                    GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(wApplicationWindow));
                    GdkDisplay *display = gdk_surface_get_display (surface);
                    GdkMonitor *monitor = gdk_display_get_monitor_at_surface( display, surface );
                    gdk_monitor_get_geometry( monitor, &geometry );
                    gtk_window_set_default_size(GTK_WINDOW(wApplicationWindow), geometry.height*1.53, geometry.height);
                    while (g_main_context_pending (g_main_context_default ()))
                        g_main_context_iteration (NULL, TRUE);
                    break;
                }
            break;
        default:
            break;
        }

    return FALSE;
}

static gboolean
CB_KeyReleased (GtkWidget *drawing_area) {
    return FALSE;
}


// insert-text signal
void
CB_edit_filterFloat ( GtkEditable* self, gchar* text, gint length,
  gint* position, gpointer user_data )
{
    gint i, count=0;
	gchar *result = g_new (gchar, length);
	const gchar *pExistingText = gtk_editable_get_text( self );
	gchar *pDP = strchr( pExistingText, '.' );
	gboolean twoDecimaPoints = (strchr( text, '.' ) && pDP);

	if( pDP && (pDP + 2 == pExistingText + strlen( pExistingText )) && (pDP < pExistingText + *position) )
		goto ignore;

    for( i=0; i < length; i++ ) {
	    if (!(isdigit( text[i] ) || ( text[i] == '.' && !twoDecimaPoints )))
	        continue;
	    result[count++] = text[i];
    }

    if (count > 0) {
        g_signal_handlers_block_by_func( G_OBJECT(self), CB_edit_filterFloat, user_data );
        gtk_editable_insert_text (self, result, count, position);
        g_signal_handlers_unblock_by_func( G_OBJECT(self), CB_edit_filterFloat, user_data );
    }
ignore:
    g_signal_stop_emission_by_name( self, "insert-text");
    g_free (result);
}

void
CB_edit_changed(   GtkEditable* self, gpointer user_data ) {
	/*
	const gchar *text = gtk_editable_get_text( self );
	gchar *processedText = g_strdup_printf( "%.1f", atof( text ) );
	g_signal_handlers_block_by_func( G_OBJECT(self), CB_edit_changed, user_data );
	g_signal_handlers_block_by_func( G_OBJECT(self), CB_edit_filterFloat, user_data );
	gtk_editable_set_text( self, processedText );
	g_free( processedText );
	g_signal_handlers_unblock_by_func( G_OBJECT(self), CB_edit_changed, user_data );
	g_signal_handlers_unblock_by_func( G_OBJECT(self), CB_edit_filterFloat, user_data );
	*/
}

/*!     \brief  Disable some controls when sweeping / calibrating / spot measurement
 *
 * Disable some controls when sweeping / calibrating / spot measurement
 *
 * \param  wBtnGetData  pointer to GtkButton
 * \param  udata        user data
 */
void
quarantineControlsOnSweep( tGlobal *pGlobal, gboolean bSpot, gboolean bShow ) {

    gtk_widget_set_sensitive ( pGlobal->widgets[ eW_tgl_Sweep ], pGlobal->HP8970settings.switches.bAutoSweep ? TRUE : bShow );
    gtk_widget_set_sensitive ( pGlobal->widgets[ eW_btn_Calibrate ], pGlobal->flags.bCalibrationNotPossible ? FALSE : bShow );
    gtk_widget_set_sensitive ( pGlobal->widgets[ eW_tgl_Spot ], bSpot ? TRUE : bShow );
    gtk_widget_set_sensitive ( pGlobal->widgets[ eW_chk_Correction ], bShow );

    gtk_widget_set_sensitive ( pGlobal->widgets[ eW_box_Spot ], bShow );
    gtk_widget_set_sensitive ( pGlobal->widgets[ eW_frm_Sweep ], bShow );
    gtk_widget_set_sensitive ( pGlobal->widgets[ eW_frm_Mode ], bShow );

    gtk_widget_set_sensitive ( pGlobal->widgets[ eW_page_Source ], bShow );
    gtk_widget_set_sensitive ( pGlobal->widgets[ eW_page_SigGen ], bShow );
    gtk_widget_set_sensitive ( pGlobal->widgets[ eW_page_Options ], bShow );

}


/*!     \brief  Callback for Sweep button
 *
 * Callback for Sweep button
 *
 * \param  wBtnGetData  pointer to GtkButton
 * \param  udata        user data
 */
static void
CB_tgl_Sweep (GtkToggleButton *wSweep, gpointer user_data) {
    gboolean bSweep;
	tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wSweep), "data");

	gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON( pGlobal->widgets[ eW_tgl_Spot ] ), FALSE );
	bSweep = gtk_toggle_button_get_active (wSweep);

	if( pGlobal->HP8970settings.switches.bSpotFrequency ) {
        postDataToGPIBThread (TG_ABORT, NULL);
        pGlobal->HP8970settings.switches.bSpotFrequency = FALSE;
	}

	// If we have pressed the button but we are not auto sweeping
	// do a single sweep
	if( bSweep && !pGlobal->HP8970settings.switches.bAutoSweep) {
	    postDataToGPIBThread (TG_SWEEP_HP8970, NULL);
	    g_signal_handlers_block_by_func( wSweep, CB_tgl_Sweep, NULL );
	    gtk_toggle_button_set_active (wSweep, FALSE);
        g_signal_handlers_unblock_by_func( wSweep, CB_tgl_Sweep, NULL );
        quarantineControlsOnSweep( pGlobal, FALSE, FALSE );
	} else {
	    // Otherwise terminate the autosweep
	    if( pGlobal->HP8970settings.switches.bAutoSweep ) {
	        g_signal_handlers_block_by_func( wSweep, CB_tgl_Sweep, NULL );
	        gtk_toggle_button_set_active (wSweep, FALSE);
	        g_signal_handlers_unblock_by_func( wSweep, CB_tgl_Sweep, NULL );
	        gtk_widget_set_sensitive ( GTK_WIDGET( wSweep ), FALSE );
	    }
	}

	pGlobal->HP8970settings.switches.bAutoSweep = FALSE;
}

/*!     \brief  Callback for Sweep (right button) .. auto trigger
 *
 * Callback for Sweep (right button) .. auto trigger
 *
 * \param  wSweepGesture        pointer to GtkGesture widget
 * \param  udata                user data (unused)
 */
static void
CB_gesture_SweepRightButton( GtkGestureClick* wSweepGesture, gpointer udata ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wSweepGesture), "data");
    GtkToggleButton *wSweep = GTK_TOGGLE_BUTTON( pGlobal->widgets[ eW_tgl_Sweep ] );

    gboolean bSweep = gtk_toggle_button_get_active ( wSweep );
    // If we are not sweeping, switch the toggle button on
    // indicate the auto sweep & start the sweep
    if( !bSweep ) {
        pGlobal->HP8970settings.switches.bAutoSweep = TRUE;
        postDataToGPIBThread (TG_SWEEP_HP8970, NULL);
        g_signal_handlers_block_by_func( wSweep, CB_tgl_Sweep, NULL );
        gtk_toggle_button_set_active (wSweep, TRUE);
        g_signal_handlers_unblock_by_func( wSweep, CB_tgl_Sweep, NULL );
        quarantineControlsOnSweep( pGlobal, FALSE, FALSE );
    } else {
        // If we are sweeping, switch off the toggle button and
        // disable the auto sweep. The sweep will end and controls will be
        // re-enabled
        pGlobal->HP8970settings.switches.bAutoSweep = FALSE;
        g_signal_handlers_block_by_func( wSweep, CB_tgl_Sweep, NULL );
        gtk_toggle_button_set_active (wSweep, FALSE);
        g_signal_handlers_unblock_by_func( wSweep, CB_tgl_Sweep, NULL );
    }
}

/*!     \brief  Callback for Spot Frequency read
 *
 * Signal to continually read 8970
 *
 * \param  wFrStart  pointer to GtkSpinButton
 * \param  udata     user data
 */
static
void CB_tgl_Spot (GtkToggleButton *wSpot, gpointer user_data) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wSpot), "data");

    gboolean bFormerState = pGlobal->HP8970settings.switches.bSpotFrequency;

    pGlobal->HP8970settings.switches.bSpotFrequency = gtk_toggle_button_get_active (wSpot);

    if( bFormerState == FALSE &&  pGlobal->HP8970settings.switches.bSpotFrequency ) {
        quarantineControlsOnSweep( pGlobal, TRUE, FALSE );
        postDataToGPIBThread (TG_SPOT_HP8970, NULL);
    }
}

/*!     \brief  Callback for Calibration button
 *
 * Calibration
 *
 * \param  wBtnCalibrate        pointer to GtkButton for Calibrate
 * \param  uData                user data (unused)
 */
static void
CB_btn_Calibrate( GtkButton* wBtnCalibrate, gpointer uData ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wBtnCalibrate), "data");

    postDataToGPIBThread ( TG_CALIBRATE, NULL);
    quarantineControlsOnSweep( pGlobal, FALSE, FALSE );
}

/*!     \brief  Callback for Calibration button
 *
 * Calibration
 *
 * \param  wBtnCalibrate        pointer to GtkButton for Calibrate
 * \param  udata                user data (unused)
 */
static void
CB_btn_FrequencyCalibrate( GtkGestureClick* wFrequencyCalibrateGesture, gpointer udata ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wFrequencyCalibrateGesture), "data");

    postDataToGPIBThread ( TG_FREQUENCY_CALIBRATE, NULL);
    quarantineControlsOnSweep( pGlobal, FALSE, FALSE );
}


/*!     \brief  Callback for correction check button
 *
 * Callback for correction check button
 *
 * \param  wCorrection  pointer to GtkCheckButton
 * \param  udata        user data
 */
static void
CB_chk_Correction (GtkCheckButton *wCorrection, gpointer identifier)
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wCorrection), "data");

    pGlobal->HP8970settings.switches.bCorrectedNFAndGain = gtk_check_button_get_active (wCorrection);

    UPDATE_8970_SETTING( pGlobal, pGlobal->HP8970settings.updateFlags.each.bCorrection );
}

/*!     \brief  Callback for Start Frequency spin button
 *
 * Spin button for the start frequency
 *
 * \param  wFrStart  pointer to GtkSpinButton
 * \param  udata     user data
 */
static void
CB_spin_Frequency(   GtkSpinButton* wFrequency,
                   gpointer udata ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wFrequency), "data");
    gboolean bExtLO = !(pGlobal->HP8970settings.mode == eMode1_0 || pGlobal->HP8970settings.mode == eMode1_4);

    pGlobal->HP8970settings.range[ bExtLO ].freqSpotMHz = gtk_spin_button_get_value( wFrequency );

    UPDATE_8970_SETTING( pGlobal, pGlobal->HP8970settings.updateFlags.each.bSpotFrequency );

    warnFrequencyRangeOutOfBounds( pGlobal );
}

/*!     \brief  Enable / Disable calibration button (and warn)
 *
 * Enable / Disable calibration button (and warn) based on frequency steps and range
 *
 * \param  pGlobal     pointer to Global Data
 */
void
validateCalibrationOperation (tGlobal *pGlobal ){
    gdouble nPoints;
    GtkWidget *wFrStepCal = GTK_WIDGET( pGlobal->widgets[ eW_spin_FrStep_Cal ] );
    GtkWidget *wBtnCal    = GTK_WIDGET( pGlobal->widgets[ eW_btn_Calibrate ] );
    gboolean bExtLO = !(pGlobal->HP8970settings.mode == eMode1_0 || pGlobal->HP8970settings.mode == eMode1_4);
    gdouble stepF = pGlobal->HP8970settings.range[ bExtLO ].freqStepCalMHz;
    gdouble startF = pGlobal->HP8970settings.range[ bExtLO ].freqStartMHz;
    gdouble stopF = pGlobal->HP8970settings.range[ bExtLO ].freqStopMHz;

    modf( (stopF - startF + SMIG) / stepF, &nPoints );
    if( nPoints + 1 > (pGlobal->flags.bbHP8970Bmodel == e8970A ? CAL_POINTS_8970A : CAL_POINTS_8970B ) ) {
        gtk_widget_add_css_class( wFrStepCal, "warning" );
        gtk_widget_set_sensitive( wBtnCal, FALSE );
        pGlobal->flags.bCalibrationNotPossible = TRUE;
    } else {
        gtk_widget_remove_css_class( wFrStepCal, "warning" );
        gtk_widget_set_sensitive( wBtnCal, TRUE );
        pGlobal->flags.bCalibrationNotPossible = FALSE;
    }
}


/*!     \brief  Callback for Start Frequency spin button
 *
 * Spin button for the start frequency
 *
 * \param  wFrStart  pointer to GtkSpinButton
 * \param  udata     user data
 */
static void
CB_spin_FrStart(   GtkSpinButton* wFrStart,
                   gpointer udata ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wFrStart), "data");
    gboolean bExtLO = !(pGlobal->HP8970settings.mode == eMode1_0 || pGlobal->HP8970settings.mode == eMode1_4);

    gdouble startF = gtk_spin_button_get_value( wFrStart );
    gdouble stopF = pGlobal->HP8970settings.range[ bExtLO ].freqStopMHz;

    pGlobal->HP8970settings.range[ bExtLO ].freqStartMHz = startF;

    UPDATE_8970_SETTING( pGlobal, pGlobal->HP8970settings.updateFlags.each.bStartFrequency );

    if( startF > stopF )
        gtk_spin_button_set_value( pGlobal->widgets[ eW_spin_FrStop ], startF );

    warnFrequencyRangeOutOfBounds( pGlobal );
    validateCalibrationOperation ( pGlobal );
}

/*!     \brief  Callback for Start Frequency spin button
 *
 * Spin button for the start frequency
 *
 * \param  wFrStart  pointer to GtkSpinButton
 * \param  udata     user data
 */
static void
CB_spin_FrStop(   GtkSpinButton* wFrStop,
                   gpointer udata ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wFrStop), "data");
    gboolean bExtLO = !(pGlobal->HP8970settings.mode == eMode1_0 || pGlobal->HP8970settings.mode == eMode1_4);

    pGlobal->HP8970settings.range[ bExtLO ].freqStopMHz = gtk_spin_button_get_value( wFrStop );

    UPDATE_8970_SETTING( pGlobal, pGlobal->HP8970settings.updateFlags.each.bStopFrequency );

    if( pGlobal->HP8970settings.range[ bExtLO ].freqStopMHz < pGlobal->HP8970settings.range[ bExtLO ].freqStartMHz )
        gtk_spin_button_set_value( pGlobal->widgets[ eW_spin_FrStart ], pGlobal->HP8970settings.range[ bExtLO ].freqStopMHz );

    warnFrequencyRangeOutOfBounds( pGlobal );
    validateCalibrationOperation ( pGlobal );
}


/*!     \brief  Callback for Start Frequency spin button (Calibration)
 *
 * Spin button for the start frequency (Calibration)
 *
 * \param  wFrStart  pointer to GtkSpinButton
 * \param  udata     user data
 */
static void
CB_spin_FrStep_Cal( GtkSpinButton* wFrStepCal, gpointer udata ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wFrStepCal), "data");
    gboolean bExtLO = !(pGlobal->HP8970settings.mode == eMode1_0 || pGlobal->HP8970settings.mode == eMode1_4);
    gdouble stepF = gtk_spin_button_get_value( wFrStepCal );

    pGlobal->HP8970settings.range[ bExtLO ].freqStepCalMHz = stepF;

    UPDATE_8970_SETTING( pGlobal, pGlobal->HP8970settings.updateFlags.each.bStepFrequency );

    validateCalibrationOperation ( pGlobal );
}

/*!     \brief  Callback for Start Frequency spin button (Sweep)
 *
 * Spin button for the start frequency (Sweep)
 *
 * \param  wFrStart  pointer to GtkSpinButton
 * \param  udata     user data
 */
static void
CB_spin_FrStep_Sweep(   GtkSpinButton* wFrStepSweep,
                   gpointer udata ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wFrStepSweep), "data");
    gboolean bExtLO = !(pGlobal->HP8970settings.mode == eMode1_0 || pGlobal->HP8970settings.mode == eMode1_4);
    gdouble stepF = gtk_spin_button_get_value( wFrStepSweep );
    pGlobal->HP8970settings.range[ bExtLO ].freqStepSweepMHz = stepF;
}


/*!     \brief  Callback for mode comboboxtext
 *
 * Change the mode
 *
 * \param  wComboMode  pointer to GtkComboBoxText
 * \param  udata       user data
 */
static void
CB_combo_Mode ( GtkComboBox* wComboMode, gpointer udata ) {
    gboolean bUpdate = TRUE;
    gboolean bShowLOpage = TRUE;
    tMode mode;

    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wComboMode), "data");

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    const gchar *sID = gtk_combo_box_get_active_id ( GTK_COMBO_BOX( wComboMode ) );
#pragma GCC diagnostic pop

    switch( sID[0] ) {
        case '0':
        default:
            mode = eMode1_0;
            bShowLOpage = FALSE;
            break;
        case '1':
            mode = eMode1_1;
            break;
        case '2':
            mode = eMode1_2;
            break;
        case '3':
            mode = eMode1_3;
            break;
        case '4':
            mode = eMode1_4;
            break;
    }

    g_mutex_lock ( &pGlobal->mUpdate );
    bUpdate = (pGlobal->HP8970settings.updateFlags.all == 0);
    pGlobal->HP8970settings.mode = mode;
    pGlobal->HP8970settings.updateFlags.each.bMode = TRUE;
    pGlobal->HP8970settings.updateFlags.each.bSpotFrequency = TRUE;
    pGlobal->HP8970settings.updateFlags.each.bStartFrequency = TRUE;
    pGlobal->HP8970settings.updateFlags.each.bStopFrequency = TRUE;
    pGlobal->HP8970settings.updateFlags.each.bStepFrequency = TRUE;
    g_mutex_unlock ( &pGlobal->mUpdate );

    refreshMainDialog( pGlobal );

    if( bUpdate )
        postDataToGPIBThread (TG_SEND_SETTINGS_to_HP8970, NULL);

    // Switch notebook page to Ext LO if not mode 1.0
    if( bShowLOpage )
        gtk_notebook_set_current_page( pGlobal->widgets[ eW_note_Controls ], ePageExtLO );

    gtk_widget_queue_draw ( pGlobal->widgets[ eW_drawing_Plot ] );
}

/*!     \brief  Callback for smoothing GtkComboBoxText
 *
 * Change the smoothing factor
 *
 * \param  wComboSmoothing  pointer to GtkComboBoxText
 * \param  udata            user data
 */
static void
CB_combo_Smoothing ( GtkComboBox* wComboSmoothing, gpointer udata ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(wComboSmoothing), "data");
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    const gchar *sID = gtk_combo_box_get_active_id ( GTK_COMBO_BOX( wComboSmoothing ) );
#pragma GCC diagnostic pop

    if( sID != NULL )
        pGlobal->HP8970settings.smoothingFactor = (gint)pow( 2.0, atoi( sID ));

    UPDATE_8970_SETTING( pGlobal, pGlobal->HP8970settings.updateFlags.each.bSmoothing );
}

// insert-text signal
void
CB_edit_Title (   GtkEditable* self, gpointer user_data )
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(gtk_widget_get_root(GTK_WIDGET(self))), "data");
    const gchar *sTitle = gtk_editable_get_text( self );

    g_free( pGlobal->plot.sTitle );
    pGlobal->plot.sTitle = g_strdup( sTitle );

    gtk_widget_queue_draw ( pGlobal->widgets[ eW_drawing_Plot ] );
}

/*
 * Mouse wheel scroll in the Plot to change the time scale in spot frequency
 */
gboolean
on_PlotMouseWheel( GtkEventControllerScroll* self, gdouble dx, gdouble dy, gpointer user_data ) {
    return FALSE;
}

/*!     \brief  Callback for mouse movement in the plot area (GtkDrawingArea)
 *
 * Callback for mouse movement in the plot area whish will set the live markers if we have valid data.
 * We alse get a callback for entry and exit of the draawing area
 *
 * \param eventGesture    event controller for this mouse action
 * \param x               x position of mouse
 * \param y               y position of mouse
 * \param action          GDK_MOTION_NOTIFY or GDK_ENTER_NOTIFY or GDK_LEAVE_NOTIFY
 */
void on_PlotMouseMotion ( GtkEventControllerMotion* eventGesture, gdouble x,  gdouble y, gpointer action ) {
    GtkDrawingArea *wDrawingArea= GTK_DRAWING_AREA( gtk_event_controller_get_widget( GTK_EVENT_CONTROLLER( eventGesture ) ));
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(gtk_widget_get_root(GTK_WIDGET(wDrawingArea))), "data");

    if( !pGlobal->flags.bHoldLiveMarker ) {
        pGlobal->flags.bLiveMarkerActive = ( GPOINTER_TO_INT( action ) == GDK_MOTION_NOTIFY
                || GPOINTER_TO_INT( action ) == GDK_ENTER_NOTIFY );


        pGlobal->liveMarkerPosnRatio.x = x / gtk_widget_get_width( GTK_WIDGET( wDrawingArea ) );
        pGlobal->liveMarkerPosnRatio.y = y / gtk_widget_get_height( GTK_WIDGET( wDrawingArea ) );

        if( pGlobal->plot.measurementBuffer.flags.bValidNoiseData || pGlobal->plot.measurementBuffer.flags.bValidGainData )
            gtk_widget_queue_draw ( GTK_WIDGET( wDrawingArea ) );
    }
}

/*!     \brief  Callback for press left mouse button in the plot area (GtkDrawingArea)
 *
 * Callback for press left mouse button in the plot area (GtkDrawingArea). This temporarily releases
 * the live marker.
 *
 * \param eventGesture    event controller for this mouse action
 * \param n_press         number of button presses (can be 1 or more)
 * \param x               x position of mouse
 * \param y               y position of mouse
 * \param uData          user data that allows us to differentiate beteen enter and leave
 */
void
on_PlotLeftMouse_1_press ( GtkGestureClick* eventGesture, gint n_press, gdouble x, gdouble y, gpointer uData ) {
    GtkDrawingArea *wDrawingArea= GTK_DRAWING_AREA( gtk_event_controller_get_widget( GTK_EVENT_CONTROLLER( eventGesture ) ));
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(gtk_widget_get_root(GTK_WIDGET(wDrawingArea))), "data");

    if ( pGlobal->flags.bPreviewModeDiagram )
        gtk_widget_grab_focus ( pGlobal->widgets[ eW_note_Controls ] );
    pGlobal->flags.bPreviewModeDiagram = FALSE;

    pGlobal->flags.bHoldLiveMarker = FALSE;
    pGlobal->liveMarkerPosnRatio.x = x / gtk_widget_get_width( GTK_WIDGET( wDrawingArea ) );
    pGlobal->liveMarkerPosnRatio.y = y / gtk_widget_get_height( GTK_WIDGET( wDrawingArea ) );
    if( pGlobal->plot.measurementBuffer.flags.bValidNoiseData || pGlobal->plot.measurementBuffer.flags.bValidGainData )
        gtk_widget_queue_draw ( GTK_WIDGET( wDrawingArea ) );
}

/*!     \brief  Callback for mouse clicks in the plot area
 *
 * Freeze and realease the live marker with the left and right mouse buttons
 *
 * \param eventGesture    event controller for this mouse action
 * \param n_press         number of button presses (can be 1 or more)
 * \param x               x position of mouse
 * \param y               y position of mouse
 * \param uData          user data that allows us to differentiate beteen enter and leave
 */
void
CB_PlotMouse_3_press_1_release ( GtkGestureClick* eventGesture, gint n_press, gdouble x, gdouble y, gpointer uData ) {
    GtkDrawingArea *wDrawingArea= GTK_DRAWING_AREA( gtk_event_controller_get_widget( GTK_EVENT_CONTROLLER( eventGesture ) ));
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(gtk_widget_get_root(GTK_WIDGET(wDrawingArea))), "data");

    GdkModifierType __attribute__((unused)) modifiers
            = gtk_event_controller_get_current_event_state( GTK_EVENT_CONTROLLER( eventGesture ) );
    guint button = gtk_gesture_single_get_current_button( GTK_GESTURE_SINGLE( eventGesture ) );

    if ( pGlobal->flags.bPreviewModeDiagram )
        gtk_widget_grab_focus ( pGlobal->widgets[ eW_note_Controls ] );
    pGlobal->flags.bPreviewModeDiagram = FALSE;

    if ( button == 1 )
        pGlobal->flags.bHoldLiveMarker = TRUE;
    else if ( button == 3 ) {
        pGlobal->flags.bHoldLiveMarker = FALSE;
        pGlobal->liveMarkerPosnRatio.x = x / gtk_widget_get_width( GTK_WIDGET( wDrawingArea ) );
        pGlobal->liveMarkerPosnRatio.y = y / gtk_widget_get_height( GTK_WIDGET( wDrawingArea ) );
        if( pGlobal->plot.measurementBuffer.flags.bValidNoiseData || pGlobal->plot.measurementBuffer.flags.bValidGainData )
            gtk_widget_queue_draw ( GTK_WIDGET( wDrawingArea ) );
    }
}

/*!     \brief  Refresh the controls on the main page
 *
 * Refresh the controls on the main page (like the spin buttons)
 *
 * \param  pGlobal  pointer to global data
 */
void
refreshMainDialog( tGlobal *pGlobal )
{
    GtkWidget *wFrStart, *wFrStop, *wFrStepCal, *wFrStepSweep, *wFrequency, *wMode;
    gboolean bExtLO = !(pGlobal->HP8970settings.mode == eMode1_0 || pGlobal->HP8970settings.mode == eMode1_4);
    gdouble min = bExtLO ? HP8970A_MIN_FREQ_R2 : HP8970A_MIN_FREQ;
    gdouble max = bExtLO ? HP8970A_MAX_FREQ_R2 : maxInputFreq[ pGlobal->flags.bbHP8970Bmodel ];
    gdouble pageStep = bExtLO ? HP8970A_PageStep_FREQ_R2 : HP8970A_PageStep_FREQ;

    wFrStart     = pGlobal->widgets[ eW_spin_FrStart ];
    wFrequency   = pGlobal->widgets[ eW_spin_Frequency ];
    wFrStop      = pGlobal->widgets[ eW_spin_FrStop ];
    wFrStepCal   = pGlobal->widgets[ eW_spin_FrStep_Cal ];
    wFrStepSweep = pGlobal->widgets[ eW_spin_FrStep_Sweep ];
    wMode        = pGlobal->widgets[ eW_combo_Mode ];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    // We don't want an infinite loop here ....
    g_signal_handlers_block_by_func( G_OBJECT( wMode ), CB_combo_Mode, NULL );
    g_signal_handlers_block_by_func( G_OBJECT( wFrStart ), CB_spin_FrStart, NULL );
    g_signal_handlers_block_by_func( G_OBJECT( wFrStop ), CB_spin_FrStop, NULL );
//  g_signal_handlers_block_by_func( G_OBJECT( wFrStepCal ), CB_spin_FrStep_Cal, NULL );
    g_signal_handlers_block_by_func( G_OBJECT( wFrStepSweep ), CB_spin_FrStep_Sweep, NULL );
    g_signal_handlers_block_by_func( G_OBJECT( wFrequency ), CB_spin_Frequency, NULL );

    gtk_combo_box_set_active( GTK_COMBO_BOX( wMode ), pGlobal->HP8970settings.mode );

    gtk_combo_box_set_active( GTK_COMBO_BOX( pGlobal->widgets[ eW_combo_Smoothing ]),
                              pGlobal->HP8970settings.smoothingFactor ? (gint)log2(pGlobal->HP8970settings.smoothingFactor) : 1 );
#pragma GCC diagnostic pop

    // Initialize widgets on main page
    gtk_check_button_set_active ( pGlobal->widgets[ eW_chk_Correction ], pGlobal->HP8970settings.switches.bCorrectedNFAndGain );

    // Frequency
    gtk_spin_button_set_range( GTK_SPIN_BUTTON( wFrequency ), min, max );
    gtk_spin_button_set_increments ( GTK_SPIN_BUTTON( wFrequency ), 1.0, pageStep );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON( wFrequency ), pGlobal->HP8970settings.range[bExtLO].freqSpotMHz );
    // Sweep Start
    gtk_spin_button_set_range( GTK_SPIN_BUTTON( wFrStart ), min, max );
    gtk_spin_button_set_increments ( GTK_SPIN_BUTTON( wFrStart ), 1.0, pageStep );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON( wFrStart ), pGlobal->HP8970settings.range[bExtLO].freqStartMHz );
    // Sweep End
    gtk_spin_button_set_range( GTK_SPIN_BUTTON( wFrStop ), min, max );
    gtk_spin_button_set_increments ( GTK_SPIN_BUTTON( wFrStop ), 1.0, pageStep );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON( wFrStop ), pGlobal->HP8970settings.range[bExtLO].freqStopMHz );

    // Sweep Step
    gtk_spin_button_set_range( GTK_SPIN_BUTTON( wFrStepCal ), 1, max );
    gtk_spin_button_set_increments ( GTK_SPIN_BUTTON( wFrStepCal ), 1.0, pageStep );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON( wFrStepCal ), pGlobal->HP8970settings.range[bExtLO].freqStepCalMHz );

    gtk_spin_button_set_range( GTK_SPIN_BUTTON( wFrStepSweep ), 1, max );
    gtk_spin_button_set_increments ( GTK_SPIN_BUTTON( wFrStepSweep ), 1.0, pageStep );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON( wFrStepSweep ), pGlobal->HP8970settings.range[bExtLO].freqStepSweepMHz );

    // Initialize widgets on main page
    gtk_check_button_set_active ( pGlobal->widgets[ eW_chk_Correction ], pGlobal->HP8970settings.switches.bCorrectedNFAndGain );

    g_signal_handlers_unblock_by_func( G_OBJECT( wMode ), CB_combo_Mode, NULL );
    g_signal_handlers_unblock_by_func( G_OBJECT( wFrStart ), CB_spin_FrStart, NULL );
    g_signal_handlers_unblock_by_func( G_OBJECT( wFrStop ), CB_spin_FrStop, NULL );
//  g_signal_handlers_unblock_by_func( G_OBJECT( wFrStepCal ), CB_spin_FrStep_Cal, NULL );
    g_signal_handlers_unblock_by_func( G_OBJECT( wFrStepSweep ), CB_spin_FrStep_Sweep, NULL );
    g_signal_handlers_unblock_by_func( G_OBJECT( wFrequency ), CB_spin_Frequency, NULL );

    gtk_widget_set_sensitive( pGlobal->widgets[ eW_btn_CSV ], pGlobal->plot.measurementBuffer.flags.bValidNoiseData );
    gtk_widget_set_sensitive( pGlobal->widgets[ eW_btn_SaveJSON ], pGlobal->plot.measurementBuffer.flags.bValidNoiseData );

    enablePageExtLOwidgets( pGlobal, pGlobal->HP8970settings.mode );
    warnFrequencyRangeOutOfBounds( pGlobal );
}


/*!     \brief  Callback for when the mouse enters of leaves the mode combobox
 *
 * The mode diagram is shown if the use moves the cursor over the mode combobox/
 * ** Editorial note ... the process of previewing the mode diagram is horribly complicated and messy.
 * I wish there was an easier way to do this!
 *
 * \param eventGesture    event controller for this mouse action
 * \param x               x position of mouse
 * \param y               y position of mouse
 * \param action          user data that allows us to differentiate beteen enter and leave
 *
 */
void on_ModeCombo_MouseEnterLeave ( GtkEventControllerMotion* eventGesture, gdouble x,  gdouble y, gpointer gpAction ) {
    guint action = GPOINTER_TO_INT( gpAction );

    GtkComboBoxText *wModeCombo= GTK_COMBO_BOX_TEXT( gtk_event_controller_get_widget( GTK_EVENT_CONTROLLER( eventGesture ) ));
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(gtk_widget_get_root(GTK_WIDGET(wModeCombo))), "data");

    gboolean value;
    g_object_get (G_OBJECT (wModeCombo), "popup-shown", &value, NULL);


    gboolean bFocused = gtk_widget_has_focus(
            gtk_widget_get_first_child( gtk_widget_get_first_child( GTK_WIDGET( wModeCombo ) ) ) );

    pGlobal->flags.bPreviewModeDiagram = value || ( GDK_ENTER_NOTIFY == action ) || bFocused;
    gtk_widget_queue_draw ( pGlobal->widgets[ eW_drawing_Plot ] );
}

/*!     \brief  Callback for when the focus leaves the combo box
 *
 * The mode diagram is shown if the user selects a new entry from the combo box.
 * We show this until he leaves focus (unless there is no valid data)
 *
 * \param eventFocus    event controller for this focus action
 * \param uData          user data
 *
 */
void
CB_combo_ModeOutOfFocus( GtkEventControllerFocus* eventFocus, gpointer uData ) {

    GtkComboBoxText *wModeCombo= GTK_COMBO_BOX_TEXT( gtk_event_controller_get_widget( GTK_EVENT_CONTROLLER( eventFocus ) ));
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(gtk_widget_get_root(GTK_WIDGET(wModeCombo))), "data");

    pGlobal->flags.bPreviewModeDiagram = FALSE;
    gtk_widget_queue_draw ( pGlobal->widgets[ eW_drawing_Plot ] );
}

/*!     \brief  Callback when Mode combobox is shown and hidden
 *
 * We use this to display the mode diagram.
 * If the mode changes, we also trigger a timer so that the new diagram is shown for a
 * period and is then removed (if a valid plot is available)
 *
 * \param gpGlobal    gpointer to the gloabl data
 * \param wModeCombo  pointer to global data
 *
 */
void
CB_combo_ModePopupAndDown ( GtkComboBox* wModeCombo, gpointer gpGlobal ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(gtk_widget_get_root(GTK_WIDGET(wModeCombo))), "data");
    gboolean bPopup;

    g_object_get (G_OBJECT (wModeCombo), "popup-shown", &bPopup, NULL);

    pGlobal->flags.bPreviewModeDiagram = TRUE;
    gtk_widget_queue_draw ( pGlobal->widgets[ eW_drawing_Plot ] );
}

/*!     \brief  Set the sweep spin buttons to match the mode
 *
 * Initialize the spin buttons
 *
 * \param  pGlobal  pointer to global data
 */
void
initializeMainDialog( tGlobal *pGlobal )
{
    GtkWidget *wDrawingArea, *wApplication, *wModeCombo, *wSaveJSON;
    GtkGesture *gesture;

    wDrawingArea = pGlobal->widgets[ eW_drawing_Plot ];
    wApplication = pGlobal->widgets[ eW_HP8970_application ];
    wModeCombo = pGlobal->widgets[ eW_combo_Mode ];
    wSaveJSON = pGlobal->widgets[ eW_btn_SaveJSON ];
    GtkNotebook *noteBook = pGlobal->widgets[ eW_note_Controls ];

    gtk_notebook_reorder_child  ( noteBook, pGlobal->widgets[ eW_page_Options ], ePage8970 );
    gtk_notebook_reorder_child  ( noteBook, pGlobal->widgets[ eW_page_Notes ], ePageNotes );
    gtk_notebook_reorder_child  ( noteBook, pGlobal->widgets[ eW_page_Plot ], ePagePlot );
    gtk_notebook_reorder_child  ( noteBook, pGlobal->widgets[ eW_page_SigGen ], ePageExtLO );
    gtk_notebook_reorder_child  ( noteBook, pGlobal->widgets[ eW_page_Source ], ePageNoiseSource );
    gtk_notebook_reorder_child  ( noteBook, pGlobal->widgets[ eW_page_Settings ], ePageOptions );
    gtk_notebook_reorder_child  ( noteBook, pGlobal->widgets[ eW_page_GPIB ], ePageGPIB );

    // Setup callbacks for **some** widgets first so that we send the initial settings
    g_signal_connect( pGlobal->widgets[ eW_chk_Correction ],  "toggled", G_CALLBACK( CB_chk_Correction ), NULL);
    // Frequency and Sweep Spinner signals
    g_signal_connect( pGlobal->widgets[ eW_spin_Frequency ], "value-changed", G_CALLBACK( CB_spin_Frequency ), NULL);
    g_signal_connect( pGlobal->widgets[ eW_spin_FrStart ],   "value-changed", G_CALLBACK( CB_spin_FrStart ),   NULL);
    g_signal_connect( pGlobal->widgets[ eW_spin_FrStop ],    "value-changed", G_CALLBACK( CB_spin_FrStop ),    NULL);
    g_signal_connect( pGlobal->widgets[ eW_spin_FrStep_Cal ],    "value-changed", G_CALLBACK( CB_spin_FrStep_Cal ),    NULL);
    g_signal_connect( pGlobal->widgets[ eW_spin_FrStep_Sweep ],    "value-changed", G_CALLBACK( CB_spin_FrStep_Sweep ),    NULL);
    g_signal_connect( pGlobal->widgets[ eW_combo_Smoothing ],  "changed", G_CALLBACK( CB_combo_Smoothing ), NULL);
    // Connect callbacks
    g_signal_connect( pGlobal->widgets[ eW_tgl_Sweep ],  "toggled", G_CALLBACK( CB_tgl_Sweep ), NULL);
    // Right mouse button press
    gesture = gtk_gesture_click_new ();
    g_object_set_data(G_OBJECT(gesture), "data", pGlobal);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 3);
    g_signal_connect( gesture, "pressed",  G_CALLBACK( CB_gesture_SweepRightButton ), pGlobal);
    gtk_widget_add_controller ( pGlobal->widgets[ eW_tgl_Sweep ], GTK_EVENT_CONTROLLER (gesture) );

    g_signal_connect( pGlobal->widgets[ eW_tgl_Spot ],  "toggled", G_CALLBACK( CB_tgl_Spot ), NULL);
    // Calibrate on Left mouse button release
    g_signal_connect( pGlobal->widgets[ eW_btn_Calibrate ], "clicked", G_CALLBACK( CB_btn_Calibrate ), NULL);
    // Frequency calibrate on right mouse release
    gesture = gtk_gesture_click_new ();
    g_object_set_data(G_OBJECT(gesture), "data", pGlobal);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 3);
    g_signal_connect (gesture, "released", G_CALLBACK (CB_btn_FrequencyCalibrate), NULL);
    gtk_widget_add_controller (pGlobal->widgets[ eW_btn_Calibrate ], GTK_EVENT_CONTROLLER (gesture));

    g_signal_connect( pGlobal->widgets[ eW_btn_Print ],  "clicked", G_CALLBACK( CB_btn_Print ), NULL);
    g_signal_connect( pGlobal->widgets[ eW_btn_PDF ],  "clicked", G_CALLBACK( CB_btn_PDF ), NULL);
    g_signal_connect( pGlobal->widgets[ eW_btn_SVG ],  "clicked", G_CALLBACK( CB_btn_SVG ), NULL);
    g_signal_connect( pGlobal->widgets[ eW_btn_PNG ],  "clicked", G_CALLBACK( CB_btn_PNG ), NULL);
    g_signal_connect( pGlobal->widgets[ eW_btn_CSV ],  "clicked", G_CALLBACK( CB_btn_CSV ), NULL);

    // Connect a right click gesture to the 'save JSON' widget
    g_signal_connect( pGlobal->widgets[ eW_btn_SaveJSON ],  "clicked", G_CALLBACK( CB_btn_SaveJSON ), NULL);
    gesture = gtk_gesture_click_new ();
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 3);
    g_signal_connect (gesture, "released", G_CALLBACK (CB_rightClickGesture_SaveJSON), wSaveJSON);
    gtk_widget_add_controller (wSaveJSON, GTK_EVENT_CONTROLLER (gesture));


    g_signal_connect( pGlobal->widgets[ eW_btn_RestoreJSON ],  "clicked", G_CALLBACK( CB_btn_RestoreJSON ), NULL);

    g_signal_connect(wModeCombo, "changed", G_CALLBACK( CB_combo_Mode ), NULL);

    g_signal_connect(gtk_editable_get_delegate(GTK_EDITABLE( pGlobal->widgets[ eW_entry_Title ] )), "changed",
                     G_CALLBACK( CB_edit_Title ), NULL);

    // Live marker when mouse moved in Plot area
    GtkEventController *eventMouseMotion = gtk_event_controller_motion_new ();
    gtk_event_controller_set_propagation_phase(eventMouseMotion, GTK_PHASE_CAPTURE);
    g_signal_connect(eventMouseMotion, "motion", G_CALLBACK( on_PlotMouseMotion ), GINT_TO_POINTER( GDK_MOTION_NOTIFY ));
    g_signal_connect(eventMouseMotion, "enter",  G_CALLBACK( on_PlotMouseMotion ), GINT_TO_POINTER( GDK_ENTER_NOTIFY ));
    g_signal_connect(eventMouseMotion, "leave",  G_CALLBACK( on_PlotMouseMotion ), GINT_TO_POINTER( GDK_LEAVE_NOTIFY ));
    gtk_widget_add_controller (wDrawingArea, GTK_EVENT_CONTROLLER (eventMouseMotion));

    // Mouse wheel in Plot area
    GtkEventController *eventMouseWheel = gtk_event_controller_scroll_new ( GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES );
    g_signal_connect(eventMouseWheel, "scroll", G_CALLBACK( on_PlotMouseWheel ), NULL);
    gtk_widget_add_controller (wDrawingArea, GTK_EVENT_CONTROLLER (eventMouseWheel));

    // Mouse button callbacks for freezing and releasing the live marker
    // Left mouse button release
    GtkGesture *gestureMouseClick = gtk_gesture_click_new ();
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gestureMouseClick), 1);  // default .. so not necessary
    g_signal_connect( gestureMouseClick, "released",  G_CALLBACK( CB_PlotMouse_3_press_1_release ), pGlobal);
    gtk_widget_add_controller ( wDrawingArea, GTK_EVENT_CONTROLLER (gestureMouseClick) );
    // Right mouse button press
    gestureMouseClick = gtk_gesture_click_new ();
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gestureMouseClick), 3);
    g_signal_connect( gestureMouseClick, "pressed",  G_CALLBACK( CB_PlotMouse_3_press_1_release ), pGlobal);
    gtk_widget_add_controller ( wDrawingArea, GTK_EVENT_CONTROLLER (gestureMouseClick) );
    // Left mouse button press
    gestureMouseClick = gtk_gesture_click_new ();
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gestureMouseClick), 1);
    g_signal_connect( gestureMouseClick, "pressed",  G_CALLBACK( on_PlotLeftMouse_1_press ), pGlobal);
    gtk_widget_add_controller ( wDrawingArea, GTK_EVENT_CONTROLLER (gestureMouseClick) );

    // Define the drawing function for the GtkDrawingArea widget
    gtk_drawing_area_set_draw_func ( pGlobal->widgets[ eW_drawing_Plot ], CB_DrawingArea_Draw, pGlobal, NULL);

#if 0
    void appEnter (GtkEventControllerFocus *self, gpointer gpGlobal);
    void appLeave (GtkEventControllerFocus *self, gpointer gpGlobal);
    GtkEventController *focus_controller = gtk_event_controller_focus_new ();
    g_signal_connect(focus_controller, "enter", G_CALLBACK( appEnter ), pGlobal);
    g_signal_connect(focus_controller, "leave", G_CALLBACK( appLeave ), pGlobal);
    gtk_widget_add_controller ( wApplication, focus_controller );
#endif

    // Get callbacks for keypress and release
    GtkEventController *key_controller = gtk_event_controller_key_new ();
    g_signal_connect_object (key_controller, "key-pressed",  G_CALLBACK(CB_KeyPressed),  wApplication, G_CONNECT_SWAPPED);
    g_signal_connect_object (key_controller, "key-released", G_CALLBACK(CB_KeyReleased), wApplication, G_CONNECT_SWAPPED);
    gtk_widget_add_controller ( wApplication, key_controller );

    // call back when mouse enters and leaves the mode combo box (to show mode diagram)
    eventMouseMotion = gtk_event_controller_motion_new ();
    gtk_event_controller_set_propagation_phase(eventMouseMotion, GTK_PHASE_CAPTURE);
    g_signal_connect(eventMouseMotion, "enter",  G_CALLBACK( on_ModeCombo_MouseEnterLeave ), GINT_TO_POINTER( GDK_ENTER_NOTIFY ));
    g_signal_connect(eventMouseMotion, "leave",  G_CALLBACK( on_ModeCombo_MouseEnterLeave ), GINT_TO_POINTER( GDK_LEAVE_NOTIFY ));
    gtk_widget_add_controller (wModeCombo, GTK_EVENT_CONTROLLER (eventMouseMotion));

    // call back when mouse enters and leaves the mode combo box (to show mode diagram)
    GtkEventController *eventFocus = gtk_event_controller_focus_new ();
    gtk_widget_add_controller( wModeCombo, eventFocus);
    g_signal_connect( eventFocus, "leave", G_CALLBACK( CB_combo_ModeOutOfFocus), NULL);

    // call back when mode combo box shows or is hidden (to show mode diagram)
    g_signal_connect_after(wModeCombo, "notify::popup-shown", G_CALLBACK( CB_combo_ModePopupAndDown ), NULL);


    refreshMainDialog( pGlobal );
    gtk_notebook_set_current_page( noteBook, ePage8970 );
}


