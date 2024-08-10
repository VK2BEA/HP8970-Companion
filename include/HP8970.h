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

#include <glib-2.0/glib.h>
#include <gpib/ib.h>
#include <gtk/gtk.h>
#include <cairo/cairo.h>


#ifndef VERSION
   #define VERSION "1.00-3"
#endif



#define LOG( level, message, ...) \
    g_log_structured (G_LOG_DOMAIN, level, \
		  "SYSLOG_IDENTIFIER", "HPGLplotter", \
		  "CODE_FUNC", __FUNCTION__, \
		  "CODE_LINE", G_STRINGIFY(__LINE__), \
		  "MESSAGE", message, ## __VA_ARGS__)

/*!     \brief Debugging levels
 */
enum _debug
{
        eDEBUG_ALWAYS    = 0,
        eDEBUG_INFO      = 1,
        eDEBUG_MINOR     = 3,
        eDEBUG_TESTING   = 4,
        eDEBUG_EXTENSIVE = 5,
        eDEBUG_EXTREME   = 6,
        eDEBUG_MAXIMUM   = 7
};

#define ERROR     (-1)
#define ABORT     (-2)
#define OK        ( 0)
#define CLEAR     ( 0)
#define SEVER_DIPLOMATIC_RELATIONS (-1)
#define INVALID	  (-1)
#define LAST_ITEM (-1)


#define GINT_MSTIME_TO_DOUBLE( x ) ((gdouble)(x) / 1000.0)

typedef struct {
        union {
            gdouble freq;
            gint64  time;
        } abscissa;
        gdouble	gain, noise;
	union {
	    struct {
	        guint32 bNoiseInvalid        : 1;
	        guint32 bGainInvalid         : 1;
	        guint32 bNoiseOverflow       : 1;
	        guint32 bGainOverflow        : 1;
            guint32 bCalPoint            : 1;
	    } each;
	    guint32 all;
	} flags;
} tNoiseAndGain;

typedef struct {
    gdouble lower;          // lower grid line
    gdouble upper;          // upper grid line
    gdouble decadeRange;    // range 0.1, 1.0, 10.0 etc
    gint    expandRange;    // expanded range ( 1, 2 or 5)
} tGridRange;

typedef enum {
    eFreqAbscissa=0, eTimeAbscissa=1
} tAbscissa;

typedef enum {
    eFreqOrTime=0, eTime=0, eFreq=0, eNoise=1, eGain=2, eMAX_AXES=3
} tGridAxes;


typedef union {
    struct {    // remember to change ALL_FUNCTIONS if an 'each' is added
        guint32 bFrequency        : 1;
        guint32 bStartFrequency   : 1;
        guint32 bStopFrequency    : 1;
        guint32 bStepFrequency    : 1;
        guint32 bSmoothing        : 1;
        guint32 bMode             : 1;
        guint32 bNoiseUnits       : 1;
        guint32 bColdTemperature  : 1;
        guint32 bLossCompenstaion : 1;
        guint32 bCorrection       : 1;
        guint32 bExternalLO       : 1;
    } each;
#define ALL_FUNCTIONS    0b11111111111
    guint32 all;
} tUpdateFlags;

typedef enum { ePage8970=0, ePageNotes=1, ePagePlot=2, ePageExtLO=3, ePageNoiseSource=4, ePageOptions=5, ePageGPIB=6 } tNoteBookPage;

typedef enum { e8970A=0, e8970B=1, e8970Bopt20=2, e8970_MAXmodels=3 } tModels;

typedef enum {
    eMode1_0=0, eMode1_1=1, eMode1_2=2, eMode1_3=3, eMode1_4=4, eMode_Max=5
} tMode;

typedef enum {
    eFdB=0, eF=1, eYdB=2, eY=3, eTeK=4, eMAX_NOISE_UNITS=5
} tNoiseType;

typedef enum {
    eDSB=0, eLSB=1, eUSB=2, eMAX_SIDEBAND_TYPES=3
} tSideband;

typedef struct {
    gdouble x, y;
} tCoordinate;

typedef struct {
    tNoiseAndGain  *measurementData;
    // tail is the index of the *next* item location (not the last one inserted)
    // i.e. the first item index is head, the last is tail-1
    // If there is nothing in the buffer, head == tail
    guint   head, tail;
    guint   size;

    // index ( 0 to size of measurement that is (say) 60 seconds before the measurement at the tail
    gint idxTimeBeforeTail;

    // Current plot data extremes
    gdouble     minNoise, maxNoise, minGain, maxGain;
    union {
      gdouble freq;
      gint64  time;
    } minAbscissa, maxAbscissa;

    struct {
        guint32 bTime          : 1;
    } flags;
} tCircularBuffer;

#define MAX_NOISE_SOURCE_NAME_LENGTH       50
#define MAX_NOISE_SOURCE_CALDATA_LENGTH    35
#define MAX_NOISE_SOURCE_CALDATA_LENGTH_A  27       // HP8970A has 27 points only
#define MAX_NOISE_SOURCES                   5
typedef struct {
    gchar   name[ MAX_NOISE_SOURCE_NAME_LENGTH+1 ];                     // name of noise source
    gdouble calibrationPoints[ MAX_NOISE_SOURCE_CALDATA_LENGTH ][2];    // freq / ENR
} tNoiseSource;

typedef struct {
    struct {
        // Frequencies in MHz
        gdouble freqSpotMHz;
        gdouble freqStartMHz;
        gdouble freqStopMHz;
        gdouble freqStepCalMHz;
        gdouble freqStepSweepMHz;
    } range[ 2 ];

    tUpdateFlags updateFlags;
    GMutex mUpdate;

    struct {
        guint32 bCorrectedNFAndGain     : 1;
        guint32 bLossCompensation       : 1;
        guint32 bSpotFrequency          : 1;
    } switches;

	gint	smoothingFactor;

	tNoiseType noiseUnits;

	// Mode 0 - no external mixer
	// Mode 1 - (DUT - amplifier) external mixer / variable external LO / Fixed IF
	// Mode 2 - (DUT - amplifier) external mixer / fixed LO / variable IF (USB or LSB + filtering)
	// Mode 3 - (DUT - mixer or receiver) external mixer / variable external LO / Fixed IF
    // Mode 4 - (DUT - mixer or receiver) external mixer / variable fixed LO / variable IF
	tMode mode;
	gint    extLOfreqIF, extLOfreqLO, settlingTime_ms; // for modes 1-4
    // External LO
    gchar   *sExtLOsetup, *sExtLOsetFreq;
	tSideband extLOsideband;

	gdouble  lossBeforeDUT, lossAfterDUT, lossTemp, coldTemp;

	tNoiseSource noiseSources[ MAX_NOISE_SOURCES ];
	gint activeNoiseSource;
	tNoiseSource noiseSourceCache;

} tHP8970settings;

typedef struct {
    // The min and max values will be quantized to a 2, 5, 10 grid
    // so a data min/max frequency of 121/149 will become 120/150 with MHz/Div of 2
    gdouble min, max, offset, perDiv;
} tAxis;

typedef struct {

	struct {
		guint32 bRunning         		    : 1;
		guint32 bbDebug					    : 3;
		guint32 bGPIBcommsActive		    : 1;
		guint32 bGPIB_UseCardNoAndPID       : 1;
        guint32 bGPIB_extLO_usePID          : 1;
		guint32 bNoGPIBtimeout			    : 1;
		guint32 bShowTime                   : 1;
		guint32 bShowTitle                  : 1;
        guint32 bShowHPlogo                 : 1;
        guint32 bAutoScaling                : 1;
        guint32 bLiveMarkerActive           : 1;
        guint32 bHoldLiveMarker             : 1;
        guint32 bPreviewModeDiagram         : 1;
        guint32 bNoLOcontrol                : 1;
        guint32 bCalibrationNotPossible     : 1;
#define N_VARIANTS 3
		guint32 bbHP8970Bmodel              : 2;
	} flags;

	tHP8970settings HP8970settings;

	// This structure holds the data and metadata for the plot.
	// When data is read from the HP8970 it is placed in a circular buffer.
	// This is so when in spot frequency mode an unknown length number of points
	// are read and it is not necessary to move around data to accomodate.
	struct {
	    tCircularBuffer measurementBuffer;
	    gdouble         spotFrequency;

        tNoiseType      noiseUnits;
        gint            smoothingFactor;

	    // These are required to plot the data correctly
        struct {
            guint32 bValidNoiseData             : 1;
            guint32 bValidGainData              : 1;
            guint32 bSpotFrequencyPlot          : 1;
            guint32 bCalibrationPlot            : 1;
            guint32 bDataCorrectedNFAndGain     : 1;
            guint32 bLossCompensation           : 1;
            guint32 bHighResolution             : 1;
            guint32 bbHP8970Bmodel              : 2;
        } flags;

        tAxis axis[ eMAX_AXES ];

        gchar           *sTitle, *sNotes, *sDateTime;

        // Snapshot of HP8973 settings when the plot was taken
        // These values are stored with the plot and will be used to set the
        // state of the meter when the plot is recalled.
        //
        gdouble freqSpotMHz;
        gdouble freqStartMHz;
        gdouble freqStopMHz;
        gdouble freqStepCalMHz;
        gdouble freqStepSweepMHz;
        tMode   mode;

        gint      extLOfreqIF, extLOfreqLO, settlingTime_ms; // for modes 1-4
        // External LO
        gchar    *sExtLOsetup, *sExtLOsetFreq;
        tSideband extLOsideband;

        gdouble   lossBeforeDUT, lossAfterDUT, lossTemp, coldTemp;

        // End of snapshot

	} plot;

    tCoordinate     liveMarkerPosnRatio;

	// If the grid is not auto-ranging, these are the boundaries
	gdouble         fixedGridFreq[2], // Unused ... placeholder
	                fixedGridNoise[eMAX_NOISE_UNITS][2], fixedGridGain[2];

#define N_PAPER_SIZES 4
	gint		    PDFpaperSize;

    gint            GPIBcontrollerIndex,  GPIBdevicePID, GPIB_extLO_PID;
    gchar           *sGPIBdeviceName, *sGPIBextLOdeviceName;
    gint            GPIBversion;

    GtkPrintSettings *printSettings;
    GtkPageSetup    *pageSetup;
    gchar           *sLastDirectory;

	GHashTable      *widgetHashTable;

	GSource         *messageEventSource;
	GAsyncQueue     *messageQueueToMain;
	GAsyncQueue     *messageQueueToGPIB;

	gchar           *sUsersJSONfilename;	// filename chose by user for saving HPGL file
	gchar           *sUsersPDFImageFilename;	// filename chosen by user for PDF file
	gchar           *sUsersPNGImageFilename;	// filename chosen by user for PNG file
	gchar           *sUsersSVGImageFilename;	// filename chosen by user for SVG file
    gchar           *sUsersCSVfilename;    // filename chosen by user for SVG file
	GThread         *pGThread;

} tGlobal;

typedef struct {
    guint areaWidth;
    guint areaHeight;

    gdouble margin;

    gdouble leftGridPosn;
    gdouble rightGridPosn;
    gdouble bottomGridPosn;
    gdouble topGridPosn;

    gdouble gridWidth;
    gdouble gridHeight;

    gdouble fontSize;
    gboolean bSuppressLiveMarker;

    cairo_matrix_t initialMatrix;
} tGridParameters;

typedef enum {  eColorNoise = 0, eColorGain = 1, eColorFrequency = 2,
                eColorGrid = 3, eColorGridGain = 4, eColorTitle = 5,
                eColorTBD1 = 6, eColorTBD2 = 7, eColorTBD3 = 8,
                eColorTBD4 = 9, eColorTBD5 = 10, eColorTBD6 = 11,
                eMAX_COLORS = 12
} tElementColor;

typedef enum { eA4 = 0, eLetter = 1, eA3 = 2, eTabloid = 3, eNumPaperSizes = 4 } tPaperSize;
typedef struct {
    gint   height, width;
    gdouble margin;
} tPaperDimensions;

extern tPaperDimensions paperDimensions[];


#define LABEL_FONT "Noto Sans"
#define MODE_DIAGRAM_FONT "Noto Sans"
#define MODE_DIAGRAM_FONT_CONDENSED "Noto Sans Condensed"

extern GdkRGBA plotElementColors[ eMAX_COLORS ];
extern GdkRGBA plotElementColorsFactory[ eMAX_COLORS ];

extern gchar *sNoiseLabel[ eMAX_NOISE_UNITS ];
extern gchar *sNoiseUnits[ eMAX_NOISE_UNITS ];

#define WLOOKUP(p,n) g_hash_table_lookup ( (p)->widgetHashTable, (gconstpointer)(n))
#define UPDATE_8970_SETTING( pGlobal, flag ) ({ \
        g_mutex_lock ( &pGlobal->HP8970settings.mUpdate ); \
        gboolean bUpdate = (pGlobal->HP8970settings.updateFlags.all == 0); \
        flag = TRUE; \
        g_mutex_unlock ( &pGlobal->HP8970settings.mUpdate ); \
        if( bUpdate ) \
                postDataToGPIBThread (TG_SEND_SETTINGS_to_HP8970, NULL); \
    })

void drawHPlogo 					(cairo_t *, gdouble, gdouble, gdouble, gboolean );
gint createNoiseFigureColumnView 	(GtkColumnView *, tGlobal * );
void logVersion						(void);
gint splashCreate 					(tGlobal *);
gint splashDestroy 					(tGlobal *);
gpointer threadGPIB					(gpointer);
gboolean plotNoiseFigureAndGain     (cairo_t *, gint, gint, tGlobal *, gboolean);

void centreJustifiedCairoText       (cairo_t *cr, gchar *sLabel, gdouble x, gdouble y, gdouble);
void rightJustifiedCairoText        (cairo_t *cr, gchar *sLabel, gdouble x, gdouble y, gboolean);
void leftJustifiedCairoText         (cairo_t *cr, gchar *sLabel, gdouble x, gdouble y, gboolean);
void updateBoundaries               ( gdouble, gdouble *, gdouble * );
void cairo_renderHewlettPackardLogo (cairo_t *, gboolean, gboolean, gdouble, gdouble );
gint getTimeStamp                   (gchar **);
void CB_edit_Title                  (GtkEditable*, gpointer);
void CB_notes_changed               (GtkTextBuffer*, gpointer);

gint savePlot                       (gchar *filePath, tGlobal *);

void initializePageGPIB             (tGlobal *);
void initializePageOptions          (tGlobal *);
void initializePageSource           (tGlobal *);
void initializePageExtLO            (tGlobal *);
void setPageExtLOwidgets            (tGlobal *);
void enablePageExtLOwidgets         (tGlobal *, tMode);
void warnFrequencyRangeOutOfBounds  (tGlobal *pGlobal);
void initializePageHP8970           (tGlobal *);
void refreshPageHP8970              (tGlobal *);
void initializePagePlot             (tGlobal *);
void initializePageNotes            (tGlobal *);
gint recoverSettings                (tGlobal *);
gint saveSettings                   (tGlobal *);
void snapshotSettings               (tGlobal *);
void setSpinNoiseRange              (tGlobal *);
void setSpinGainRange              (tGlobal *);
void setFixedRangePlotWidgets       (tGlobal *);
void initializeMainDialog           (tGlobal *);
void refreshMainDialog              (tGlobal *);
gboolean sweepHP8970                (tGlobal *, gint, gint, gint *);
gboolean calibrateHP8970            (tGlobal *, gint, gint, gint *);
gboolean spotFrequencyHP8970        (tGlobal *, gint, gint, gint *);
void validateCalibrationOperation   (tGlobal *);
void quarantineControlsOnSweep( tGlobal *pGlobal, gboolean bSpot, gboolean bShow );

void initCircularBuffer             (tCircularBuffer *, guint, tAbscissa);
tNoiseAndGain *getItemFromCircularBuffer(tCircularBuffer *, guint);
gint nItemsInCircularBuffer         (tCircularBuffer *);
gboolean addItemToCircularBuffer    (tCircularBuffer *, tNoiseAndGain *, gboolean );
gboolean determineTimeExtremesInCircularBuffer( tCircularBuffer * );
gint findTimeDeltaInCircularBuffer  (tCircularBuffer *, gdouble);
gchar *msTimeToString               (gint64, gboolean);
void drawModeDiagram                (cairo_t *, tMode, gint, gdouble, gdouble, gdouble);
void freeSVGhandles                 (void);

extern gdouble maxInputFreq[ e8970_MAXmodels ];
extern gchar *sHP89709models[];

#define LOCAL_DELAYms   50              // Delay after going to local from remote
#define SHORT_STRING    25
#define MEDIUM_STRING   50
#define LONG_STRING     256

extern tGlobal globalData;

#define MODE_PREVIEW_TIME   6

#define ODD(x)  ((x) % 2 == 1)
#define EVEN(x) ((x) % 2 == 0)
#define ms(x) ((x)*1000)
#define MHz(x) ((x)*1.0e6)
#define DBG( level, message, ... ) \
        if( globalData.flags.bbDebug >= level ) \
                LOG( G_LOG_LEVEL_DEBUG, message, ## __VA_ARGS__)

#define ERROR_INDICATOR_HP8970  9.00e10
#define IS_HP8970_ERROR(x) ((x) >= ERROR_INDICATOR_HP8970)
#define IS_HP8970_OVERFLOW(x) ((x) == ERROR_INDICATOR_HP8970 + 99.0E06)

#define MIN_RANGE_FREQ      4.0

#define MIN_RANGE_GAINdB    0.2
#define MIN_RANGE_NOISE_FdB 0.2
#define MIN_RANGE_NOISE_F   1.0
#define MIN_RANGE_NOISE_YdB 0.20
#define MIN_RANGE_NOISE_Y   1.0
#define MIN_RANGE_NOISE_T_K 10.0
#define NOISE_MIN_RANGE_CALIBRATION 2.0

#define MIN_GAIN -20.0
#define MAX_GAIN  60.0
#define MIN_GAIN_RANGE 0.1

#define DEFAULT_COLD_T  296.5

#define N_RANGES   4
#define LOG10   (1.0)
#define LOG2    (0.3010299956639811952137388947244930267681898814621085413104274611)
#define LOG5    (0.6989700043360188047862611052755069732318101185378914586895725388)
#define LOG1    0

#define TIME_PLOT_LENGTH    60.0
#define TIME_DIVISIONS_PER_GRID 10

#define CAL_POINTS_8970A    81
#define CAL_POINTS_8970B    181

#define MAX_SPOT_POINTS    2000
#define SMIG 0.001


// maximum and minimum frequencies (mode 1.0 and 1.4)
#define HP8970A_MAX_FREQ                1500.0
#define HP8970B_MAX_FREQ                1600.0
#define HP8970Bopt20_MAX_FREQ           2047.0
#define HP8970A_MIN_FREQ                  10.0
#define HP8970A_DEFAULT_FREQ              30.0
#define HP8970A_PageStep_FREQ             20.0

#define HP8970A_STOP_SWEEP_DEFAULT      1500.0
#define HP8970A_START_SWEEP_DEFAULT       10.0
#define HP8970A_STEP_SWEEP_DEFAULT        20.0

// maximum and minimum frequencies (mode 1.1, 1.2)
#define HP8970A_MAX_FREQ_R2            60000.0
#define HP8970A_MIN_FREQ_R2                1.0
#define HP8970A_DEFAULT_FREQ_R2        10000.0
#define HP8970A_PageStep_FREQ_R2         100.0

#define HP8970A_STOP_SWEEP_DEFAULT_R2  12000.0
#define HP8970A_START_SWEEP_DEFAULT_R2  8000.0
#define HP8970A_STEP_SWEEP_DEFAULT_R2    200.0

#define UNINITIALIZED_DOUBLE	 1.60217663e-19

#define DEFAULT_HP8970_GPIB_DEVICE_ID 8
#define DEFAULT_GPIB_CONTROLLER_INDEX 1

#define GSETTINGS_SCHEMA    "us.heterodyne.hp8970"

