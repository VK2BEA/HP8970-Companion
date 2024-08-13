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

#include <HP8970.h>
#include <messageEvent.h>

typedef struct {
    gint timerID;
    GtkLabel *wLabel;
} tStatusTimer;

static tStatusTimer clearTimer = { 0, NULL };
static tStatusTimer clearTimer_LO = { 0, NULL };

gboolean
clearNotification( gpointer gpStatusTimer ) {
    tStatusTimer *pStatusTimer = (tStatusTimer *)gpStatusTimer;

	gtk_label_set_text(GTK_LABEL( pStatusTimer->wLabel ), "");
	pStatusTimer->timerID = 0;
	return G_SOURCE_REMOVE;
}

GSourceFuncs messageEventFunctions = { messageEventPrepare, messageEventCheck,
		messageEventDispatch, NULL, };

/*!     \brief  Dispatch message posted by a GPIB thread
 *
 * Only the main event loop can update screen widgets.
 * Other threads post messages that are accepted here.
 *
 * Display messages (strings) are individually pulled from a queue.
 *
 * \param source   : GSource for the message event
 * \param callback : callback defined for this source (unused)
 * \param udata    : other data (unused)
 * \return G_SOURCE_CONTINUE to keep this source in the main loop
 */
gboolean
messageEventDispatch(GSource *source, GSourceFunc callback, gpointer udata) {
	messageEventData *message;

	tGlobal *pGlobal = &globalData;
	static gboolean bInitial = TRUE;

	if( bInitial ) {
        clearTimer.wLabel = GTK_LABEL( pGlobal->widgets[ eW_lbl_Status ]);
        clearTimer_LO.wLabel = GTK_LABEL( pGlobal->widgets[ eW_lbl_Status_LO ]);
        bInitial = FALSE;
	}
	gchar *sMarkup;



	while ((message = g_async_queue_try_pop(pGlobal->messageQueueToMain))) {
		switch (message->command) {
		case TM_INFO:
            if( clearTimer.timerID != 0 )
                g_source_remove( clearTimer.timerID );
            clearTimer.timerID = g_timeout_add ( 10000, clearNotification, &clearTimer );
            sMarkup = g_markup_printf_escaped("<i>%s</i>", message->sMessage);
            gtk_label_set_markup(clearTimer.wLabel, sMarkup);
            g_free(sMarkup);
		    break;

        case TM_INFO_LO:
            if( clearTimer_LO.timerID != 0 )
                g_source_remove( clearTimer_LO.timerID );
            clearTimer_LO.timerID = g_timeout_add ( 10000, clearNotification, &clearTimer_LO );
            sMarkup = g_markup_printf_escaped("<i>%s</i>", message->sMessage);
            gtk_label_set_markup(clearTimer_LO.wLabel, sMarkup);
            g_free(sMarkup);
            break;

		case TM_INFO_HIGHLIGHT:
			if( clearTimer.timerID != 0 )
				g_source_remove( clearTimer.timerID );
			clearTimer.timerID = g_timeout_add ( 10000, clearNotification, &clearTimer );
			sMarkup = g_markup_printf_escaped("<span color='darkgreen'><i>َ%s</i></span>", message->sMessage);
			gtk_label_set_markup(clearTimer.wLabel, sMarkup);
			g_free(sMarkup);
			break;

		case TM_ERROR:
            if( clearTimer.timerID != 0 )
                g_source_remove( clearTimer.timerID );
            clearTimer.timerID = g_timeout_add ( 15000, clearNotification, &clearTimer );

            sMarkup = g_markup_printf_escaped(
                    "<span color=\"darkred\">%s</span>", message->sMessage);
            gtk_label_set_markup(clearTimer.wLabel, sMarkup);
            g_free(sMarkup);
		    break;

        case TM_ERROR_LO:
			if( clearTimer_LO.timerID != 0 )
				g_source_remove( clearTimer_LO.timerID );
			clearTimer_LO.timerID = g_timeout_add ( 15000, clearNotification, &clearTimer_LO );

			sMarkup = g_markup_printf_escaped(
					"<span color=\"darkred\">%s</span>", message->sMessage);
            gtk_label_set_markup(clearTimer_LO.wLabel, sMarkup);
			g_free(sMarkup);
			break;

        case TM_REFRESH_PLOT:
            setSpinNoiseRange( pGlobal );
            gtk_widget_queue_draw ( pGlobal->widgets[ eW_drawing_Plot ] );
            g_free( message->data );
            break;

		case TM_COMPLETE_GPIB:
		    quarantineControlsOnSweep( pGlobal, FALSE, TRUE );
            validateCalibrationOperation( pGlobal);
            gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON( pGlobal->widgets[ eW_tgl_Spot ] ), FALSE );

			break;

		default:
			break;
		}

		g_free(message->sMessage);
		g_free(message);
	}

	return G_SOURCE_CONTINUE;
}

/*!     \brief  Prepare source event
 *
 * check for messages sent from duplication threads
 * to update the GUI.
 * We can only (safely) update the widgets from the main tread loop
 *
 * \param source : GSource for the message event
 * \param pTimeout : pointer to return timeout value
 * \return TRUE if we have a message to dispatch
 */
gboolean messageEventPrepare(GSource *source, gint *pTimeout) {
	*pTimeout = -1;
	return g_async_queue_length(globalData.messageQueueToMain) > 0;
}

/*!     \brief  Check source event
 *
 * check for messages sent from duplication threads
 * to update the GUI.
 * We can only (safely) update the widgets from the main tread loop
 *
 * \param source : GSource for the message event
 * \return TRUE if we have a message to dispatch
 */
gboolean messageEventCheck(GSource *source) {
	return g_async_queue_length(globalData.messageQueueToMain) > 0;
}

/*!     \brief  Send status state from thread to the main loop
 *
 * Send error information to the main loop
 *
 * \param Command       : enumerated state to indicate action
 * \param sMessage      : message or signal
 */
void
postMessageToMainLoop(enum _threadmessage Command, gchar *sMessage) {
	// sMesaage can be a pointer to a string or a small gint up to PM_MAX (notification message)

	messageEventData *messageData;   // g_free() in threadEventsDispatch
	messageData = g_malloc0(sizeof(messageEventData));

	messageData->sMessage = g_strdup(sMessage); // g_free() in threadEventsDispatch
	messageData->command = Command;

	g_async_queue_push(globalData.messageQueueToMain, messageData);
	g_main_context_wakeup( NULL);
}

/*!     \brief  Send message with number from thread to the main loop
 *
 * Send message with number to the main loop
 *
 * \param Command	enumerated state to indicate action
 * \param sMessageWithFormat message with printf formatting
 * \param number
 */
void
postInfoWithCount(gchar *sMessageWithFormat, gint number, gint number2) {
	gchar *sLabel = g_strdup_printf( sMessageWithFormat, number, number2 );
	postMessageToMainLoop( TM_INFO, sLabel );
	g_free (sLabel);

}


/*!     \brief  Send status state from thread to the main loop
 *
 * Send error information to the main loop
 *
 * \param Command       : enumerated state to indicate action
 * \param sMessage      : message or signal
 */
void postDataToMainLoop(enum _threadmessage Command, void *data) {
	// sMesaage can be a pointer to a string or a small gint up to PM_MAX (notification message)

	messageEventData *messageData;   // g_free() in threadEventsDispatch
	messageData = g_malloc0(sizeof(messageEventData));

	messageData->data = data;
	messageData->command = Command;

	g_async_queue_push(globalData.messageQueueToMain, messageData);
	g_main_context_wakeup( NULL);
}

/*!     \brief  Send status state from thread to the main loop
 *
 * Send error information to the main loop
 *
 * \param Command       : enumerated state to indicate action
 * \param sMessage      : message or signal
 */
void postDataToGPIBThread(enum _threadmessage Command, void *data) {
	// sMesaage can be a pointer to a string or a small gint up to PM_MAX (notification message)
	messageEventData *messageData;   // g_free() in threadEventsDispatch
	messageData = g_malloc0(sizeof(messageEventData));

	messageData->data = data;
	messageData->command = Command;

	if( Command == TG_ABORT ) {
	    g_async_queue_push_front(globalData.messageQueueToGPIB, messageData);
	} else {
	    g_async_queue_push(globalData.messageQueueToGPIB, messageData);
	}
}
