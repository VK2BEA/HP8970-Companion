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

#include <cairo/cairo.h>
#include <glib-2.0/glib.h>
#include <gtk/gtk.h>
#include <HP8970.h>
#include <math.h>
#include <sys/utsname.h>

/*! \brief Log version to journal
 *
 */

void
logVersion (void) {

    struct utsname UTSbuffer;

    errno = 0;
    if (uname (&UTSbuffer) != 0) {
        LOG(G_LOG_LEVEL_CRITICAL, "%s", strerror (errno));
        return;
    }
    LOG(G_LOG_LEVEL_INFO, "%s %s %s %s %s", UTSbuffer.sysname, UTSbuffer.nodename, UTSbuffer.release, UTSbuffer.version,
        UTSbuffer.machine);
    LOG(G_LOG_LEVEL_INFO, "HP8970 version: %s", VERSION);

    return;
}

/*!     \brief  generate the ASCII representation of the current time
 *
 * generate the ASCII representation of the current time
 *
 * \ingroup initialize
 *
 * \param  psDateTime : pointer to pointer ASCII time
 */
gint
getTimeStamp( gchar **psDateTime ) {
    g_free( *psDateTime );
    GDateTime *dt = g_date_time_new_now_local();
    *psDateTime = g_date_time_format(dt, "%e %b %Y %H:%M:%S");
    g_date_time_unref(dt);

    return 0;
}


/*!     \brief  Display the splash screen
 *
 * Show the splash screen
 *
 * \ingroup initialize
 *
 * \param  pGlobal : Pointer to global data
 */
gint
splashCreate (tGlobal *pGlobal) {
    GtkWidget *wSplash = pGlobal->widgets[ eW_Splash ];
    GtkWidget *wApplicationWidget = pGlobal->widgets[ eW_HP8970_application ];
    GtkWidget *wVersionLabel = pGlobal->widgets[ eW_lbl_version ];
    gchar *sVersion;
    if (wSplash) {
        sVersion = g_strdup_printf ("Version %s\t(🔨 %s)", VERSION, __DATE__); // Changelog date is used in RPM
        gtk_label_set_label (GTK_LABEL(wVersionLabel), sVersion);
        g_free (sVersion);
        // this is needed for Wayland to get rid of the warning notice about transient window not attached
        // to parent
        gtk_window_set_transient_for (GTK_WINDOW(wSplash), GTK_WINDOW(wApplicationWidget));
        // gtk_window_set_position(GTK_WINDOW(wSplash), GTK_WIN_POS_CENTER_ALWAYS);
        gtk_window_present (GTK_WINDOW(wSplash)); // make sure we are on top
    }
    return FALSE;
}

/*!     \brief  Destroy the splash screen
 *
 * After a few seconds destroy the splash screen
 *
 * \ingroup initialize
 *
 * \param  Splash : Pointer to Splash widget to destroy
 */
gint
splashDestroy (tGlobal *pGlobal) {
    GtkWidget *wSplash = pGlobal->widgets[ eW_Splash ];
    if (GTK_IS_WIDGET(wSplash)) {
        gtk_window_destroy (GTK_WINDOW(wSplash));
    }
    return FALSE;
}


