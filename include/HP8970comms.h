/*
 * HP8970comms.h
 *
 *  Created on: May 22, 2024
 *      Author: michael
 */

#ifndef HP8970COMMS_H_
#define HP8970COMMS_H_

const gchar *HP8970errorString( gint );
tGPIBReadWriteStatus enableSRQonDataReady (gint, gint *);
tGPIBReadWriteStatus GPIBtriggerMeasurement (gint, tNoiseAndGain *, gint *, gint *, gdouble);


#endif /* HP8970COMMS_H_ */
