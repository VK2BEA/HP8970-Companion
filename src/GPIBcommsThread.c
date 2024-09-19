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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include <glib-2.0/glib.h>
#include <gpib/ib.h>
#include <errno.h>
#include <HP8970.h>
#include <locale.h>

#include "GPIBcomms.h"
#include "HP8970comms.h"
#include "messageEvent.h"

/*!     \brief  See if there are messages on the asynchronous queue
 *
 * If the argument contains a pointer to a queue, set the default queue to it
 *
 * Check the queue and report the number of messages
 *
 * \param asyncQueue  pointer to async queue (or NULL)
 * \return            number of messages in queue
 */
gint
checkMessageQueue (GAsyncQueue *asyncQueue) {
    static GAsyncQueue *queueToCheck = NULL;
    int queueLength;

    if (asyncQueue)
        queueToCheck = asyncQueue;

    if (!asyncQueue && queueToCheck) {
        queueLength = g_async_queue_length (queueToCheck);
        if (queueLength > 0) {
            messageEventData *message = g_async_queue_pop (queueToCheck);
            gint command = message->command;
            g_async_queue_push_front (queueToCheck, message);
            if (command == TG_ABORT || command == TG_ABORT_CLEAR || command == TG_END)
                return SEVER_DIPLOMATIC_RELATIONS;
        } else {
            return queueLength;
        }
        return queueLength;
    } else {
        return 0;
    }
}

/*!     \brief  Write binary data from the GPIB device asynchronously
 *
 * Write data from the GPIB device asynchronously while checking for exceptions
 * This is needed when it is anticipated that the response will take some time.
 *
 * \param GPIBdescriptor GPIB device descriptor
 * \param sData          pointer to data to write
 * \param length         number of bytes to write
 * \param pGPIBstatus    pointer to GPIB status
 * \param timeoutSecs    the maximum time to wait before abandoning
 * \return               read status result
 */
tGPIBReadWriteStatus
GPIBasyncWriteBinary (gint GPIBdescriptor, const void *sData, gint length, gint *pGPIBstatus, gdouble timeoutSecs) {
    gdouble waitTime = 0.0;
    tGPIBReadWriteStatus rtn = eRDWT_CONTINUE;

    if (GPIBfailed(*pGPIBstatus)) {
        return eRDWT_PREVIOUS_ERROR;
    }

    ibtmo (GPIBdescriptor, TNONE);

    *pGPIBstatus = ibwrta (GPIBdescriptor, sData, length);

    if (GPIBfailed(*pGPIBstatus))
        return eRDWT_ERROR;
#if !GPIB_CHECK_VERSION(4,3,6)
    //todo - remove when linux GPIB driver fixed
    // a bug in the drive means that the timout used for the ibrda command is not accessed immediatly
    // we delay, so that the timeout used is TNONE bedore changing to T30ms
    usleep(20 * 1000);
#endif

    // set the timout for the ibwait to 30ms
    ibtmo (GPIBdescriptor, T30ms);
    do {
        // Wait for read completion or timeout
        *pGPIBstatus = ibwait (GPIBdescriptor, TIMO | CMPL | END);
        if ((*pGPIBstatus & TIMO) == TIMO) {
            // Timeout
            rtn = eRDWT_CONTINUE;
            waitTime += THIRTY_MS;
            if (waitTime > FIVE_SECONDS && fmod (waitTime, 1.0) < THIRTY_MS) {
                gchar *sMessage = g_strdup_printf ("✍🏻 Waiting for HP8970: %ds", (gint) (waitTime));
                postInfo(sMessage);
                g_free (sMessage);
            }
        } else {
            // did we have a read error
            if ((*pGPIBstatus & ERR) == ERR)
                rtn = eRDWT_ERROR;
            // or did we complete the read
            else if ((*pGPIBstatus & CMPL) == CMPL || (*pGPIBstatus & END) == END)
                rtn = eRDWT_OK;
        }
        // If we get a message on the queue, it is assumed to be an abort
        if (checkMessageQueue ( NULL) == SEVER_DIPLOMATIC_RELATIONS) {
            // This will stop future GPIB commands for this sequence
            *pGPIBstatus |= ERR;
            rtn = eRDWT_ABORT;
        }
    }
    while (rtn == eRDWT_CONTINUE && (globalData.flags.bNoGPIBtimeout || waitTime < timeoutSecs));

    if (rtn != eRDWT_OK)
        ibstop (GPIBdescriptor);

    *pGPIBstatus = AsyncIbsta ();

    DBG(eDEBUG_EXTREME, "🖊 HP8970: %d / %d bytes", AsyncIbcnt (), length);

    if ((*pGPIBstatus & CMPL) != CMPL) {
        if (waitTime >= timeoutSecs)
            LOG(G_LOG_LEVEL_CRITICAL, "GPIB async write timeout after %.2f sec. status %04X", timeoutSecs, *pGPIBstatus);
        else
            LOG(G_LOG_LEVEL_CRITICAL, "GPIB async write status/error: %04X/%d", *pGPIBstatus, AsyncIberr ());
    }

    if (waitTime > FIVE_SECONDS)
        postInfo("");

    if (rtn == eRDWT_CONTINUE) {
        *pGPIBstatus |= ERR_TIMEOUT;
        return (eRDWT_TIMEOUT);
    } else {
        return (rtn);
    }
}

/*!     \brief  Write (async) string to the GPIB device
 *
 * Send NULL terminated string to the GPIB device (asynchronously)
 *
 * \param GPIBdescriptor GPIB device descriptor
 * \param sData          data to send
 * \param pGPIBstatus    pointer to GPIB status
 * \param timeoutSecs    the maximum time to wait before abandoning
 * \return               count or ERROR
 */
tGPIBReadWriteStatus
GPIBasyncWrite (gint GPIBdescriptor, const void *sData, gint *pGPIBstatus, gdouble timeoutSecs) {
    DBG(eDEBUG_EXTREME, "🖊 HP8970: %s", sData);
    return GPIBasyncWriteBinary (GPIBdescriptor, sData, strlen ((gchar*) sData), pGPIBstatus, timeoutSecs);
}

/*!     \brief  Write string with a numeric value to the GPIB device
 *
 * Send NULL terminated string with numeric paramater to the GPIB device
 *
 * \param GPIBdescriptor GPIB device descriptor
 * \param sData          data to send
 * \param number         number to embed in string (%d)
 * \param pGPIBstatus    pointer to GPIB status
 * \param timeout        seconds to timeout
 * \return               count or ERROR (from GPIBasyncWriteBinary)
 */
tGPIBReadWriteStatus
GPIBasyncWriteNumber (gint GPIBdescriptor, const void *sData, gint number, gint *GPIBstatus, double timeout) {
    gchar *sCmd = g_strdup_printf (sData, number);
    DBG(eDEBUG_EXTREME, "👉 HP8970: %s", sCmd);
    tGPIBReadWriteStatus rtnStatus = GPIBasyncWrite (GPIBdescriptor, sCmd, GPIBstatus, timeout);
    g_free (sCmd);
    return rtnStatus;
}

/*!     \brief  Read data from the GPIB device asynchronously
 *
 * Read data from the GPIB device asynchronously while checking for exceptions
 * This is needed when it is anticipated that the response will take some time.
 *
 * \param GPIBdescriptor GPIB device descriptor
 * \param readBuffer     pointer to data to save read data
 * \param maxBytes       maxium number of bytes to read
 * \param pNbytesRead    pointer top the location to store the number of characters read
 * \param pGPIBstatus    pointer to GPIB status
 * \param timeoutSecs        the maximum time to wait before abandoning
 * \return               read status result
 */
tGPIBReadWriteStatus
GPIBasyncRead (gint GPIBdescriptor, void *readBuffer, glong maxBytes, glong *pNbytesRead, gint *pGPIBstatus, gdouble timeoutSecs) {

    gdouble waitTime = 0.0;
    tGPIBReadWriteStatus rtn = eRDWT_CONTINUE;

    if (GPIBfailed(*pGPIBstatus)) {
        return eRDWT_PREVIOUS_ERROR;
    }

    // for the read itself we have no timeout .. we loop using ibwait with short timeout
    ibtmo (GPIBdescriptor, TNONE);
    *pGPIBstatus = ibrda (GPIBdescriptor, readBuffer, maxBytes);

    if (GPIBfailed(*pGPIBstatus))
        return eRDWT_ERROR;

#if !GPIB_CHECK_VERSION(4,3,6)
    //todo - remove when linux GPIB driver fixed
    // a bug in the drive means that the timout used for the ibrda command is not accessed immediatly
    // we delay, so that the timeout used is TNONE bedore changing to T30ms
    usleep(20 * 1000);
#endif

    // set the timout for the ibwait to 30ms
    ibtmo (GPIBdescriptor, T30ms);
    do {
        // Wait for read completion or timeout
        *pGPIBstatus = ibwait (GPIBdescriptor, TIMO | CMPL | END);
        if ((*pGPIBstatus & TIMO) == TIMO) {
            // Timeout
            rtn = eRDWT_CONTINUE;
            waitTime += THIRTY_MS;
            if (waitTime > FIVE_SECONDS && fmod (waitTime, 1.0) < THIRTY_MS) {
                gchar *sMessage = g_strdup_printf ("👀 Waiting for HP8970: %ds", (gint) (waitTime));
                postInfo(sMessage);
                g_free (sMessage);
            }
        } else {
            // did we have a read error
            if ((*pGPIBstatus & ERR) == ERR)
                rtn = eRDWT_ERROR;
            // or did we complete the read
            else if ((*pGPIBstatus & CMPL) == CMPL || (*pGPIBstatus & END) == END)
                rtn = eRDWT_OK;
        }
        // If we get a message on the queue, it is assumed to be an abort
        if (checkMessageQueue ( NULL) == SEVER_DIPLOMATIC_RELATIONS) {
            // This will stop future GPIB commands for this sequence
            *pGPIBstatus |= ERR;
            rtn = eRDWT_ABORT;
        }
    }
    while (rtn == eRDWT_CONTINUE && (globalData.flags.bNoGPIBtimeout || waitTime < timeoutSecs));

    if (rtn != eRDWT_OK)
        ibstop (GPIBdescriptor);

    if (pNbytesRead)
        *pNbytesRead = AsyncIbcnt ();
    *pGPIBstatus = AsyncIbsta ();

    DBG(eDEBUG_EXTREME, "👓 HP8970: %d bytes (%d max)", AsyncIbcnt (), maxBytes);

    if ((*pGPIBstatus & CMPL) != CMPL) {
        if (timeoutSecs != TIMEOUT_NONE && waitTime >= timeoutSecs)
            LOG(G_LOG_LEVEL_CRITICAL, "GPIB async read timeout after %.2f sec. status %04X", timeoutSecs, *pGPIBstatus);
        else
            LOG(G_LOG_LEVEL_CRITICAL, "GPIB async read status/error: %04X/%d", *pGPIBstatus, AsyncIberr ());
    }

    if (waitTime > FIVE_SECONDS)
        postInfo("");

    if (rtn == eRDWT_CONTINUE) {
        *pGPIBstatus |= ERR_TIMEOUT;
        return (eRDWT_TIMEOUT);
    } else {
        return (rtn);
    }
}

/*!     \brief  Read configuration value from the GPIB device
 *
 * Read configuration value for GPIB device
 *
 * \param GPIBdescriptor GPIB device descriptor
 * \param option         the paramater to read
 * \param result         pointer to the integer receiving the value
 * \param pGPIBstatus    pointer to GPIB status
 * \return               OK or ERROR
 */
int
GPIBreadConfiguration (gint GPIBdescriptor, gint option, gint *result, gint *pGPIBstatus) {
    *pGPIBstatus = ibask (GPIBdescriptor, option, result);

    if (GPIBfailed(*pGPIBstatus))
        return ERROR;
    else
        return OK;
}

/*!     \brief  Free resources from an IPC message
 *
 * Free resources from an IPC message
 *
 * \param pMessage        poinetr to message event
 */
void
freeMessage( messageEventData *pMessage )
{
    g_free (pMessage->sMessage);
    g_free (pMessage->data);
    g_free (pMessage);
}

/*!     \brief  Ping GPIB device
 *
 * Checks for the presence of a device, by attempting to address it as a listener.
 *
 * \param descGPIBdevice GPIB device descriptor
 * \param pGPIBstatus    pointer to GPIB status
 * \return               TRUE if device responds or FALSE if not
 */
static gboolean
pingGPIBdevice (gint descGPIBdevice, gint *pGPIBstatus) {
    gint PID = INVALID;
    gint timeout;
    gint descGPIBboard = INVALID;

    gshort bFound = FALSE;

    // Get the device PID
    if ((*pGPIBstatus = ibask (descGPIBdevice, IbaPAD, &PID)) & ERR)
        goto err;
    // Get the board number
    if ((*pGPIBstatus = ibask (descGPIBdevice, IbaBNA, &descGPIBboard)) & ERR)
        goto err;

    // save old timeout
    if ((*pGPIBstatus = ibask (descGPIBboard, IbaTMO, &timeout)) & ERR)
        goto err;
    // set new timeout (for ping purpose only)
    if ((*pGPIBstatus = ibtmo (descGPIBboard, T3s)) & ERR)
        goto err;

    // Actually do the ping
    if ((*pGPIBstatus = ibln (descGPIBboard, PID, NO_SAD, &bFound)) & ERR) {
        DBG(eDEBUG_EXTENSIVE, "🖊 HP8970: ping to %d failed (status: %04x, error %04x)", PID, *pGPIBstatus, ThreadIberr ());
        goto err;
    }

    *pGPIBstatus = ibtmo (descGPIBboard, timeout);

err: return (bFound);
}

#define GPIB_EOI		TRUE
#define	GPIB_EOS_NONE	0

/*!     \brief  open the GPIB device
 *
 * Get the device descriptors 8790 GPIB device
 * based on the parameters set (whether to use descriptors or GPIB addresses)
 *
 * \param pGlobal             pointer to global data structure
 * \param pDescGPIB_HP8753    pointer to GPIB device descriptor
 * \return                    0 on success or ERROR on failure
 */
gint
open_8790_GPIBdevice (tGlobal *pGlobal, gint *pDescGPIB_HP8970) {
    gint GPIBstatus = 0;

    // The board index can be used as a device descriptor; however,
    // if a device descriptor was returned from the ibfind, it must be freed
    // with ibnol().
    // The board index corresponds to the minor number /dev/
    // $ ls -l /dev/gpib0
    // crw-rw----+ 1 root root 160, 0 May 12 09:24 /dev/gpib0
#define FIRST_ALLOCATED_CONTROLLER_DESCRIPTOR 16
    // raise(SIGSEGV);

    if (*pDescGPIB_HP8970 != INVALID) {
        ibonl (*pDescGPIB_HP8970, 0);
    }

    *pDescGPIB_HP8970 = INVALID;

    // Look for the HP8970
    if (pGlobal->flags.bGPIB_UseCardNoAndPID) {
        if (pGlobal->GPIBcontrollerIndex >= 0 && pGlobal->GPIBdevicePID >= 0)
            *pDescGPIB_HP8970 = ibdev (pGlobal->GPIBcontrollerIndex, pGlobal->GPIBdevicePID, 0, T3s,
                                       GPIB_EOI, GPIB_EOS_NONE);
        else {
            postError("Bad GPIB controller or device number");
            return ERROR;
        }
    } else {
        if( (*pDescGPIB_HP8970 = ibfind (pGlobal->sGPIBdeviceName)) != ERROR ) {
            ibeot (*pDescGPIB_HP8970, GPIB_EOI);
            ibeos (*pDescGPIB_HP8970, GPIB_EOS_NONE);
        }
    }

    if (*pDescGPIB_HP8970 == ERROR) {
        postError("Cannot find HP8970");
        return ERROR;
    }

    if (!pingGPIBdevice (*pDescGPIB_HP8970, &GPIBstatus)) {
        postError("Cannot contact HP8970");
        return ERROR;
    } else {
        postInfo("Contact with HP8970 established");
        ibloc (*pDescGPIB_HP8970);
        usleep ( LOCAL_DELAYms * 1000);
    }
    return 0;
}

/*!     \brief  open the External LO GPIB device
 *
 * Get the device descriptors of the GPIB device
 * based on the parameters set (whether to use descriptors or GPIB addresses)
 *
 * \param pGlobal             pointer to global data structure
 * \param pDescGPIB_ExtLO     pointer to GPIB device descriptor
 * \return                    0 on success or ERROR on failure
 */
gint
open_ExtLO_GPIBdevice (tGlobal *pGlobal, gint *pDescGPIB_ExtLO) {
    gint GPIBstatus = 0;

    // The board index can be used as a device descriptor; however,
    // if a device descriptor was returned from the ibfind, it must be freed
    // with ibnol().
    // The board index corresponds to the minor number /dev/
    // $ ls -l /dev/gpib0
    // crw-rw----+ 1 root root 160, 0 May 12 09:24 /dev/gpib0
#define FIRST_ALLOCATED_CONTROLLER_DESCRIPTOR 16
    // raise(SIGSEGV);

    if (*pDescGPIB_ExtLO != INVALID) {
        ibonl (*pDescGPIB_ExtLO, 0);
    }

    *pDescGPIB_ExtLO = INVALID;

    // Look for the HP8970
    if (pGlobal->flags.bGPIB_extLO_usePID) {
        if (pGlobal->GPIBcontrollerIndex >= 0 && pGlobal->GPIB_extLO_PID >= 0)
            *pDescGPIB_ExtLO = ibdev (pGlobal->GPIBcontrollerIndex, pGlobal->GPIB_extLO_PID, 0, T3s,
                                       GPIB_EOI, GPIB_EOS_NONE);
        else {
            postError("Bad GPIB controller or LO device number");
            return ERROR;
        }
    } else {
        if( (*pDescGPIB_ExtLO = ibfind (pGlobal->sGPIBextLOdeviceName)) != 0 ) {
            ibeot (*pDescGPIB_ExtLO, GPIB_EOI);
            ibeos (*pDescGPIB_ExtLO, GPIB_EOS_NONE);
        }
    }

    if (*pDescGPIB_ExtLO == ERROR) {
        postError("Cannot find External LO");
        return ERROR;
    }

    if (!pingGPIBdevice (*pDescGPIB_ExtLO, &GPIBstatus)) {
        postError("Cannot contact External LO");
        return ERROR;
    } else {
        postInfo("Contact with External LO established");
        ibloc (*pDescGPIB_ExtLO);
        usleep ( LOCAL_DELAYms * 1000);
    }
    return 0;
}


/*!     \brief  close the GPIB devices
 *
 * Close the controller if it was opened and close the device
 *
 * \param pDescGPIB    pointer to GPIB device descriptor
 */
gint
GPIBclose (gint *pDescGPIB) {
    gint GPIBstatusDevice = 0;

    if (*pDescGPIB != INVALID) {
        GPIBstatusDevice = ibonl (*pDescGPIB, 0);
        *pDescGPIB = INVALID;
    }

    return GPIBstatusDevice;
}

/*!     \brief  Get real time in ms
 *
 * Get the time in milliseconds
 *
 * \return       the current time in milliseconds
 */
gulong
now_milliSeconds () {
    struct timespec now;
    clock_gettime (CLOCK_REALTIME, &now);
    return now.tv_sec * 1.0e3 + now.tv_nsec / 1.0e6;
}

/*!     \brief  Update the min / max values in the circular buffer
 *
 * Update the min / max values based on the current data point
 *
 * \param current data value
 * \param pMin    pointer to the minimum value of the data set
 * \param pMax    pointer to the maximum value of the data set
 */
void
updateBoundaries( gdouble current, gdouble *pMin, gdouble *pMax ) {
    if( current >= ERROR_INDICATOR_HP8970 )
        return;
    if( *pMin == UNINITIALIZED_DOUBLE || current < *pMin )
        *pMin = current;
    if( *pMax == UNINITIALIZED_DOUBLE || current > *pMax )
        *pMax = current;
}

/*!     \brief  Compare two frequencies in the tNoiseAndGain data set
 *
 * Compare two frequencies in the data set
 *
 * \param a : pointer to the first tNoiseAndGain structure
 * \param b : pointer to the second tNoiseAndGain structure
 * \return  0 equal - b greater than a and + if a greater than b
 */
int
cmpDataPointFrequency( const void * a, const void * b )
{
    return( ((tNoiseAndGain *)a)->abscissa.freq - ((tNoiseAndGain *)b)->abscissa.freq );
}

/*!     \brief  Take a snap-shot of the settings when the trace is saved
 *
 * Take a snap-shot of the settings when the trace is saved so that it can be
 * restored with the trace.
 *
 *
 * \param pGlobal : pointer to the the global data
 */
void
snapshotSettings( tGlobal *pGlobal ) {

    gboolean bExtLO = !(pGlobal->HP8970settings.mode == eMode1_0 || pGlobal->HP8970settings.mode == eMode1_4);

    pGlobal->plot.freqSpotMHz = pGlobal->HP8970settings.range[ bExtLO ].freqSpotMHz;
    pGlobal->plot.freqStartMHz = pGlobal->HP8970settings.range[ bExtLO ].freqStartMHz;
    pGlobal->plot.freqStopMHz = pGlobal->HP8970settings.range[ bExtLO ].freqStopMHz;
    pGlobal->plot.freqStepCalMHz = pGlobal->HP8970settings.range[ bExtLO ].freqStepCalMHz;
    pGlobal->plot.freqStepSweepMHz = pGlobal->HP8970settings.range[ bExtLO ].freqStepSweepMHz;

    pGlobal->plot.mode = pGlobal->HP8970settings.mode;
    pGlobal->plot.noiseUnits = pGlobal->HP8970settings.noiseUnits;
    pGlobal->plot.smoothingFactor = pGlobal->HP8970settings.smoothingFactor;

    pGlobal->plot.extLOfreqIF = pGlobal->HP8970settings.extLOfreqIF;
    pGlobal->plot.extLOfreqLO = pGlobal->HP8970settings.extLOfreqLO;
    pGlobal->plot.settlingTime_ms = pGlobal->HP8970settings.settlingTime_ms;

    // External LO
    g_free( pGlobal->plot.sExtLOsetFreq );
    pGlobal->plot.sExtLOsetFreq = g_strdup( pGlobal->HP8970settings.sExtLOsetFreq );
    g_free( pGlobal->plot.sExtLOsetFreq );
    pGlobal->plot.sExtLOsetFreq = g_strdup( pGlobal->HP8970settings.sExtLOsetFreq );

    pGlobal->plot.extLOsideband = pGlobal->HP8970settings.extLOsideband;

    pGlobal->plot.lossBeforeDUT = pGlobal->HP8970settings.lossBeforeDUT;
    pGlobal->plot.lossAfterDUT = pGlobal->HP8970settings.lossAfterDUT;
    pGlobal->plot.lossTemp = pGlobal->HP8970settings.lossTemp;
    pGlobal->plot.coldTemp = pGlobal->HP8970settings.coldTemp;

    pGlobal->plot.flags.bLossCompensation = pGlobal->HP8970settings.switches.bLossCompensation;
    pGlobal->plot.flags.bDataCorrectedNFAndGain = pGlobal->HP8970settings.switches.bCorrectedNFAndGain;
}

/*!     \brief  Thread to communicate with GPIB
 *
 * Start thread before asynchronous GPIB communication
 *
 * \param _pGlobal : pointer to structure holding global variables
 * \return       0 for success and ERROR on problem
 */
gpointer
threadGPIB (gpointer _pGlobal) {
    tGlobal *pGlobal = (tGlobal*) _pGlobal;

    gchar *sGPIBversion = NULL;
    gint verMajor, verMinor, verMicro;
    gint GPIBstatus, LO_GPIBstatus;
    gint timeoutHP8970;   				// previous timeout

    gint descGPIB_HP8970 = ERROR;
    gint descGPIB_extLO = ERROR;
    messageEventData *message;
    gboolean bRunning = TRUE;
    gboolean bSimulatedCommand;
    gulong __attribute__((unused)) datum = 0;
    GString *pstCommands;
    gchar HP8970status;
#define DEFAULT_MSG_TIMEOUT 2000
#define MINIMAL_MSG_TIMEOUT 1
    gint messageTimeout = DEFAULT_MSG_TIMEOUT;

    gboolean bNewSettings;
    tUpdateFlags updateFlags;
    tMode mode;

    // The HP8970 formats numbers like 3.141 not, the continental European way 3,14159
    setlocale (LC_NUMERIC, "C");
    ibvers (&sGPIBversion);
    LOG(G_LOG_LEVEL_CRITICAL, sGPIBversion);
    if (sGPIBversion && sscanf (sGPIBversion, "%d.%d.%d", &verMajor, &verMinor, &verMicro) == 3) {
        pGlobal->GPIBversion = verMajor * 10000 + verMinor * 100 + verMicro;
    }
    // g_print( "Linux GPIB version: %s\n", sGPIBversion );

    // look for the hp82357b GBIB controller
    // it is defined in /usr/local/etc/gpib.conf which can be overridden with the IB_CONFIG environment variable
    //
    //	interface {
    //    	minor = 0                       /* board index, minor = 0 uses /dev/gpib0, minor = 1 uses /dev/gpib1, etc.	*/
    //    	board_type = "agilent_82357b"   /* type of interface board being used 										*/
    //    	name = "hp82357b"               /* optional name, allows you to get a board descriptor using ibfind() 		*/
    //    	pad = 0                         /* primary address of interface             								*/
    //		sad = 0                         /* secondary address of interface          								    */
    //		timeout = T100ms                /* timeout for commands 												    */
    //		master = yes                    /* interface board is system controller 								    */
    //	}

    // loop waiting for messages from the main loop

    // Set the default queue to check for interruptions to async GPIB reads
    checkMessageQueue (pGlobal->messageQueueToGPIB);

    do {
        message = g_async_queue_timeout_pop (pGlobal->messageQueueToGPIB, ms( messageTimeout ));
        // Reset message timeout
        messageTimeout = DEFAULT_MSG_TIMEOUT;
        // See if it is a timeout
        if( message == NULL ) {
            // Timeout
            if( pGlobal->HP8970settings.updateFlags.all == 0 ) {
                // No pending settings .. so loop and wait
                continue;
            } else {
                // The only reason that there is pending data but no async message is if the
                // message was already acted upon but the connection to the instrument had failed.

                // if there is pending data - try to re-establish connection every 1/2 second, and send settings if successful
                message = g_malloc0(sizeof(messageEventData));
                message->command = TG_SEND_SETTINGS_to_HP8970;
                bSimulatedCommand = TRUE;
            }
        } else {
            bSimulatedCommand = FALSE;
        }

        // Reset the status ..  GPIB_AsyncRead & GBIPwrte will not proceed if this
        // shows an error
        GPIBstatus = 0;

        switch (message->command)
            {
            case TG_SETUP_GPIB:
            case TG_REINITIALIZE_GPIB:
                if( open_8790_GPIBdevice (pGlobal, &descGPIB_HP8970) == OK ) {
                    pGlobal->HP8970settings.updateFlags.all = ALL_FUNCTIONS;
                    message->command = TG_SEND_SETTINGS_to_HP8970;
                } else {
                    freeMessage( message );
                    continue;
                }
                break;
            case TG_END:
                GPIBclose (&descGPIB_HP8970);
                GPIBclose (&descGPIB_extLO);
                freeMessage( message );
                bRunning = FALSE;
                continue;
            default:
                if (descGPIB_HP8970 == INVALID) {
                    open_8790_GPIBdevice (pGlobal, &descGPIB_HP8970);
                }
                if ( pGlobal->flags.bNoLOcontrol == FALSE
                        && pGlobal->HP8970settings.mode != eMode1_0
                            &&  descGPIB_extLO == INVALID) {
                    open_ExtLO_GPIBdevice (pGlobal, &descGPIB_extLO);
                }
                break;
            }
#define IBLOC(x, y, z) { z = ibloc( x ); y = now_milliSeconds(); usleep( ms( LOCAL_DELAYms ) ); }
        // Most but not all commands require the GBIB
        if (descGPIB_HP8970 == INVALID) {
            postError("Cannot obtain HP8970 descriptor");
        } else if (!pingGPIBdevice (descGPIB_HP8970, &GPIBstatus)) {
            postError("HP8970 is not responding");
            ibtmo (descGPIB_HP8970, T1s);
            GPIBstatus = ibclr (descGPIB_HP8970);
            pGlobal->HP8970settings.updateFlags.all = ALL_FUNCTIONS;
            usleep (ms(250));
        } else {
            pGlobal->flags.bGPIBcommsActive = TRUE;
            GPIBstatus = ibask (descGPIB_HP8970, IbaTMO, &timeoutHP8970); /* Remember old timeout */
            ibtmo (descGPIB_HP8970, T30s);

            switch (message->command)
                {
                case TG_SETUP_EXT_LO_GPIB:
                    open_ExtLO_GPIBdevice (pGlobal, &descGPIB_extLO);
                    break;
                case TG_SEND_SETTINGS_to_HP8970:
                    bNewSettings = TRUE;

                    do {
                        pstCommands = g_string_new ( NULL );
                        gboolean bExtLO;

                        // snapshot of the settings we need to send
                        g_mutex_lock ( &pGlobal->mUpdate );
                        updateFlags = pGlobal->HP8970settings.updateFlags;
                        pGlobal->HP8970settings.updateFlags.all = 0;
                        mode = pGlobal->HP8970settings.mode;
                        bExtLO = !(mode == eMode1_0 || mode == eMode1_4);
                        g_mutex_unlock ( &pGlobal->mUpdate );

                        // Set the external signal generator (LO) for higher modes
                        if( (updateFlags.each.bSpotFrequency || updateFlags.each.bStartFrequency || updateFlags.each.bStopFrequency)
                                &&  pGlobal->flags.bNoLOcontrol == FALSE && mode != eMode1_0 ) {
                            gboolean bLOerror = TRUE;
                            gchar LOstatus;
                            gdouble signalFrequency = 0.0, LOfreq = 0.0;

                            if( updateFlags.each.bSpotFrequency )
                                signalFrequency = pGlobal->HP8970settings.range[ bExtLO ].freqSpotMHz;
                            if( updateFlags.each.bStopFrequency )
                                signalFrequency = pGlobal->HP8970settings.range[ bExtLO ].freqStopMHz;
                            else if( updateFlags.each.bStartFrequency )
                                signalFrequency = pGlobal->HP8970settings.range[ bExtLO ].freqStartMHz;

                            do {    //
                                if( pGlobal->HP8970settings.sExtLOsetup )
                                    if( GPIBasyncWrite (descGPIB_extLO, pGlobal->HP8970settings.sExtLOsetup, &LO_GPIBstatus, 10 * TIMEOUT_RW_1SEC) != eRDWT_OK )
                                        break;

                                if( ( LOfreq = LOfrequency( pGlobal, signalFrequency ) ) != 0.0 ) {
                                    g_string_printf( pstCommands, pGlobal->HP8970settings.sExtLOsetFreq, LOfreq );
                                    if( GPIBasyncWrite (descGPIB_extLO, pstCommands->str, &LO_GPIBstatus, 10 * TIMEOUT_RW_1SEC) != eRDWT_OK ) {
                                        break;
                                    }
                                    bLOerror = FALSE;
                                    LO_GPIBstatus = ibrsp (descGPIB_extLO, &LOstatus); // get the status byte from the LO
                                    gchar *sMessage = g_strdup_printf( "Signal Generator (LO): %.0lf MHz", LOfreq );
                                    postInfoLO( sMessage );
                                    g_free( sMessage );
                                }

                            } while FALSE;  // do loop one time so we can break on problem

                            if( bLOerror )
                                postErrorLO( "Communications failure with signal generator (LO)" );
                        }

                        // Send new settings to the HP8970
                        // always send the mode because if this is wrong (say preset was pressed), the
                        // frequencies may not make sense.
                        g_string_printf( pstCommands, "E%1d", pGlobal->HP8970settings.mode );
                        if( updateFlags.each.bStartFrequency )
                            g_string_append_printf( pstCommands, "FA%dMZ", (gint)pGlobal->HP8970settings.range[ bExtLO ].freqStartMHz );
                        if( updateFlags.each.bStopFrequency )
                            g_string_append_printf( pstCommands, "FB%dMZ", (gint)pGlobal->HP8970settings.range[ bExtLO ].freqStopMHz );
                        if( updateFlags.each.bStepFrequency )
                            g_string_append_printf( pstCommands, "SS%dMZ", (gint)pGlobal->HP8970settings.range[ bExtLO ].freqStepCalMHz );
                        if( updateFlags.each.bSmoothing )
                            g_string_append_printf( pstCommands, "F%1d", (gint)round( log2( pGlobal->HP8970settings.smoothingFactor ) ));
                        if( updateFlags.each.bSpotFrequency )
                            g_string_append_printf( pstCommands, "FR%dMZ", (gint)pGlobal->HP8970settings.range[ bExtLO ].freqSpotMHz );
                        if( updateFlags.each.bNoiseUnits )
                            g_string_append_printf( pstCommands, "N%1d", pGlobal->HP8970settings.noiseUnits );
                        if( updateFlags.each.bCorrection )
                            g_string_append_printf( pstCommands, "M%1d",
                                                    pGlobal->HP8970settings.switches.bCorrectedNFAndGain ? 2 : 1 );
                        if( updateFlags.each.bExternalLO )
                            g_string_append_printf( pstCommands, "IF%dMZLF%dMZB%1d", pGlobal->HP8970settings.extLOfreqIF,
                                                    pGlobal->HP8970settings.extLOfreqLO, pGlobal->HP8970settings.extLOsideband );
                        if( updateFlags.each.bLossCompenstaion )
                            g_string_append_printf( pstCommands, "D0L%1dLA%.3lfENLB%.3lfENLT%.2lfEN",
                                                    pGlobal->HP8970settings.switches.bLossCompensation,
                                                    pGlobal->HP8970settings.lossBeforeDUT, pGlobal->HP8970settings.lossAfterDUT,
                                                    pGlobal->HP8970settings.lossTemp);
                        if( updateFlags.each.bColdTemperature )
                            g_string_append_printf( pstCommands, "TC%.2lfEN", pGlobal->HP8970settings.coldTemp );

                        if( updateFlags.each.bRFattenuation )
                            g_string_append_printf( pstCommands, "R%1d", pGlobal->HP8970settings.RFattenuation );
                        if( updateFlags.each.bIFattenuation )
                            g_string_append_printf( pstCommands, "I%1d", pGlobal->HP8970settings.IFattenuation );
                        if( updateFlags.each.bHoldRFattenuator )
                            g_string_append_printf( pstCommands, "RH" );
                        if( updateFlags.each.bHoldIFattenuator )
                            g_string_append_printf( pstCommands, "IH" );

                        if( GPIBasyncWrite (descGPIB_HP8970, pstCommands->str, &GPIBstatus, 10 * TIMEOUT_RW_1SEC) != eRDWT_OK )
                            break;
                        g_string_free ( pstCommands, TRUE );
                        pstCommands = NULL;
                        g_mutex_lock ( &pGlobal->mUpdate );
                        bNewSettings = ( pGlobal->HP8970settings.updateFlags.all != 0 );
                        g_mutex_unlock ( &pGlobal->mUpdate );
                    } while ( bNewSettings );

                    if( pstCommands )
                    	g_string_free ( pstCommands, TRUE );
                    IBLOC(descGPIB_HP8970, datum, GPIBstatus);
                    break;
                case TG_CALIBRATE:
                    calibrateHP8970( pGlobal, descGPIB_HP8970, descGPIB_extLO, &GPIBstatus );
                    IBLOC(descGPIB_HP8970, datum, GPIBstatus);
                    ibrsp (descGPIB_HP8970, &HP8970status);    // Clear out status
                    break;

                case TG_FREQUENCY_CALIBRATE:
                    postInfo( "Frequency calibration started");
                    if( GPIBasyncWrite (descGPIB_HP8970, "Y2", &GPIBstatus, 10 * TIMEOUT_RW_1SEC) != eRDWT_OK ) {
                        postError( "Frequency calibration error");
                    } else {
                        postInfo( "Frequency calibration complete");
                    }
                    IBLOC(descGPIB_HP8970, datum, GPIBstatus);
                    ibrsp (descGPIB_HP8970, &HP8970status);    // Clear out status
                    break;

                case TG_SWEEP_HP8970:
                    snapshotSettings( pGlobal );
                    sweepHP8970( pGlobal, descGPIB_HP8970, descGPIB_extLO, &GPIBstatus );
                    IBLOC(descGPIB_HP8970, datum, GPIBstatus);
                    break;

                case TG_SPOT_HP8970:
                    snapshotSettings( pGlobal );
                    spotFrequencyHP8970( pGlobal, descGPIB_HP8970, descGPIB_extLO, &GPIBstatus );
                    IBLOC(descGPIB_HP8970, datum, GPIBstatus);
                    break;

                case TG_SEND_ENR_TABLE_TO_HP8970:
                    do {
                        pstCommands = g_string_new ( NULL );
                        postInfo( "Send ENR table to HP8970");
                        // For B models set the calibration table to 0
                        if( pGlobal->flags.bbHP8970Bmodel != 0 )
                            g_string_printf( pstCommands, "NDEC0EM0NR" );
                        else
                            g_string_printf( pstCommands, "NDNR" );
                        for( gint i=0; i < (pGlobal->flags.bbHP8970Bmodel == 0 ?
                                MAX_NOISE_SOURCE_ENR_DATA_LENGTH_A : MAX_NOISE_SOURCE_ENR_DATA_LENGTH); i++ ) {
                            if( pGlobal->noiseSourceCache.calibrationPoints[i][0] == 0.0 )
                                continue;

                            g_string_append_printf( pstCommands, "%.0lfEN%.3lfEN",
                                                    pGlobal->noiseSourceCache.calibrationPoints[i][0],
                                                    pGlobal->noiseSourceCache.calibrationPoints[i][1] );
                        }
                        g_string_append_printf( pstCommands, "FR" );
                        if( GPIBasyncWrite (descGPIB_HP8970, pstCommands->str, &GPIBstatus, 10 * TIMEOUT_RW_1SEC) != eRDWT_OK )
                            break;
                    } while FALSE;

                    g_string_free ( pstCommands, TRUE );

                    if( GPIBsucceeded( GPIBstatus ) )
                    	postInfo( "ENR table uploaded to HP8970");
                    else
                    	postError( "Failed to upload ENR table to HP8970");
                    IBLOC(descGPIB_HP8970, datum, GPIBstatus);
                    ibrsp (descGPIB_HP8970, &HP8970status);    // Clear out status
                    break;

                case TG_UTILITY:
                    GPIBasyncWrite (descGPIB_HP8970, "CLES", &GPIBstatus, 10 * TIMEOUT_RW_1SEC);
                    // see what's changed in the learn string after making some change on the HP8970
                    //... for diagnostic reasons only
                    {

                    }
                    IBLOC(descGPIB_HP8970, datum, GPIBstatus);
                    break;
                case TG_ABORT:
                case TG_ABORT_CLEAR:
                    postError("GPIB communication with HP8970 Aborted");
                    if( pGlobal->flags.bNoLOcontrol == FALSE && pGlobal->HP8970settings.mode != eMode1_0 )
                        postErrorLO("GPIB communication with signal generator Aborted");

                    {   // Clear the interface
                        gint boardIndex = 0;
                        ibask (descGPIB_HP8970, IbaBNA, &boardIndex);
                        ibsic (boardIndex);
                        if( descGPIB_HP8970 != INVALID ) {
							if( message->command == TG_ABORT_CLEAR ) {
								GPIBstatus = ibclr (descGPIB_HP8970);

							}
                            pGlobal->HP8970settings.updateFlags.all = ALL_FUNCTIONS;
							messageTimeout = MINIMAL_MSG_TIMEOUT;
							IBLOC(descGPIB_HP8970, datum, GPIBstatus);
                        }

                        if( descGPIB_extLO != INVALID ) {
							if( message->command == TG_ABORT_CLEAR )
								ibclr (descGPIB_extLO);
							ibloc(descGPIB_extLO);
                        }
                    }
                    break;
                default:
                    break;
                }
        }

        // restore timeout
        if( descGPIB_HP8970 == INVALID ) {
            postError("GPIB connection failure (Controller or HP8970)");
        } else {
            ibtmo (descGPIB_HP8970, timeoutHP8970);
            if (GPIBfailed(GPIBstatus)) {
                postError("GPIB error or timeout");
            }
        }

        if( !bSimulatedCommand )
            postMessageToMainLoop (TM_COMPLETE_GPIB, NULL);

        freeMessage( message );
        pGlobal->flags.bGPIBcommsActive = FALSE;
    } while (bRunning);

    return NULL;
}
