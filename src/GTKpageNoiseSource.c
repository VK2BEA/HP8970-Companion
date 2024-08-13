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

#include <glib-2.0/glib.h>
#include <gpib/ib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <locale.h>
#include <time.h>

#include <ctype.h>
#include <errno.h>
#include <HP8970.h>
#include "messageEvent.h"

#define NOISE_SOURCE_TYPE_TUPLE (noise_source_tuple_get_type ())
G_DECLARE_FINAL_TYPE (NoiseSourceTuple, noise_source_tuple, NOISE, SOURCE_TUPLE, GObject)

enum
{
	PROP_FREQUENCY = 1,
	PROP_ENR,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];


struct _NoiseSourceTuple
{
	GObject parent_instance;

	char *frequency;
	char *enr;
};

G_DEFINE_TYPE (NoiseSourceTuple, noise_source_tuple, G_TYPE_OBJECT)

static void
noise_source_tuple_set_property (GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec)

{
	NoiseSourceTuple *self = NOISE_SOURCE_TUPLE(object);

	switch (property_id)
	{
		case PROP_FREQUENCY:
			self->frequency = g_value_dup_string (value);
			break;

		case PROP_ENR:
			self->enr = g_value_dup_string (value);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}


static void
noise_source_tuple_get_property (GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec)
{
	NoiseSourceTuple *self = NOISE_SOURCE_TUPLE(object);

	switch (property_id)
	{
		case PROP_FREQUENCY:
			g_value_set_string (value, self->frequency);
			break;

		case PROP_ENR:
			g_value_set_string (value, self->enr);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
noise_source_tuple_finalize (GObject *object)
{
	NoiseSourceTuple *self = NOISE_SOURCE_TUPLE(object);

	g_free (self->frequency);
	g_free (self->enr);
}

static void
noise_source_tuple_init (NoiseSourceTuple *self)
{
}

static void
noise_source_tuple_class_init (NoiseSourceTupleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GParamFlags flags = G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT;

	object_class->finalize = noise_source_tuple_finalize;
	object_class->set_property = noise_source_tuple_set_property;
	object_class->get_property = noise_source_tuple_get_property;

	properties[PROP_FREQUENCY] = g_param_spec_string ("frequency", NULL, NULL, NULL, flags);
	properties[PROP_ENR] = g_param_spec_string ("enr", NULL, NULL, NULL, flags);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}


static NoiseSourceTuple *
noise_source_tuple_new (const char *frequency, const char *enr)
{
	return g_object_new (NOISE_SOURCE_TYPE_TUPLE,
			"frequency", frequency, "enr", enr, NULL);
}

// each character entered into the cells of the noise source GtkColumnView
// is checked if it is a valid number.
void
CB_NS_entry_insertText ( GtkEditable* wEditable, gchar* text, gint length, gint* position, gpointer user_data ) {
    GtkEntry *wEntry = GTK_ENTRY( gtk_widget_get_parent( GTK_WIDGET( wEditable ) ) );
    gint columnPosn = GPOINTER_TO_INT( g_object_get_data ( G_OBJECT( wEntry ), "column" ) );

    gboolean bHasDP = strchr( gtk_editable_get_text( wEditable ), '.' ) != NULL;
    gboolean bIgnore = FALSE;

    for( int i=0; i < length; i++ ) {
        gboolean bIsDP = (text[i] == '.');
        if( columnPosn == 0 ) {  // Frequency
            bIgnore = bIgnore || !isdigit( text[i] );
        } else {
            bIgnore = bIgnore || (bHasDP && bIsDP) || !(bIsDP || isdigit( text[i]) );
        }
        bHasDP = bHasDP || bIsDP;
    }

    if( bIgnore )
        g_signal_stop_emission_by_name( wEditable, "insert-text");

    gtk_widget_set_sensitive( globalData.widgets[ eW_NS_btn_Save ], TRUE );

}

void
CB_NS_entry_enter ( GtkEventControllerFocus* self, gpointer wListItem ) {
	// wListItem is GtkColumnViewCell
	// G_OBJECT_TYPE_NAME( G_OBJECT( gtk_column_view_cell_get_item( wListItem  )) )
	// gtk_column_view_cell_get_item( wListItem  ) is NoiseSourceTuple

	GtkWidget *wEntry = gtk_column_view_cell_get_child( wListItem );

	GtkWidget  *wColumnView = gtk_widget_get_ancestor( gtk_column_view_cell_get_child( wListItem ), GTK_TYPE_COLUMN_VIEW );

	// Select the item in the list model to match the row where the entry widget is
	GtkSelectionModel *model = gtk_column_view_get_model( GTK_COLUMN_VIEW( wColumnView ) );
	gtk_selection_model_select_item ( model, gtk_column_view_cell_get_position ( (GtkColumnViewCell *)( wListItem ) ), TRUE );

//	gtk_widget_add_css_class ( gtk_column_view_cell_get_child( wListItem ), "highlight");
#if 0
	GtkListView *wListView = GTK_LIST_VIEW( gtk_widget_get_ancestor ( gtk_column_view_cell_get_child( wListItem ), GTK_TYPE_LIST_VIEW ) );
	g_print( "child of list item: %s\n", G_OBJECT_TYPE_NAME( G_OBJECT( wListView ) ) );
	g_print( "bingo %d %s\n", gtk_column_view_cell_get_position ( (GtkColumnViewCell *)( wListItem ) ), G_OBJECT_TYPE_NAME( G_OBJECT( wEntry ) ) );
#endif
    g_object_set_data ( G_OBJECT( wEntry ), "changed" , GINT_TO_POINTER( FALSE ) );
}

gint
compareNoiseSourceTuple(void* a, void* b, void* userData) {
	GValue valueA = G_VALUE_INIT, valueB = G_VALUE_INIT;
	g_value_init ( &valueA, G_TYPE_STRING );
	g_value_init ( &valueB, G_TYPE_STRING );

	g_object_get_property( G_OBJECT( a ), "frequency", &valueA );
	g_object_get_property( G_OBJECT( b ), "frequency", &valueB );

	const gchar *strA = g_value_get_string( &valueA );
	const gchar *strB = g_value_get_string( &valueB );

	gdouble A = atof( strA ), B = atof( strB );


	if( A - B == 0.0 )
		return 0;

	// We put 0.0 on top (it's larger than any other number)
	if( A == 0.0 )
	    return 1;
	else if( B == 0.0 )
	    return -1;

	if( A - B < 0.0 )
		return -1;
	else
		return 1;
}

/*!     \brief  Test if the calibration list is sorted
 *
 * Test if the calibration list is sorted
 *
 * \param  listStore      pointer to GListStore
 * \return TRUE if sorted or FALSE if not
 */
gboolean
isListStoreSorted( GListStore *listStore ) {

    GListModel *listModel = G_LIST_MODEL( listStore );
    gdouble freq, lastFreq = 0.0;
    gint nCalPoints = g_list_model_get_n_items( listModel );
    for( int i = 0; i < nCalPoints; i++ ) {

        NoiseSourceTuple *pNoiseTuple = g_list_model_get_item ( listModel, i );
        gchar *sFreq;

        g_object_get( pNoiseTuple, "frequency", &sFreq, NULL );

        freq = g_ascii_strtod( sFreq, NULL );
        g_free( sFreq );

        if ( freq < lastFreq && freq != 0.0 )   // account for 0.0 being on top
            return FALSE;

        lastFreq = freq;
    }
    return TRUE;
}


void
CB_EntryChanged ( GtkEditable* wEditable, gpointer wListItem ) {
	GtkEntry *wEntry = GTK_ENTRY( gtk_widget_get_parent( GTK_WIDGET( wEditable ) ) );
	gint columnPosn = GPOINTER_TO_INT( g_object_get_data ( G_OBJECT( wEntry ), "column" ) );

	// guint row = gtk_column_view_cell_get_position( wListItem );
	NoiseSourceTuple *ENR = gtk_column_view_cell_get_item( (GtkColumnViewCell *)wListItem  );

	if( columnPosn == 0 ) {
	   g_free( ENR->frequency );
	   ENR->frequency = g_strdup( gtk_editable_get_text( wEditable ));
	} else {
		g_free( ENR->enr );
		gchar *sENR = g_strdup_printf( "%.2lf", g_strtod( gtk_editable_get_text( GTK_EDITABLE( wEditable )), NULL ) );
		ENR->enr = g_strdup( sENR );
		g_free ( sENR );
	}

	g_object_set_data ( G_OBJECT( wEntry ), "changed" , GINT_TO_POINTER( TRUE ) );
    // g_print( "%s %s\n", gtk_editable_get_text( wEditable ), G_OBJECT_TYPE_NAME( G_OBJECT( gtk_column_view_cell_get_item( (GtkColumnViewCell *)wListItem  )) ) );
}

/*!     \brief  Sort the list of freq/ENR entries if changed
 *
 * Sort the list of freq/ENR entries if changed
 *
 * \param wListItem list item
 */
static void
updatyeNScharacterizationList( gpointer wListItem ) {
    GtkWidget *wEntry = gtk_column_view_cell_get_child( wListItem );
    gint columnPosn = GPOINTER_TO_INT(  g_object_get_data ( G_OBJECT( wEntry ), "column" ) );
    GtkListView __attribute__((unused)) *wListView =
            GTK_LIST_VIEW( gtk_widget_get_ancestor ( gtk_column_view_cell_get_child( wListItem ), GTK_TYPE_LIST_VIEW ) );

    if( GPOINTER_TO_INT( g_object_get_data ( G_OBJECT( wEntry ), "changed" )) && columnPosn == 0 ) {
        GtkWidget  *wColumnView = gtk_widget_get_ancestor( gtk_column_view_cell_get_child( wListItem ), GTK_TYPE_COLUMN_VIEW );
        GListStore *listStore = g_object_get_data( G_OBJECT( wColumnView ), "list");

        if( !isListStoreSorted( listStore ) )
            g_list_store_sort( listStore, (GCompareDataFunc)compareNoiseSourceTuple, NULL );
    }
}

/*!     \brief  Callback when we leave the GtkEntry
 *
 * Test if the calibration list is sorted
 *
 * \param self      pointer to GtkEventControllerFocus of GtkEntry
 * \param wListItem list item
 */
void
CB_NS_entry_leave ( GtkEventControllerFocus* self, gpointer wListItem ) {
    updatyeNScharacterizationList ( wListItem );
}

/*!     \brief  Callback when an Enter key hit in GtkEntry
 *
 * Test if the calibration list is sorted
 *
 * \param self      pointer to GtkEntry of GtkEntry
 * \param wListItem list item
 */
static void
CB_EntryActivate ( GtkEntry* wEntry, gpointer wListItem ) {
    updatyeNScharacterizationList ( wListItem );
}


static void
setup_cb (GtkSignalListItemFactory *self, GtkListItem *wListItem, gpointer column)
{
	// wListItem is a pointer to a GtkColumnViewCell
	//
	GtkWidget *wEntry = gtk_entry_new ();
	gtk_entry_set_input_hints( GTK_ENTRY( wEntry ), GTK_INPUT_HINT_NO_EMOJI );
	gtk_entry_set_alignment( GTK_ENTRY( wEntry ), 1.0 );	//right align
	if( GPOINTER_TO_INT(column) == 0 )
	    gtk_entry_set_input_purpose( GTK_ENTRY( wEntry ), GTK_INPUT_PURPOSE_DIGITS );
	else
        gtk_entry_set_input_purpose( GTK_ENTRY( wEntry ), GTK_INPUT_PURPOSE_NUMBER );

	gtk_entry_set_activates_default ( GTK_ENTRY( wEntry ), TRUE );

	g_object_set_data( G_OBJECT( wEntry ), "column", column );
	g_signal_connect(gtk_editable_get_delegate(GTK_EDITABLE(wEntry)), "insert-text", G_CALLBACK( CB_NS_entry_insertText ), NULL);
	g_signal_connect(gtk_editable_get_delegate(GTK_EDITABLE(wEntry)), "changed", G_CALLBACK( CB_EntryChanged ), wListItem);
	g_signal_connect( wEntry, "activate", G_CALLBACK( CB_EntryActivate ), wListItem);

	GtkEventController *controller = gtk_event_controller_focus_new ( );
	g_signal_connect( controller, "enter", G_CALLBACK( CB_NS_entry_enter ),  wListItem );
	g_signal_connect( controller, "leave", G_CALLBACK( CB_NS_entry_leave ),  wListItem );

	gtk_widget_add_controller ( wEntry, controller );

	gtk_widget_add_css_class( wEntry, "table" );

	gtk_widget_set_hexpand( wEntry, TRUE );

	gtk_list_item_set_child (wListItem, wEntry);
}

/* Here is a new version of bind_cb using `GtkExpression`.
 *
 * Note that we never actually explicitly grab the `item` as a GObject and
 * extract its string and manually set the label as we did before.
 *
 * Instead, we basically create the following binding in pseudocode:
 *
 *   NOISE_SOURCE_TUPLE(GtkListItem.item)->{"frequency"} ==> GTK_LABEL(label)->{"label"}
 *
 * 		... in other words, we bind the "frequency" (or "enr", depending on the
 * 		`prop` argument) property of our NoiseSourceTuple -- which we know to be
 * 		the `item` of the GtkListItem -- to the "text" property of our
 * 		GtkEntry, which we know to be the `child` of the same GtkListItem.
 *
 * This may seem pointless in code, but the usefulness of this technique will
 * (hopefully) become clearer when we look at how GtkExpressions can be used in
 * GtkBuilder XML files.
 */
static void
bind_cb (GtkSignalListItemFactory *self, GtkListItem *list_item, const char *prop)
{
	GtkWidget *wEntry = gtk_list_item_get_child (list_item);
	GtkExpression *expr1, *expr2;

	expr1 = gtk_property_expression_new (GTK_TYPE_LIST_ITEM, NULL, "item");
	expr2 = gtk_property_expression_new (NOISE_SOURCE_TYPE_TUPLE, expr1, prop);
	gtk_expression_bind (expr2, wEntry, "text", /*this*/ list_item);
}

static GtkListItemFactory *
create_factory (const char *prop, gint column)
{
	GtkListItemFactory *factory = gtk_signal_list_item_factory_new ();

	g_signal_connect (factory, "setup", G_CALLBACK(setup_cb), GINT_TO_POINTER( column ));
	g_signal_connect (factory, "bind",  G_CALLBACK(bind_cb ), (gpointer)prop);

	return factory;
}

static void
initializeNoiseSourceCalPoints (GListStore *store, tNoiseSource *noiseSource)
{
    g_list_store_remove_all( store );
	for ( gint i=0; i < MAX_NOISE_SOURCE_CALDATA_LENGTH; i++ ) {
	    if( noiseSource->calibrationPoints[ i ][ 0 ] != 0.0 ) {
            gchar *sFreq = g_strdup_printf( "%.0lf", noiseSource->calibrationPoints[ i ][ 0 ] );
            gchar *sENR  = g_strdup_printf( "%.2lf", noiseSource->calibrationPoints[ i ][ 1 ] );

            g_autoptr(NoiseSourceTuple) calPoint = noise_source_tuple_new (sFreq, sENR);
            g_list_store_append (store, calPoint);

            g_free( sFreq );
            g_free( sENR  );
	    }
	}
}



gint
createNoiseFigureColumnView (GtkColumnView *wNoiseFigure, tGlobal *pGlobal )
{
	GListStore *noiseSourceListStore = g_list_store_new (NOISE_SOURCE_TYPE_TUPLE);
	GtkSelectionModel *selectionModel;
	selectionModel = GTK_SELECTION_MODEL(gtk_single_selection_new (G_LIST_MODEL(noiseSourceListStore)));

	gtk_column_view_set_model ( wNoiseFigure, selectionModel );
	gtk_widget_add_css_class( GTK_WIDGET( wNoiseFigure ), "table" );

	GtkListItemFactory *factoryFreq = create_factory ("frequency", 0);
	GtkListItemFactory *factoryENR  = create_factory ("enr", 1);
	GtkColumnViewColumn *colFreq = gtk_column_view_column_new ("Freq. (MHz)", factoryFreq);
	gtk_column_view_column_set_id ( colFreq, "freq" );
	GtkColumnViewColumn *colNoise = gtk_column_view_column_new ("ENR   (dB)", factoryENR);
	gtk_column_view_column_set_id ( colNoise, "enr" );

	initializeNoiseSourceCalPoints (noiseSourceListStore, &pGlobal->HP8970settings.noiseSources[ 0 ]);
	g_object_set_data( G_OBJECT( wNoiseFigure ), "list", noiseSourceListStore );

	gtk_column_view_append_column (GTK_COLUMN_VIEW(wNoiseFigure), colFreq);
	gtk_column_view_append_column (GTK_COLUMN_VIEW(wNoiseFigure), colNoise);

	return TRUE;
}

/*!     \brief  Upload Noise Source table to HP8970
 *
 * Upload Noise Source table to HP8970
 *
 * \param  wNSuploadBtn   pointer to "Upload" button widget
 * \param  uData          user data (unused)
 */
static void
CB_NS_btn_Upload ( GtkButton* wNSsaveBtn, gpointer user_data ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data( G_OBJECT( wNSsaveBtn ), "data");

    // We upload what is shown in the table...
    // This might be different to what is stored in the array of noise sources
    GtkColumnView *wCVnoiseSource = GTK_COLUMN_VIEW( pGlobal->widgets[ eW_CV_NoiseSource ] );
    GListModel *listModel = G_LIST_MODEL( gtk_column_view_get_model( wCVnoiseSource) );

    bzero( &pGlobal->HP8970settings.noiseSourceCache, sizeof(tNoiseSource) );
    gint nCalPoints = g_list_model_get_n_items( listModel );
    for( int i = 0; i < nCalPoints && i < MAX_NOISE_SOURCE_CALDATA_LENGTH; i++ ) {

        NoiseSourceTuple *pNoiseTuple = g_list_model_get_item ( listModel, i );
        gchar *sFreq, *sENR;

        g_object_get( pNoiseTuple, "frequency", &sFreq, "enr", &sENR, NULL );

        pGlobal->HP8970settings.noiseSourceCache.calibrationPoints[i][0] = g_ascii_strtod( sFreq, NULL );
        pGlobal->HP8970settings.noiseSourceCache.calibrationPoints[i][1] = g_ascii_strtod( sENR, NULL );

        g_free( sFreq );
        g_free( sENR );
    }

    postDataToGPIBThread (TG_SEND_ENR_TABLE_TO_HP8970, NULL);
}

/*!     \brief  Save Noise Source table
 *
 * Save Noise Source table
 *
 * \param  wNSsaveBtn      pointer to "Add" button widget
 * \param  uData          user data (unused)
 */
static void
CB_NS_btn_Save ( GtkButton* wNSsaveBtn, gpointer user_data ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data( G_OBJECT( wNSsaveBtn ), "data");
    GtkComboBoxText *wNSselect = pGlobal->widgets[ eW_NS_combo_Source ];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    // const gchar *activeID = gtk_combo_box_get_active_id( GTK_COMBO_BOX( wNSselect ));
    gint activeNoiseSource = pGlobal->HP8970settings.activeNoiseSource;

    gchar    *sNewName = gtk_combo_box_text_get_active_text( wNSselect );
    g_strlcpy( pGlobal->HP8970settings.noiseSources[ activeNoiseSource ].name, sNewName, MAX_NOISE_SOURCE_NAME_LENGTH+1 );
    gtk_combo_box_text_remove( wNSselect, activeNoiseSource );
    gtk_combo_box_text_insert_text( wNSselect, activeNoiseSource, pGlobal->HP8970settings.noiseSources[ activeNoiseSource ].name );
#pragma GCC diagnostic pop

    GtkColumnView *wCVnoiseSource = GTK_COLUMN_VIEW( pGlobal->widgets[ eW_CV_NoiseSource ] );
//  GListStore *listStore = g_object_get_data( G_OBJECT( wCVnoiseSource ), "list");
    GListModel *listModel = G_LIST_MODEL( gtk_column_view_get_model( wCVnoiseSource) );

    gint nCalPoints = g_list_model_get_n_items( listModel );
    for( int i = 0; i < nCalPoints && i < MAX_NOISE_SOURCE_CALDATA_LENGTH; i++ ) {

        NoiseSourceTuple *pNoiseTuple = g_list_model_get_item ( listModel, i );
        gchar *sFreq, *sENR;

        g_object_get( pNoiseTuple, "frequency", &sFreq, "enr", &sENR, NULL );

        pGlobal->HP8970settings.noiseSources[ pGlobal->HP8970settings.activeNoiseSource ].calibrationPoints[ i ][ 0 ] = g_ascii_strtod( sFreq, NULL );
        pGlobal->HP8970settings.noiseSources[ pGlobal->HP8970settings.activeNoiseSource ].calibrationPoints[ i ][ 1 ] = g_ascii_strtod( sENR, NULL );

        g_free( sFreq );
        g_free( sENR );
    }

    g_free( sNewName );
    // Disable save button
    gtk_widget_set_sensitive( pGlobal->widgets[ eW_NS_btn_Save ], FALSE );
}

/*!     \brief  Delete frequency / ENR line to Noise Source table
 *
 * Delete frequency / ENR line to Noise Source table
 *
 * \param  wNSdeleteBtn      pointer to "Delete" button widget
 * \param  uData          user data (unused)
 */
static void
CB_NS_btn_Delete ( GtkButton* wNSdeleteBtn, gpointer user_data ) {
	tGlobal *pGlobal = (tGlobal *)g_object_get_data( G_OBJECT( wNSdeleteBtn ), "data");

	GtkColumnView *wCVnoiseSource = GTK_COLUMN_VIEW ( pGlobal->widgets[ eW_CV_NoiseSource ] );
	GListStore *listStore = g_object_get_data( G_OBJECT( wCVnoiseSource ), "list");

	// Find the selected item in the list model
	GtkSelectionModel *wSelectionModel = gtk_column_view_get_model( GTK_COLUMN_VIEW( wCVnoiseSource ) );
	GtkBitset *bitSetSelected = gtk_selection_model_get_selection( wSelectionModel );
	GtkBitsetIter bitSetIter;
	guint row;

	if( gtk_bitset_iter_init_first( &bitSetIter, bitSetSelected, &row ) ) {
		g_list_store_remove( listStore, row );
	}

    // Enable save button
    gtk_widget_set_sensitive( pGlobal->widgets[ eW_NS_btn_Save ], TRUE );
}

/*!     \brief  Update the scroll window holding the ENR table
 *
 * Update the scroll window holding the ENR table.
 * We have to wait for the columnview widget to update after adding
 * an item to the liststore.
 *
 * \param  uData    pointer to the scroll window adjustment
 */
void
CB_scrollNSlisonIdle (gpointer uData)
{
    GtkAdjustment *wScrollAdj = (GtkAdjustment *)uData;
    gtk_adjustment_set_value( wScrollAdj, gtk_adjustment_get_upper( wScrollAdj ) );
}

/*!     \brief  Add frequency / ENR line to Noise Source table
 *
 * Add frequency / ENR line to Noise Source table
 *
 * \param  wNSaddBtn      pointer to "Add" button widget
 * \param  uData          user data (unused)
 */
static void
CB_NS_btn_Add( GtkButton* wNSaddBtn, gpointer uData ) {
	tGlobal *pGlobal = (tGlobal *)g_object_get_data( G_OBJECT( wNSaddBtn ), "data");

	GtkColumnView *wCVnoiseSource = GTK_COLUMN_VIEW ( pGlobal->widgets[ eW_CV_NoiseSource ] );
	GListStore *listStore = g_object_get_data( G_OBJECT( wCVnoiseSource ), "list");

	g_autoptr(NoiseSourceTuple) calPoint = noise_source_tuple_new ( "0.00", "15.00");
	g_list_store_append ( listStore, calPoint );

//	g_list_store_sort( listStore, (GCompareDataFunc)compareNoiseSourceTuple, NULL );

#if 1
	// select new item
	gtk_selection_model_select_item( gtk_column_view_get_model( wCVnoiseSource ),
	                                 g_list_model_get_n_items( G_LIST_MODEL( listStore ) ) - 1, TRUE);
    // Schedule scroll when the list updates the column view widget
    GtkAdjustment *wScrollAdj = gtk_scrolled_window_get_vadjustment( pGlobal->widgets[ eW_scroll_NoiseSource ]);
    g_idle_add_once (CB_scrollNSlisonIdle, wScrollAdj);
#else
    // This doesn't work because the added item in the listStore has not propagated
    // through to the widget.
    gtk_column_view_scroll_to( wCVnoiseSource, g_list_model_get_n_items( G_LIST_MODEL( listStore ) )-1, NULL, GTK_LIST_SCROLL_SELECT, NULL);
#endif

	// Enable save button
	gtk_widget_set_sensitive( pGlobal->widgets[ eW_NS_btn_Save ], TRUE );
}

void
CB_NS_combo_Source_changed ( GtkComboBox* wNSselect, gpointer gpGlobal ) {
    tGlobal *pGlobal = (tGlobal *)gpGlobal;
    gint whichNoiseSource;
    GListStore *listStore = g_object_get_data( G_OBJECT( pGlobal->widgets[ eW_CV_NoiseSource ] ), "list");

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    GtkEntry       *wNSentry = GTK_ENTRY( gtk_combo_box_get_child( wNSselect ) );
    GtkEntryBuffer __attribute__((unused)) *wNSentryBuffer = gtk_entry_get_buffer( wNSentry );
    // const gchar    *sNewName = gtk_entry_buffer_get_text( wNSentryBuffer );

    // This will be 0-4 or -1
    // -1 when the user modifies the edit box
    whichNoiseSource = gtk_combo_box_get_active ( wNSselect );

    if( whichNoiseSource >= 0 && whichNoiseSource < MAX_NOISE_SOURCES ) {
        initializeNoiseSourceCalPoints (listStore, &pGlobal->HP8970settings.noiseSources[ whichNoiseSource ]);
        gtk_widget_grab_focus( GTK_WIDGET( wNSentry ));
        gtk_editable_select_region( GTK_EDITABLE(wNSentry), -1, -1);
        gtk_widget_grab_focus( pGlobal->widgets[ eW_NS_btn_Upload ] );
        pGlobal->HP8970settings.activeNoiseSource = whichNoiseSource;
    }

    gtk_widget_set_sensitive( pGlobal->widgets[ eW_NS_btn_Save ], whichNoiseSource == INVALID );

#pragma GCC diagnostic pop
}

/*!     \brief  Initialize the widgets on the Settings Notebook page
 *
 * Initialize the widgets on the Settings Notebook page
 *
 * \param  pGlobal      pointer to global data
 */
void
initializePageSource( tGlobal *pGlobal ) {
    GtkSizeGroup *wSrcSize =  gtk_size_group_new ( GTK_SIZE_GROUP_HORIZONTAL );
    GtkComboBoxText *wSelectNoiseSource = pGlobal->widgets[ eW_NS_combo_Source ];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    gtk_size_group_add_widget ( wSrcSize, pGlobal->widgets[ eW_NS_btn_Delete ] );
    gtk_size_group_add_widget ( wSrcSize, pGlobal->widgets[ eW_NS_btn_Add ] );
    gtk_size_group_add_widget ( wSrcSize, pGlobal->widgets[ eW_NS_btn_Save ] );
    gtk_size_group_add_widget ( wSrcSize, pGlobal->widgets[ eW_NS_btn_Upload ] );

    g_signal_connect(pGlobal->widgets[ eW_NS_btn_Delete ], "clicked", G_CALLBACK( CB_NS_btn_Delete ), NULL);
    g_signal_connect(pGlobal->widgets[ eW_NS_btn_Add ], "clicked", G_CALLBACK( CB_NS_btn_Add ), NULL);
    g_signal_connect(pGlobal->widgets[ eW_NS_btn_Save ], "clicked", G_CALLBACK( CB_NS_btn_Save ), NULL);
    g_signal_connect(pGlobal->widgets[ eW_NS_btn_Upload ], "clicked", G_CALLBACK( CB_NS_btn_Upload ), NULL);

    // This is called if either the combobox is changed or the entry is changed
    g_signal_connect(wSelectNoiseSource, "changed", G_CALLBACK( CB_NS_combo_Source_changed ), pGlobal);

    for( int i = 0; i < MAX_NOISE_SOURCES; i++ ) {
        gtk_combo_box_text_append_text( wSelectNoiseSource, pGlobal->HP8970settings.noiseSources[ i ].name );
    }

    gtk_entry_set_input_hints( GTK_ENTRY( gtk_combo_box_get_child(GTK_COMBO_BOX(wSelectNoiseSource))), GTK_INPUT_HINT_NO_EMOJI );
    gtk_combo_box_set_active( GTK_COMBO_BOX( wSelectNoiseSource ), pGlobal->HP8970settings.activeNoiseSource );
#pragma GCC diagnostic pop
}
