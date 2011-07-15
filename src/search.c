/*
    roxterm - GTK+ 2.0 terminal emulator with tabs
    Copyright (C) 2004 Tony Houghton <h@realh.co.uk>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "dlg.h"
#include "search.h"

static GtkWidget *search_dialog = NULL;

struct {
    GtkEntry *entry;
    GtkToggleButton *match_case, *entire_word, *as_regex,
            *backwards, *wrap;
    ROXTermData *roxterm;
    VteTerminal *vte;
    MultiWin *win;
} search_data;

static void search_destroy_cb(GtkWidget *widget, void *handle)
{
    search_data.vte = NULL;
    search_dialog = NULL;
}

static void search_vte_destroyed_cb(VteTerminal *widget, void *handle)
{
    if (search_data.vte == widget)
    {
        search_data.vte = NULL;
        if (search_dialog)
            gtk_widget_hide(search_dialog);
    }
}

static void search_response_cb(GtkWidget *widget,
        guint response, void *handle)
{
    if (response == GTK_RESPONSE_ACCEPT)
    {
        GError *error = NULL;
        const char *pattern = gtk_entry_get_text(search_data.entry);
        gboolean backwards =
                gtk_toggle_button_get_active(search_data.backwards);
        guint flags =
                (gtk_toggle_button_get_active(search_data.match_case) ?
                        0 : ROXTERM_SEARCH_MATCH_CASE) |
                (gtk_toggle_button_get_active(search_data.as_regex) ?
                        ROXTERM_SEARCH_AS_REGEX : 0) |
                (gtk_toggle_button_get_active(search_data.entire_word) ?
                        ROXTERM_SEARCH_ENTIRE_WORD : 0) |
                (backwards ? 
                        ROXTERM_SEARCH_BACKWARDS : 0) |
                (gtk_toggle_button_get_active(search_data.wrap) ?
                        ROXTERM_SEARCH_WRAP : 0);
        
        if (roxterm_set_search(search_data.roxterm, pattern, flags, &error))
        {
            if (pattern && pattern[0])
            {
                if (backwards)
                    vte_terminal_search_find_previous(search_data.vte);
                else
                    vte_terminal_search_find_next(search_data.vte);
            }
        }
        else
        {
            dlg_warning(GTK_WINDOW(search_dialog),
                    _("Invalid search expression: %s"), error->message);
            g_error_free(error);
            /* Keep dialog open if there was an error */
            return;
        }
    }
    gtk_widget_hide(search_dialog);
}

void search_open_dialog(ROXTermData *roxterm)
{
    const char *pattern = roxterm_get_search_pattern(roxterm);
    guint flags = roxterm_get_search_flags(roxterm);
    
    search_data.roxterm = roxterm;
    search_data.win = roxterm_get_multi_win(roxterm);
    search_data.vte = roxterm_get_vte(roxterm);
    g_signal_connect(search_data.vte, "destroy",
            G_CALLBACK(search_vte_destroyed_cb), NULL);
    
    if (!search_dialog)
    {
        GtkBox *vbox;
        GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
        GtkWidget *w = gtk_label_new_with_mnemonic(_("_Search for:"));
        GtkWidget *entry = gtk_entry_new();
        
        search_dialog = gtk_dialog_new_with_buttons(_("Find"),
                GTK_WINDOW(multi_win_get_widget(search_data.win)),
                GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                GTK_STOCK_FIND, GTK_RESPONSE_ACCEPT,
                NULL);
        gtk_dialog_set_default_response(GTK_DIALOG(search_dialog),
                GTK_RESPONSE_ACCEPT);
                 
        vbox = GTK_BOX(gtk_dialog_get_content_area(
                GTK_DIALOG(search_dialog)));

        search_data.entry = GTK_ENTRY(entry);
        gtk_entry_set_width_chars(search_data.entry, 40);
        gtk_entry_set_activates_default(search_data.entry, TRUE);
        gtk_widget_set_tooltip_text(entry, _("A search string or "
                "perl-compatible regular expression."));
        gtk_label_set_mnemonic_widget(GTK_LABEL(w), entry);
        gtk_box_pack_start(GTK_BOX(hbox), w, FALSE, FALSE, DLG_SPACING);
        gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, DLG_SPACING);
        gtk_box_pack_start(vbox, hbox, FALSE, FALSE, DLG_SPACING);
        
        w = gtk_check_button_new_with_mnemonic(_("Match _Case"));
        gtk_widget_set_tooltip_text(w,
                _("Whether the search is case sensitive"));
        search_data.match_case = GTK_TOGGLE_BUTTON(w);
        gtk_box_pack_start(vbox, w, FALSE, FALSE, DLG_SPACING);
        
        w = gtk_check_button_new_with_mnemonic(_("Match _Entire Word"));
        gtk_widget_set_tooltip_text(w, _("If set the pattern will only match "
                "when it forms a word on its own."));
        search_data.entire_word = GTK_TOGGLE_BUTTON(w);
        gtk_box_pack_start(vbox, w, FALSE, FALSE, DLG_SPACING);
        
        w = gtk_check_button_new_with_mnemonic(
                _("Match As _Regular Expression"));
        gtk_widget_set_tooltip_text(w, _("If set the pattern is a "
                "perl-compatible regular expression."));
        search_data.as_regex = GTK_TOGGLE_BUTTON(w);
        gtk_box_pack_start(vbox, w, FALSE, FALSE, DLG_SPACING);
        
        w = gtk_check_button_new_with_mnemonic(_("Search _Backwards"));
        gtk_widget_set_tooltip_text(w, _("Whether to search backwards when "
                "the Find button is clicked. This does not affect the "
                "Find Next and Find Previous menu items."));
        search_data.backwards = GTK_TOGGLE_BUTTON(w);
        gtk_box_pack_start(vbox, w, FALSE, FALSE, DLG_SPACING);
        
        w = gtk_check_button_new_with_mnemonic(_("_Wrap Around"));
        gtk_widget_set_tooltip_text(w, _("Whether to wrap the search to the "
                "opposite end of the buffer when the beginning or end is "
                "reached."));
        search_data.wrap = GTK_TOGGLE_BUTTON(w);
        gtk_box_pack_start(vbox, w, FALSE, FALSE, DLG_SPACING);
        
        g_signal_connect(search_dialog, "response",
                G_CALLBACK(search_response_cb), NULL);
        g_signal_connect(search_dialog, "destroy",
                G_CALLBACK(search_destroy_cb), NULL);
        
    }
    
    if (pattern)
    {
        gtk_entry_set_text(search_data.entry, pattern);
        gtk_editable_select_region(GTK_EDITABLE(search_data.entry),
                0, g_utf8_strlen(pattern, -1));
    }
    else
    {
        gtk_entry_set_text(search_data.entry, "");
        flags = ROXTERM_SEARCH_BACKWARDS | ROXTERM_SEARCH_WRAP;
    }
    gtk_toggle_button_set_active(search_data.match_case,
            flags & ROXTERM_SEARCH_MATCH_CASE);
    gtk_toggle_button_set_active(search_data.entire_word,
            flags & ROXTERM_SEARCH_ENTIRE_WORD);
    gtk_toggle_button_set_active(search_data.as_regex,
            flags & ROXTERM_SEARCH_AS_REGEX);
    gtk_toggle_button_set_active(search_data.backwards,
            flags & ROXTERM_SEARCH_BACKWARDS);
    gtk_toggle_button_set_active(search_data.wrap,
            flags & ROXTERM_SEARCH_WRAP);
    
    if (gtk_widget_get_visible(search_dialog))
    {
        gtk_window_present(GTK_WINDOW(search_dialog));
    }
    else
    {
        gtk_widget_show_all(search_dialog);
    }
    gtk_widget_grab_focus(GTK_WIDGET(search_data.entry));
}

/* vi:set sw=4 ts=4 et cindent cino= */
