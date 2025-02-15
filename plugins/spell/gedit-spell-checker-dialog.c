/*
 * gedit-spell-checker-dialog.c
 * This file is part of gedit
 *
 * Copyright (C) 2002 Paolo Maggi
 * Copyright (C) 2015 Sébastien Wilmet
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "gedit-spell-checker-dialog.h"
#include <glib/gi18n.h>
#include "gedit-spell-checker.h"

/**
 * SECTION:spell-checker-dialog
 * @Short_description: Spell checker dialog
 * @Title: GeditSpellCheckerDialog
 * @See_also: #GeditSpellNavigator
 *
 * #GeditSpellCheckerDialog is a #GtkDialog to spell check a document one word
 * at a time. It uses a #GeditSpellNavigator.
 */

typedef struct _GeditSpellCheckerDialogPrivate GeditSpellCheckerDialogPrivate;

struct _GeditSpellCheckerDialog
{
	GtkDialog parent_instance;
};

struct _GeditSpellCheckerDialogPrivate
{
	GeditSpellNavigator *navigator;
	GeditSpellChecker *checker;

	gchar *misspelled_word;

	GtkLabel *misspelled_word_label;
	GtkEntry *word_entry;
	GtkWidget *check_word_button;
	GtkWidget *ignore_button;
	GtkWidget *ignore_all_button;
	GtkWidget *change_button;
	GtkWidget *change_all_button;
	GtkWidget *add_word_button;
	GtkTreeView *suggestions_view;

	guint initialized : 1;
};

enum
{
	PROP_0,
	PROP_SPELL_NAVIGATOR,
};

enum
{
	COLUMN_SUGGESTION,
	N_COLUMNS
};

G_DEFINE_TYPE_WITH_PRIVATE (GeditSpellCheckerDialog, gedit_spell_checker_dialog, GTK_TYPE_DIALOG)

static void
set_spell_checker (GeditSpellCheckerDialog *dialog,
		   GeditSpellChecker       *checker)
{
	GeditSpellCheckerDialogPrivate *priv;

	priv = gedit_spell_checker_dialog_get_instance_private (dialog);

	if (g_set_object (&priv->checker, checker))
	{
		GtkHeaderBar *header_bar;
		const GeditSpellCheckerLanguage *lang;

		header_bar = GTK_HEADER_BAR (gtk_dialog_get_header_bar (GTK_DIALOG (dialog)));

		lang = gedit_spell_checker_get_language (checker);

		gtk_header_bar_set_subtitle (header_bar,
					     gedit_spell_checker_language_to_string (lang));
	}
}

static void
set_navigator (GeditSpellCheckerDialog *dialog,
	       GeditSpellNavigator     *navigator)
{
	GeditSpellCheckerDialogPrivate *priv;

	priv = gedit_spell_checker_dialog_get_instance_private (dialog);

	g_return_if_fail (priv->navigator == NULL);
	priv->navigator = g_object_ref (navigator);

	g_object_notify (G_OBJECT (dialog), "spell-navigator");
}

static void
clear_suggestions (GeditSpellCheckerDialog *dialog)
{
	GeditSpellCheckerDialogPrivate *priv;
	GtkListStore *store;

	priv = gedit_spell_checker_dialog_get_instance_private (dialog);

	store = GTK_LIST_STORE (gtk_tree_view_get_model (priv->suggestions_view));
	gtk_list_store_clear (store);

	gtk_tree_view_columns_autosize (priv->suggestions_view);
}

static void
set_suggestions (GeditSpellCheckerDialog *dialog,
		 GSList                  *suggestions)
{
	GeditSpellCheckerDialogPrivate *priv;
	GtkListStore *store;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	const gchar *first_suggestion;
	GSList *l;

	priv = gedit_spell_checker_dialog_get_instance_private (dialog);

	clear_suggestions (dialog);

	store = GTK_LIST_STORE (gtk_tree_view_get_model (priv->suggestions_view));

	if (suggestions == NULL)
	{
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    /* Translators: Displayed in the "Check Spelling"
				     * dialog if there are no suggestions for the current
				     * misspelled word.
				     */
				    COLUMN_SUGGESTION, _("(no suggested words)"),
				    -1);

		gtk_entry_set_text (priv->word_entry, "");
		gtk_widget_set_sensitive (GTK_WIDGET (priv->suggestions_view), FALSE);
		return;
	}

	gtk_widget_set_sensitive (GTK_WIDGET (priv->suggestions_view), TRUE);

	first_suggestion = suggestions->data;
	gtk_entry_set_text (priv->word_entry, first_suggestion);

	for (l = suggestions; l != NULL; l = l->next)
	{
		const gchar *suggestion = l->data;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COLUMN_SUGGESTION, suggestion,
				    -1);
	}

	selection = gtk_tree_view_get_selection (priv->suggestions_view);
	gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter);
	gtk_tree_selection_select_iter (selection, &iter);
}

static void
set_misspelled_word (GeditSpellCheckerDialog *dialog,
		     const gchar             *word)
{
	GeditSpellCheckerDialogPrivate *priv;
	gchar *label;
	GSList *suggestions;

	g_assert (word != NULL);

	priv = gedit_spell_checker_dialog_get_instance_private (dialog);

	g_return_if_fail (!gedit_spell_checker_check_word (priv->checker, word, NULL));

	g_free (priv->misspelled_word);
	priv->misspelled_word = g_strdup (word);

	label = g_strdup_printf("<b>%s</b>", word);
	gtk_label_set_markup (priv->misspelled_word_label, label);
	g_free (label);

	suggestions = gedit_spell_checker_get_suggestions (priv->checker, priv->misspelled_word);

	set_suggestions (dialog, suggestions);

	g_slist_free_full (suggestions, g_free);
}

static void
set_completed (GeditSpellCheckerDialog *dialog)
{
	GeditSpellCheckerDialogPrivate *priv;

	priv = gedit_spell_checker_dialog_get_instance_private (dialog);

	clear_suggestions (dialog);
	gtk_entry_set_text (priv->word_entry, "");

	gtk_widget_set_sensitive (GTK_WIDGET (priv->word_entry), FALSE);
	gtk_widget_set_sensitive (priv->check_word_button, FALSE);
	gtk_widget_set_sensitive (priv->ignore_button, FALSE);
	gtk_widget_set_sensitive (priv->ignore_all_button, FALSE);
	gtk_widget_set_sensitive (priv->change_button, FALSE);
	gtk_widget_set_sensitive (priv->change_all_button, FALSE);
	gtk_widget_set_sensitive (priv->add_word_button, FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->suggestions_view), FALSE);
}

static void
show_error (GeditSpellCheckerDialog *dialog,
	    GError                  *error)
{
	GeditSpellCheckerDialogPrivate *priv;
	gchar *label;

	priv = gedit_spell_checker_dialog_get_instance_private (dialog);

	label = g_strdup_printf ("<b>%s</b> %s", _("Error:"), error->message);
	gtk_label_set_markup (priv->misspelled_word_label, label);
	g_free (label);

	set_completed (dialog);
}

static void
goto_next (GeditSpellCheckerDialog *dialog)
{
	GeditSpellCheckerDialogPrivate *priv;
	gchar *word = NULL;
	GeditSpellChecker *checker = NULL;
	GError *error = NULL;
	gboolean found;

	priv = gedit_spell_checker_dialog_get_instance_private (dialog);

	found = gedit_spell_navigator_goto_next (priv->navigator, &word, &checker, &error);

	if (error != NULL)
	{
		show_error (dialog, error);
		g_error_free (error);
		error = NULL;
	}
	else if (found)
	{
		set_spell_checker (dialog, checker);
		set_misspelled_word (dialog, word);
	}
	else
	{
		gchar *label;

		if (priv->initialized)
		{
			label = g_strdup_printf ("<b>%s</b>", _("Completed spell checking"));
		}
		else
		{
			label = g_strdup_printf ("<b>%s</b>", _("No misspelled words"));
		}

		gtk_label_set_markup (priv->misspelled_word_label, label);
		g_free (label);

		set_completed (dialog);
	}

	priv->initialized = TRUE;

	g_free (word);
	g_clear_object (&checker);
}

static void
gedit_spell_checker_dialog_get_property (GObject    *object,
					 guint       prop_id,
					 GValue     *value,
					 GParamSpec *pspec)
{
	GeditSpellCheckerDialogPrivate *priv;

	priv = gedit_spell_checker_dialog_get_instance_private (GEDIT_SPELL_CHECKER_DIALOG (object));

	switch (prop_id)
	{
		case PROP_SPELL_NAVIGATOR:
			g_value_set_object (value, priv->navigator);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_spell_checker_dialog_set_property (GObject      *object,
					 guint         prop_id,
					 const GValue *value,
					 GParamSpec   *pspec)
{
	GeditSpellCheckerDialog *dialog = GEDIT_SPELL_CHECKER_DIALOG (object);

	switch (prop_id)
	{
		case PROP_SPELL_NAVIGATOR:
			set_navigator (dialog, g_value_get_object (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_spell_checker_dialog_dispose (GObject *object)
{
	GeditSpellCheckerDialogPrivate *priv;

	priv = gedit_spell_checker_dialog_get_instance_private (GEDIT_SPELL_CHECKER_DIALOG (object));

	g_clear_object (&priv->navigator);
	g_clear_object (&priv->checker);

	G_OBJECT_CLASS (gedit_spell_checker_dialog_parent_class)->dispose (object);
}

static void
gedit_spell_checker_dialog_finalize (GObject *object)
{
	GeditSpellCheckerDialogPrivate *priv;

	priv = gedit_spell_checker_dialog_get_instance_private (GEDIT_SPELL_CHECKER_DIALOG (object));

	g_free (priv->misspelled_word);

	G_OBJECT_CLASS (gedit_spell_checker_dialog_parent_class)->finalize (object);
}

static void
gedit_spell_checker_dialog_show (GtkWidget *widget)
{
	GeditSpellCheckerDialog *dialog = GEDIT_SPELL_CHECKER_DIALOG (widget);
	GeditSpellCheckerDialogPrivate *priv;

	priv = gedit_spell_checker_dialog_get_instance_private (dialog);

	/* Chain-up */
	if (GTK_WIDGET_CLASS (gedit_spell_checker_dialog_parent_class)->show != NULL)
	{
		GTK_WIDGET_CLASS (gedit_spell_checker_dialog_parent_class)->show (widget);
	}

	/* A typical implementation of a SpellNavigator is to select the
	 * misspelled word when goto_next() is called. Showing the dialog makes
	 * a focus change, which can unselect the buffer selection (e.g. in a
	 * GtkTextBuffer). So that's why goto_next() is called after the
	 * chain-up.
	 */
	if (!priv->initialized)
	{
		goto_next (dialog);
	}
}

static void
gedit_spell_checker_dialog_class_init (GeditSpellCheckerDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->get_property = gedit_spell_checker_dialog_get_property;
	object_class->set_property = gedit_spell_checker_dialog_set_property;
	object_class->dispose = gedit_spell_checker_dialog_dispose;
	object_class->finalize = gedit_spell_checker_dialog_finalize;

	widget_class->show = gedit_spell_checker_dialog_show;

	/**
	 * GeditSpellCheckerDialog:spell-navigator:
	 *
	 * The #GeditSpellNavigator to use.
	 */
	g_object_class_install_property (object_class,
					 PROP_SPELL_NAVIGATOR,
					 g_param_spec_object ("spell-navigator",
							      "Spell Navigator",
							      "",
							      GEDIT_TYPE_SPELL_NAVIGATOR,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY |
							      G_PARAM_STATIC_STRINGS));

	/* Bind class to template */
	gtk_widget_class_set_template_from_resource (widget_class,
						     "/org/gnome/gedit/plugins/spell/ui/spell-checker.ui");
	gtk_widget_class_bind_template_child_private (widget_class, GeditSpellCheckerDialog, misspelled_word_label);
	gtk_widget_class_bind_template_child_private (widget_class, GeditSpellCheckerDialog, word_entry);
	gtk_widget_class_bind_template_child_private (widget_class, GeditSpellCheckerDialog, check_word_button);
	gtk_widget_class_bind_template_child_private (widget_class, GeditSpellCheckerDialog, ignore_button);
	gtk_widget_class_bind_template_child_private (widget_class, GeditSpellCheckerDialog, ignore_all_button);
	gtk_widget_class_bind_template_child_private (widget_class, GeditSpellCheckerDialog, change_button);
	gtk_widget_class_bind_template_child_private (widget_class, GeditSpellCheckerDialog, change_all_button);
	gtk_widget_class_bind_template_child_private (widget_class, GeditSpellCheckerDialog, add_word_button);
	gtk_widget_class_bind_template_child_private (widget_class, GeditSpellCheckerDialog, suggestions_view);
}

static void
word_entry_changed_handler (GtkEntry                *word_entry,
			    GeditSpellCheckerDialog *dialog)
{
	GeditSpellCheckerDialogPrivate *priv;
	gboolean sensitive;

	priv = gedit_spell_checker_dialog_get_instance_private (dialog);

	sensitive = gtk_entry_get_text_length (word_entry) > 0;

	gtk_widget_set_sensitive (priv->check_word_button, sensitive);
	gtk_widget_set_sensitive (priv->change_button, sensitive);
	gtk_widget_set_sensitive (priv->change_all_button, sensitive);
}

static void
suggestions_selection_changed_handler (GtkTreeSelection        *selection,
				       GeditSpellCheckerDialog *dialog)
{
	GeditSpellCheckerDialogPrivate *priv;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *text;

	priv = gedit_spell_checker_dialog_get_instance_private (dialog);

	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
	{
		return;
	}

	gtk_tree_model_get (model, &iter,
			    COLUMN_SUGGESTION, &text,
			    -1);

	gtk_entry_set_text (priv->word_entry, text);

	g_free (text);
}

static void
check_word_button_clicked_handler (GtkButton               *button,
				   GeditSpellCheckerDialog *dialog)
{
	GeditSpellCheckerDialogPrivate *priv;
	const gchar *word;
	gboolean correctly_spelled;
	GError *error = NULL;

	priv = gedit_spell_checker_dialog_get_instance_private (dialog);

	g_return_if_fail (gtk_entry_get_text_length (priv->word_entry) > 0);

	word = gtk_entry_get_text (priv->word_entry);

	correctly_spelled = gedit_spell_checker_check_word (priv->checker, word, &error);

	if (error != NULL)
	{
		show_error (dialog, error);
		g_error_free (error);
		return;
	}

	if (correctly_spelled)
	{
		GtkListStore *store;
		GtkTreeIter iter;

		clear_suggestions (dialog);

		store = GTK_LIST_STORE (gtk_tree_view_get_model (priv->suggestions_view));

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
		                    /* Translators: Displayed in the "Check
				     * Spelling" dialog if the current word
				     * isn't misspelled.
				     */
				    COLUMN_SUGGESTION, _("(correct spelling)"),
				    -1);

		gtk_widget_set_sensitive (GTK_WIDGET (priv->suggestions_view), FALSE);
	}
	else
	{
		GSList *suggestions;

		suggestions = gedit_spell_checker_get_suggestions (priv->checker, word);

		set_suggestions (dialog, suggestions);

		g_slist_free_full (suggestions, g_free);
	}
}

static void
add_word_button_clicked_handler (GtkButton               *button,
				 GeditSpellCheckerDialog *dialog)
{
	GeditSpellCheckerDialogPrivate *priv;

	priv = gedit_spell_checker_dialog_get_instance_private (dialog);

	g_return_if_fail (priv->misspelled_word != NULL);

	gedit_spell_checker_add_word_to_personal (priv->checker, priv->misspelled_word);

	goto_next (dialog);
}

static void
ignore_button_clicked_handler (GtkButton               *button,
			       GeditSpellCheckerDialog *dialog)
{
	goto_next (dialog);
}

static void
ignore_all_button_clicked_handler (GtkButton               *button,
				   GeditSpellCheckerDialog *dialog)
{
	GeditSpellCheckerDialogPrivate *priv;

	priv = gedit_spell_checker_dialog_get_instance_private (dialog);

	g_return_if_fail (priv->misspelled_word != NULL);

	gedit_spell_checker_add_word_to_session (priv->checker, priv->misspelled_word);

	goto_next (dialog);
}

static void
change_button_clicked_handler (GtkButton               *button,
			       GeditSpellCheckerDialog *dialog)
{
	GeditSpellCheckerDialogPrivate *priv;
	gchar *change_to;

	priv = gedit_spell_checker_dialog_get_instance_private (dialog);

	g_return_if_fail (priv->misspelled_word != NULL);

	change_to = g_strdup (gtk_entry_get_text (priv->word_entry));
	g_return_if_fail (change_to != NULL);
	g_return_if_fail (change_to[0] != '\0');

	gedit_spell_checker_set_correction (priv->checker,
					    priv->misspelled_word,
					    change_to);

	gedit_spell_navigator_change (priv->navigator, priv->misspelled_word, change_to);
	g_free (change_to);

	goto_next (dialog);
}

/* double click on one of the suggestions is like clicking on "change" */
static void
suggestions_row_activated_handler (GtkTreeView             *view,
				   GtkTreePath             *path,
				   GtkTreeViewColumn       *column,
				   GeditSpellCheckerDialog *dialog)
{
	GeditSpellCheckerDialogPrivate *priv;

	priv = gedit_spell_checker_dialog_get_instance_private (dialog);

	change_button_clicked_handler (GTK_BUTTON (priv->change_button), dialog);
}

static void
change_all_button_clicked_handler (GtkButton               *button,
				   GeditSpellCheckerDialog *dialog)
{
	GeditSpellCheckerDialogPrivate *priv;
	gchar *change_to;

	priv = gedit_spell_checker_dialog_get_instance_private (dialog);

	g_return_if_fail (priv->misspelled_word != NULL);

	change_to = g_strdup (gtk_entry_get_text (priv->word_entry));
	g_return_if_fail (change_to != NULL);
	g_return_if_fail (change_to[0] != '\0');

	gedit_spell_checker_set_correction (priv->checker,
					    priv->misspelled_word,
					    change_to);

	gedit_spell_navigator_change_all (priv->navigator, priv->misspelled_word, change_to);
	g_free (change_to);

	goto_next (dialog);
}

static void
gedit_spell_checker_dialog_init (GeditSpellCheckerDialog *dialog)
{
	GeditSpellCheckerDialogPrivate *priv;
	GtkListStore *store;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell;
	GtkTreeSelection *selection;

	priv = gedit_spell_checker_dialog_get_instance_private (dialog);

	gtk_widget_init_template (GTK_WIDGET (dialog));

	/* Suggestion list */
	store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING);
	gtk_tree_view_set_model (priv->suggestions_view, GTK_TREE_MODEL (store));
	g_object_unref (store);

	/* Add the suggestions column */
	cell = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Suggestions"), cell,
							   "text", COLUMN_SUGGESTION,
							   NULL);

	gtk_tree_view_append_column (priv->suggestions_view, column);

	gtk_tree_view_set_search_column (priv->suggestions_view, COLUMN_SUGGESTION);

	selection = gtk_tree_view_get_selection (priv->suggestions_view);

	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	/* Connect signals */
	g_signal_connect (priv->word_entry,
			  "changed",
			  G_CALLBACK (word_entry_changed_handler),
			  dialog);

	g_signal_connect_object (selection,
				 "changed",
				 G_CALLBACK (suggestions_selection_changed_handler),
				 dialog,
				 0);

	g_signal_connect (priv->check_word_button,
			  "clicked",
			  G_CALLBACK (check_word_button_clicked_handler),
			  dialog);

	g_signal_connect (priv->add_word_button,
			  "clicked",
			  G_CALLBACK (add_word_button_clicked_handler),
			  dialog);

	g_signal_connect (priv->ignore_button,
			  "clicked",
			  G_CALLBACK (ignore_button_clicked_handler),
			  dialog);

	g_signal_connect (priv->ignore_all_button,
			  "clicked",
			  G_CALLBACK (ignore_all_button_clicked_handler),
			  dialog);

	g_signal_connect (priv->change_button,
			  "clicked",
			  G_CALLBACK (change_button_clicked_handler),
			  dialog);

	g_signal_connect (priv->change_all_button,
			  "clicked",
			  G_CALLBACK (change_all_button_clicked_handler),
			  dialog);

	g_signal_connect (priv->suggestions_view,
			  "row-activated",
			  G_CALLBACK (suggestions_row_activated_handler),
			  dialog);

	gtk_widget_grab_default (priv->change_button);
}

/**
 * gedit_spell_checker_dialog_new:
 * @parent: transient parent of the dialog.
 * @navigator: the #GeditSpellNavigator to use.
 *
 * Returns: a new #GeditSpellCheckerDialog widget.
 */
GtkWidget *
gedit_spell_checker_dialog_new (GtkWindow           *parent,
				GeditSpellNavigator *navigator)
{
	g_return_val_if_fail (GTK_IS_WINDOW (parent), NULL);
	g_return_val_if_fail (GEDIT_IS_SPELL_NAVIGATOR (navigator), NULL);

	return g_object_new (GEDIT_TYPE_SPELL_CHECKER_DIALOG,
			     "transient-for", parent,
			     "use-header-bar", TRUE,
			     "spell-navigator", navigator,
			     NULL);
}

/* ex:set ts=8 noet: */
