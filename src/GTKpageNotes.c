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

/*! \file GTKpageOptions.c
 *  \brief Handle GUI elements on the GtkNotebook page for Options
 *
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <cairo/cairo.h>
#include <glib-2.0/glib.h>
#include <HP8970.h>
#include "messageEvent.h"
#include <math.h>

/*!     \brief  Callback when the user changes the note
 *
 * Callback when the user changes the note
 *
 * \param  wNoteBuffer  pointer to the GtkTextBuffer from the GtkTextView widget
 * \param  pGlobal      pointer to global data
 */
void
CB_notes_changed ( GtkTextBuffer* wNoteBuffer, gpointer gpGlobal ) {
    tGlobal *pGlobal = (tGlobal *)gpGlobal;
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(wNoteBuffer, &start, &end);

    g_free( pGlobal->plot.sNotes );
    pGlobal->plot.sNotes = gtk_text_buffer_get_text( wNoteBuffer, &start, &end, TRUE );
}

/*!     \brief  Initialize the widgets on the Notes Notebook page
 *
 * Initialize the widgets on the Notes Notebook page
 *
 * \param  pGlobal      pointer to global data
 */
void
initializePageNotes( tGlobal *pGlobal ) {
    GtkTextView *wNotes = pGlobal->widgets[ eW_textView_Notes ];
    GtkTextBuffer *wNoteBuffer = gtk_text_view_get_buffer( wNotes );
    gtk_text_buffer_set_text( wNoteBuffer, pGlobal->plot.sNotes ? pGlobal->plot.sNotes : "", -1 );
    gtk_text_view_set_input_hints( wNotes, 0 );
    g_signal_connect( wNoteBuffer, "changed", G_CALLBACK( CB_notes_changed ), pGlobal);
}
