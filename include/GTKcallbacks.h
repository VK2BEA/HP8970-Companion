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

#ifndef GTKCALLBACKS_H_
#define GTKCALLBACKS_H_

void CB_edit_filterFloat    (GtkEditable*, gchar*, gint, gint*, gpointer);
void CB_DrawingArea_Draw    (GtkDrawingArea *, cairo_t *, gint, gint, gpointer);

void CB_btn_Print           (GtkButton *, gpointer);
void CB_btn_PDF             (GtkButton *, gpointer);
void CB_btn_SVG             (GtkButton *, gpointer);
void CB_btn_PNG             (GtkButton *, gpointer);
void CB_btn_CSV             (GtkButton *, gpointer);

void CB_btn_SaveJSON        (GtkButton *, gpointer);
void CB_rightClickGesture_SaveJSON (GtkGesture *, int, double, double, gpointer *);
void CB_btn_RestoreJSON     (GtkButton *, gpointer);

void CB_spin_opt_GPIB_PID_LO(   GtkSpinButton*, gpointer );
void CB_chk_use_LO_GPIBdeviceName (GtkCheckButton *, gpointer);
void CB_edit_opt_LO_GPIB_name (GtkEditable *, gpointer);

#endif /* GTKCALLBACKS_H_ */
