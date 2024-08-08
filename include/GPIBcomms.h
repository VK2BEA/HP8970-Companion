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

#ifndef GPIBCOMMS_H_
#define GPIBCOMMS_H_

#define CAL_COMPLETE 0x1000
typedef enum { eRDWT_OK=0, eRDWT_ERROR, eRDWT_TIMEOUT, eRDWT_ABORT, eRDWT_CONTINUE, eRDWT_PREVIOUS_ERROR } tGPIBReadWriteStatus;

gint GPIBwriteBinary( gint, const void *, gint, gint * );
gint GPIBread( gint, void *sData, gint, gint * );
gint GPIBwrite( gint, const void *, gint * );
tGPIBReadWriteStatus GPIBasyncWriteNumber( gint, const void *, gint, gint *, gdouble );
tGPIBReadWriteStatus GPIBasyncRead (gint, void *, glong, glong *, gint *, gdouble);
tGPIBReadWriteStatus GPIBasyncWrite( gint , const void *, gint *, gdouble );
tGPIBReadWriteStatus GPIBasyncWriteBinary( gint, const void *, gint , gint *, gdouble  );
tGPIBReadWriteStatus GPIBasyncSRQwrite( gint , void *, gint, gint *, gdouble );
tGPIBReadWriteStatus enableSRQonOPC( gint , gint * );
gint checkMessageQueue (GAsyncQueue *);
gint HP8970getFreqNoiseGain (gint descGPIB_HP8970, gint timeout, gint *pGPIBstatus, tNoiseAndGain *pResult, gint *pError);

#define NULL_STR	-1
#define WAIT_STR	-2
#define TIMEOUT_SAFETY_FACTOR	1.25

#define THIRTY_MS 0.030
#define FIVE_SECONDS 5.0

#define ERR_TIMEOUT (0x10000)

#define GPIBfailed(x) (((x) & (ERR | ERR_TIMEOUT)) != 0)
#define GPIBsucceeded(x) (((x) & (ERR | ERR_TIMEOUT)) == 0)

#define TIMEOUT_RW_1SEC  1.0
#define TIMEOUT_RW_1MIN 60.0
#define TIMEOUT_NONE     0.0

#endif /* GPIBCOMMS_H_ */
