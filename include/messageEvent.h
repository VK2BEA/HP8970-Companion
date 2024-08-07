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

#ifndef MESSAGEEVENT_H_
#define MESSAGEEVENT_H_

gboolean messageEventPrepare (GSource *, gint *);
gboolean messageEventCheck (GSource *);
gboolean messageEventDispatch (GSource *, GSourceFunc, gpointer);

#define MSG_STRING_SIZE 256


enum _threadmessage
{
	TM_INFO,							// show information
	TM_INFO_LO,                         // Info on LO
	TM_INFO_HIGHLIGHT,					// show information with a highlight (green)
    TM_ERROR,							// show error message (in red)
    TM_ERROR_LO,                           // show error message (in red)

	TM_COMPLETE_GPIB,					// update widgets based on GPIB connection
	TM_REFRESH_PLOT,
	TM_SAVE_SETUP,						// save calibration and setup to database

	TG_SETUP_GPIB,						// configure GPIB
	TG_REINITIALIZE_GPIB,				// If GPIB options are changed
	TG_SETUP_EXT_LO_GPIB,               // configure GPIB for the LO
	TG_SEND_SETTINGS_to_HP8970,         // Send changed settings to the HP8970
	TG_SWEEP_HP8970,			        // Get Frequency, Gain, Noise Figure data from HP8970
    TG_SPOT_HP8970,                     // Get Frequency, Gain, Noise Figure data from HP8970
	TG_SEND_ENR_TABLE_TO_HP8970,        // Send ENR table to HP8970
    TG_CALIBRATE,                       // Run HP8970 calibration

	TG_UTILITY,
	TG_ABORT,
	TG_ABORT_CLEAR,
	TG_END								// end thread
};

typedef struct
{
    enum _threadmessage command;
    gchar *		sMessage;
    void  *		data;
    gint		dataLength;
} messageEventData;


extern GSourceFuncs 	messageEventFunctions;


void postMessageToMainLoop (enum _threadmessage Command, gchar *sMessage);
void postInfoWithCount(gchar *sMessageWithFormat, gint number, gint number2);
void postDataToMainLoop (enum _threadmessage Command, void *data);
void postDataToGPIBThread (enum _threadmessage Command, void *data);

#define postInfo(x)		postMessageToMainLoop( TM_INFO, (x) )
#define postInfoLO(x)   postMessageToMainLoop( TM_INFO_LO, (x) )
#define postError(x)	{ postMessageToMainLoop( TM_ERROR, (x) ); LOG( G_LOG_LEVEL_CRITICAL, (x) ); }
#define postErrorLO(x)  { postMessageToMainLoop( TM_ERROR_LO, (x) ); LOG( G_LOG_LEVEL_CRITICAL, (x) ); }

#endif /*MESSAGEEVENT_H_*/
