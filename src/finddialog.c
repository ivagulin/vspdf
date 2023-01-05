#include <gtk/gtk.h>

#include "finddialog.h"
#include "marshal.h"

static gboolean search_iteration(gpointer data)
{
	FindDialog *me = data;
	gboolean emited = FALSE;
	int n_pages = poppler_document_get_n_pages(me->document);

	if(me->places){
//		PopplerRectangle *r = me->places->data;
//		g_print("page %d, coords: %.0f:%.0f - %.0f:%.0f\n", me->current_page, r->x1, r->y1, r->x2, r->y2);
		g_signal_emit_by_name(me, "page-selected", me->current_page-1, me->places->data);
		me->places = me->places->next;
		emited = TRUE;
	}else if(me->current_page < n_pages){
		PopplerPage *page = poppler_document_get_page(me->document, me->current_page);
		me->places = poppler_page_find_text(page, gtk_entry_get_text(me->entry));
		g_object_unref(page);
		me->current_page++;
	}

	gtk_progress_set_value(me->progress, me->current_page);

	return emited == FALSE && (me->current_page < n_pages || me->places);
}

static void start_search(FindDialog *me)
{
	g_idle_add(search_iteration, me);
}

static void finddialog_init(FindDialog *me)
{
	GtkDialog *dialog = GTK_DIALOG(me);

	gtk_window_set_title(GTK_WINDOW(me), "Find text");

	//add find button
	GtkWidget *find_button = gtk_button_new_from_stock(GTK_STOCK_FIND);
	gtk_container_add(GTK_CONTAINER (dialog->action_area), find_button);
	g_signal_connect_swapped(find_button, "clicked", G_CALLBACK(start_search), me);

	//message with entry
	GtkWidget *label = gtk_label_new("Text to find:");
	me->entry = (GtkEntry*)gtk_entry_new();
	GtkWidget *hbox = gtk_hbox_new(0, 1);
	gtk_container_add(GTK_CONTAINER(hbox), label);
	gtk_container_add(GTK_CONTAINER(hbox), GTK_WIDGET(me->entry));
	gtk_container_add(GTK_CONTAINER (dialog->vbox), hbox);
	g_signal_connect_swapped(me->entry, "activate", G_CALLBACK(start_search), me);

	//progress bar
	me->progress = (GtkProgress*)gtk_progress_bar_new();
	gtk_container_add(GTK_CONTAINER (dialog->vbox), GTK_WIDGET(me->progress));

}

static void finddialog_class_init(FindDialogClass *class)
{
	g_signal_new ("page-selected",
				  G_OBJECT_CLASS_TYPE (class),
				  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
				  0, NULL, NULL,
				  g_cclosure_user_marshal_VOID__INT_BOXED,
				  G_TYPE_NONE, 2,
				  G_TYPE_INT, POPPLER_TYPE_RECTANGLE);
}

G_DEFINE_TYPE(FindDialog, finddialog, GTK_TYPE_DIALOG);

FindDialog* finddialog_new(GtkWindow *parent, PopplerDocument *document, int current_page)
{
	FindDialog *me = g_object_new(TYPE_FINDDIALOG, NULL);
	int n_pages = poppler_document_get_n_pages(document);

	me->places = NULL;
	me->current_page = current_page;
	me->document = document;
	gtk_progress_configure(me->progress, me->current_page, 0, n_pages-1);

	gtk_window_set_transient_for(GTK_WINDOW(me), parent);

	gtk_widget_show_all(GTK_WIDGET(me));

	return me;
}
