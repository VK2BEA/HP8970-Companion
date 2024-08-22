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
#include "messageEvent.h"


gchar   *HP8970errorCodes[] = {
        // error codes 10 - 19
        // Hardware error
        "A/D conversion failed",
        "A/D converter overflow",
        "Input overflow",
        "IF attenuator calibration failed",
        "Proper IF or RF attenuators cannot be selected",
        NULL,
        NULL,
        NULL,
        "Frequency calibration failed",
        NULL,

        // error codes 20 - 29
        // Not Properly Calibrated For Corrected Measurement
        "Not calibrated",
        "Current frequency is out of calibrated range",
        "Current RF attenuation not calibrated",
        "Not calibrated in the current measurement and sideband modes",
        "Not calibrated for the current IF", // (Measurement Modes 1.1 and 1.3)
        "Not calibrated for the current LO frequency", // (Measurement mode 1.2)
        "Internal IF attenuators not calibrated",
        "Overflow while calibrating",
        NULL,
        NULL,

        // error codes 30 - 39
        // Invalid Frequency Error
        "Start frequency is greater than stop frequency during calibration "
            "or plot. Or, the lower limit is greater than the upper limit (noise or gain) during sweep",
        "Number of calibration points exceeds 81 (A) or 181 (B)",
        "LO frequency will be out range",
        "IF will be out of range",
        "Double sideband is not allowed in Measurement Mode 1.2"
        // Entry Error
        "Entered value is out of range",
        "Undefined special function",
        "Cannot enter specified  Select proper function that parameter",
        NULL,
        NULL,

        // error codes 40 - 43
        // HP-IB errors
        "Undefined HP-IB code",
        "Invalid HP-IB characters",
        "No external LO is connected",
        "Codes received while in Talk Only Mode" // (4.2 SP)

        // error codes 50 - 79 Service-related errors
        // error code 80 Continuous memory failure

};

#define DATA_ERROR  100
const gchar *
HP8970errorString( gint errorCode ) {
    if( (errorCode < 10 || (errorCode > 43 && errorCode < 50)) && (errorCode != 80 || errorCode != DATA_ERROR) )
        return "Undocumented error";
    else if( errorCode == DATA_ERROR )
        return "Received data error";
    else if( errorCode == 80 )
        return "Continuous memory failure";
    else if( errorCode == 99 )
        return "Measurement Overflow";
    else if( errorCode > 50 && errorCode < 80 )
        return "Service-related error";
    else
        return HP8970errorCodes[ errorCode - 10 ];
}

/*!     \brief  Get freq / Noise & Gain from HP8970
 *
 * Have triggered the acquisition cycle and must retrieve the data.
 * Since there might be averaging over many cycles, this may last many seconds
 *
 * \param descGPIB_HP8970  GPIB descriptor for HP8970 device
 * \param timeoutSec	   timeout period to wait (could be long if we average)
 * \param pGPIBstatus      pointer to GPIB status
 * \param pResult          pointer to the structure receiving the data
 * \return 0 - OK, -1 - data error, 1 HP8970 error
 */
gint
HP8970getFreqNoiseGain (gint descGPIB_HP8970, gint timeoutSec, gint *pGPIBstatus, tNoiseAndGain *pResult, gint *pError) {
#define MAX_RESPONSE_SIZE   100
    gchar sResponse[MAX_RESPONSE_SIZE + 1];
    glong  nChars;
    tGPIBReadWriteStatus readResult;

    // don't parse invalid input
    readResult = GPIBasyncRead (descGPIB_HP8970, sResponse, MAX_RESPONSE_SIZE, &nChars, pGPIBstatus, timeoutSec);
    if ( readResult == eRDWT_ABORT )
        return ABORT;
    else if (readResult != eRDWT_OK)
        return ERROR;

    // null terminate
    sResponse[ nChars ] = 0;
    gint nArgs = sscanf (sResponse, "%lf,%lf,%lf", &pResult->abscissa.freq, &pResult->gain, &pResult->noise);
    if (nArgs != 3) {
        *pError = DATA_ERROR;
        return ERROR;
    } else if (pResult->noise > ERROR_INDICATOR_HP8970) {
        *pError = (pResult->noise - ERROR_INDICATOR_HP8970) / 1.0e6;
        return 1;
    } else {
        *pError = 0;
    }

    return 0;
}

/*!     \brief  Enable for SRQ for OPC
 *
 * The OPC bit in the Event Status Register mask (B0) is set to
 * trigger an SRQ (since the ESE bit (B5) in the Status Register Enable mask is set).
 *
 * \param  descGPIB_HP8970  GPIB descriptor for HP8970 device
 * \param  pGPIBstatus      pointer to GPIB status
 * \return TRUE on success or ERROR on problem
 */
tGPIBReadWriteStatus
enableSRQonDataReady (gint descGPIB_HP8970, gint *pGPIBstatus) {
    if (GPIBfailed(*pGPIBstatus))
        return eRDWT_PREVIOUS_ERROR;
    /*
     * T0 Trigger Free Run
     * T1 Trigger Hold
     * T2 Trigger Execute Measurement
     *
     * Q0 Disable SRQ capability (clears all enabled conditions)
     * Q1 Enable Data Ready
     * Q2 Enable RF Calibration Complete (not for Zero Frequency or IF Calibration)
     * Q3 Enable HP-IB Code Error
     * Q6 Enable Instrument Error
     */
    return GPIBasyncWrite (descGPIB_HP8970, "Q0T1Q1Q2Q3Q6", pGPIBstatus, 10 * TIMEOUT_RW_1SEC);
}

/*!     \brief  Trigger then wait for SRQ
 *
 * The trigger will result in a data return and/or error.
 * We send the trigger, then wait for the SRQ indicating data is ready or an error.
 * A serial poll will clear the SRQ.
 *
 * \param descGPIB_HP8970  GPIB descriptor for HP8970 device
 * \param pResult          pointer to the structure receiving the data
 * \param pGPIBstatus      pointer to GPIB status
 * \param timeoutSecs      timeout period to wait (could be long if we average)
 * \return status as a tGPIBReadWriteStatus
 */

tGPIBReadWriteStatus
GPIBtriggerMeasurement (gint descGPIB_HP8970, tNoiseAndGain *pResult, gint *pGPIBstatus, gint *pHP8970error, gdouble timeoutSecs) {

#define SRQ_EVENT       1
#define TIMEOUT_EVENT   0
#define THIRTY_MS 0.030

    tGPIBReadWriteStatus rtn = eRDWT_CONTINUE;
    int HP8790rtn;
    gdouble waitTime = 0.0;
    gint GPIBcontrollerIndex = 0;

    if (GPIBfailed(*pGPIBstatus)) {
        return eRDWT_PREVIOUS_ERROR;
    }

    // trigger the measurement
    if ( ibtrg (descGPIB_HP8970 )  & ERR ) {
        return eRDWT_ERROR;
    }

    // get the controller index
    ibask (descGPIB_HP8970, IbaBNA, &GPIBcontrollerIndex);
    // set the controller timeout
    ibtmo (GPIBcontrollerIndex, T30ms);    // timeout WaitSRQ every 30ms so we can check if abort is ordered

    DBG(eDEBUG_EXTENSIVE, "Waiting for data SRQ from HP8970");
    do {
        short waitResult = 0;
        char status = 0;
        // This will timeout every 30ms (the timeout we set for the controller)
        WaitSRQ (GPIBcontrollerIndex, &waitResult);
#define ST_RQS			0x40
#define ST_INST_ERR		0x20
#define ST_HPIB_ERR		0x04
#define ST_CAL			0x02
#define	ST_DATA_READY	0x01
        if (waitResult == SRQ_EVENT) {
            // This actually is an SRQ ..  is it from the HP8970 ?
            // Serial poll for status to reset SRQ and find out if it was the HP8970
            if ((*pGPIBstatus = ibrsp (descGPIB_HP8970, &status)) & ERR) {
                LOG(G_LOG_LEVEL_CRITICAL, "HPIB serial poll fail %04X/%d", *pGPIBstatus, AsyncIberr ());
                rtn = eRDWT_ERROR;
            } else if (status & ST_RQS) {
                if( status & ST_HPIB_ERR ) {
                    rtn = eRDWT_ERROR;
                } else if( status & ST_INST_ERR ) {
                    // Read the error
                    HP8790rtn = HP8970getFreqNoiseGain ( descGPIB_HP8970, timeoutSecs, pGPIBstatus, pResult, pHP8970error);
                    if( HP8790rtn == ABORT )
                        rtn = eRDWT_ABORT;
                    else if( HP8790rtn != ERROR ) {
                        rtn = eRDWT_OK;
                    } else {
                        rtn = eRDWT_ERROR;
                    }
                    // not really an error
                    if( *pHP8970error == 99 )
                        rtn = eRDWT_OK;
                    else
                        rtn = eRDWT_ERROR;
                } else if( status & ST_DATA_READY ) {
                    HP8790rtn = HP8970getFreqNoiseGain ( descGPIB_HP8970, timeoutSecs, pGPIBstatus, pResult, pHP8970error);
                    if( HP8790rtn == ABORT )
                        rtn = eRDWT_ABORT;
                    else if( HP8790rtn != ERROR ) {
                        rtn = eRDWT_OK;
                    } else {
                        if( *pHP8970error == 99 )
                            rtn = eRDWT_OK;
                        else
                            rtn = eRDWT_ERROR;
                    }
                }
                if( rtn == eRDWT_CONTINUE && (status & ST_CAL) )
                    rtn = CAL_COMPLETE;
            } else {
                DBG(eDEBUG_ALWAYS, "No SRQ from HP8970 but SRQ triggered", status);
            }
            // its not the HP8970 ... some other GPIB device is requesting service
        } else {
            // It's a 30ms timeout .. if we get an abort/end message on the queue - return
            if (checkMessageQueue (NULL) == SEVER_DIPLOMATIC_RELATIONS) {
                // This will stop future GPIB commands for this sequence
                *pGPIBstatus |= ERR;
                rtn = eRDWT_ABORT;
            }
        }

        waitTime += THIRTY_MS;
        if (waitTime > FIVE_SECONDS && fmod (waitTime, 1.0) < THIRTY_MS) {
            gchar *sMessage;
            if (timeoutSecs > 15) {    // this means we have a "WAIT;" message .. so show the estimated time
                sMessage = g_strdup_printf ("✳️ Waiting for HP8970 : %ds / %.0lfs", (gint) (waitTime),
                                            (double) timeoutSecs / TIMEOUT_SAFETY_FACTOR);
            } else {
                sMessage = g_strdup_printf ("✳️ Waiting for HP8970 : %ds", (gint) (waitTime));
            }
            postInfo(sMessage);
            g_free (sMessage);
        }
    }
    while (rtn == eRDWT_CONTINUE && (globalData.flags.bNoGPIBtimeout || waitTime < timeoutSecs));

    if (rtn == eRDWT_OK) {
        DBG(eDEBUG_EXTENSIVE, "SRQ asserted and acknowledged");
    } else {
        DBG(eDEBUG_ALWAYS, "SRQ error waiting: %04X/%d", ibsta, iberr);
    }

    if (rtn == eRDWT_CONTINUE) {
        *pGPIBstatus |= ERR_TIMEOUT;
        return (eRDWT_TIMEOUT);
    } else {
        return (rtn);
    }
}


