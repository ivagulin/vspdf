#include <string.h>
#include <gtk/gtk.h>

#include "findbar.h"
#include "marshal.h"

enum{
	COLUMN_TEXT,
	COLUMN_PAGE,
	COLUMN_RECT,
	COLUMN_MAX,
};

static gchar* normalize(gchar *text, const char* needle)
{
	GString* stripped = g_string_new(NULL);
	
	//skip !isalnum
	gunichar* unitext = g_utf8_to_ucs4_fast(text, -1, NULL);
	for(gunichar *it = unitext; *it; it++){
		if(g_unichar_isalnum(*it))
			g_string_append_unichar(stripped, *it);
		else if(g_unichar_isspace(*it))
			g_string_append_unichar(stripped, g_utf8_get_char(" "));
	}
	g_free(unitext);

	//escape
	char *escaped = g_markup_escape_text(stripped->str, -1);
	g_string_free(stripped, TRUE);

	//insert <bold>
	char *escaped_down = g_utf8_strdown(escaped, -1);
	char *needle_down = g_utf8_strdown(needle, -1);
	char *needle_down_ptr = strstr(escaped_down, needle_down);
	long needle_down_offset = g_utf8_pointer_to_offset(escaped_down, needle_down_ptr);
	char *needle_ptr_start = g_utf8_offset_to_pointer(escaped, needle_down_offset);
	char *needle_ptr_end = g_utf8_offset_to_pointer(escaped, needle_down_offset + g_utf8_strlen(needle, -1));
	g_free(needle_down);
	g_free(escaped_down);
	
	int offset = needle_ptr_start - escaped;
	GString *res = g_string_new_len(escaped, offset);
	g_string_append(res, "<b>");
	g_string_append_len(res, needle_ptr_start, needle_ptr_end - needle_ptr_start);
	g_string_append(res, "</b>");
	g_string_append(res, needle_ptr_end);
		   
	return res->str;
}

static gboolean find_step(gpointer data)
{
	Findbar *self = FINDBAR(data);
	int n_pages = poppler_document_get_n_pages(self->document);

	if(self->to_search < 0 || self->to_search >= n_pages)
		return FALSE;

	PopplerPage *page = poppler_document_get_page(self->document, self->to_search);
	if(!page)
		return FALSE;

	GList *rects =  poppler_page_find_text(page, gtk_entry_get_text(self->entry));
	for(GList *it = rects; it; it=it->next){
		PopplerRectangle *r = it->data;
		PopplerRectangle resized = {r->x1 + 0.5, r->y2 + 0.5,
									r->x2 - 0.5, r->y1 - 0.5};
		char *ctx = poppler_page_get_selected_text(page, POPPLER_SELECTION_WORD, &resized);
		char *normalized = normalize(ctx, gtk_entry_get_text(self->entry));

		GtkTreeIter iter;
		gtk_list_store_append(self->model, &iter);
		gtk_list_store_set(self->model, &iter, 
						   COLUMN_TEXT, normalized, 
						   COLUMN_PAGE, self->to_search, 
						   COLUMN_RECT, &resized, -1);
		g_free(ctx);
	}
	
	self->to_search++;
	gtk_progress_bar_set_fraction(self->bar, (double)self->to_search / n_pages);
	g_list_free(rects);
	g_object_unref(page);
	return TRUE;
}

static void find_stop(gpointer data)
{
	Findbar *self = FINDBAR(data);
	g_source_remove(self->find_id);
	self->find_id = 0;
	gtk_button_set_label(self->button, GTK_STOCK_FIND);
}

static void find_clicked(Findbar *self)
{
	if(!strlen(gtk_entry_get_text(self->entry)))
		return;

	if(strcmp(gtk_button_get_label(self->button), GTK_STOCK_STOP) == 0){
		find_stop(self);
		return;
	}

	gtk_button_set_label(self->button, GTK_STOCK_STOP);
	gtk_list_store_clear(self->model);
	self->to_search = 0;
	self->find_id = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, find_step, self, find_stop);
}

static void cursor_changed(Findbar *self)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	gtk_tree_view_get_cursor(self->view, &path, NULL);
	
	if(!gtk_tree_model_get_iter(GTK_TREE_MODEL(self->model), &iter, path))
		return;

	int page;
	PopplerRectangle *rect;
	gtk_tree_model_get(GTK_TREE_MODEL(self->model), &iter, COLUMN_PAGE, &page, COLUMN_RECT, &rect, -1);
	g_signal_emit_by_name(self, "page-selected", page, rect);
}

static gboolean allocate(Findbar *self, GtkAllocation *allocation)
{
	g_object_set(self->cell, "wrap-width", allocation->width, NULL);
	return FALSE;
}

static void findbar_init(Findbar *self)
{
	self->model =  gtk_list_store_new(COLUMN_MAX, G_TYPE_STRING, G_TYPE_INT, POPPLER_TYPE_RECTANGLE);
	self->view = (GtkTreeView*)gtk_tree_view_new_with_model(GTK_TREE_MODEL(self->model));
	self->bar = (GtkProgressBar*)gtk_progress_bar_new();
	self->entry = (GtkEntry*)gtk_entry_new();

	self->button = (GtkButton*)gtk_button_new_from_stock(GTK_STOCK_FIND);
	GtkBox *hbox = (GtkBox*)gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(hbox, GTK_WIDGET(self->entry), TRUE, TRUE, 0);
	gtk_box_pack_start(hbox, GTK_WIDGET(self->button), FALSE, FALSE, 0);

	GtkScrolledWindow *sw = (GtkScrolledWindow*)gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(sw), GTK_WIDGET(self->view));
	gtk_scrolled_window_set_policy(sw, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	gtk_box_pack_start(GTK_BOX(self), GTK_WIDGET(hbox), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(self), GTK_WIDGET(self->bar), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(self), GTK_WIDGET(sw), TRUE, TRUE, 0);

	GtkRequisition req_button, req_entry;
	gtk_widget_size_request(GTK_WIDGET(self->button), &req_button);
	gtk_widget_size_request(GTK_WIDGET(self->entry), &req_entry);
	self->cell = gtk_cell_renderer_text_new();
	g_object_set(self->cell, "wrap-mode", PANGO_WRAP_CHAR, NULL);
	gtk_tree_view_insert_column_with_attributes(self->view, -1, "text", self->cell,
												"markup", COLUMN_TEXT, NULL);
	gtk_tree_view_set_headers_visible(self->view, FALSE);
	gtk_tree_view_set_rules_hint(self->view, TRUE);
	

	g_signal_connect_swapped(self->button, "clicked", G_CALLBACK(find_clicked), self);
	g_signal_connect_swapped(self->entry, "activate", G_CALLBACK(find_clicked), self);
	g_signal_connect_swapped(self->view, "cursor-changed", G_CALLBACK(cursor_changed), self);
	g_signal_connect_swapped(self->view, "size-allocate", G_CALLBACK(allocate), self);
}

static void findbar_class_init(FindbarClass *class)
{
	g_signal_new ("page-selected",
				  G_OBJECT_CLASS_TYPE (class),
				  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
				  0, NULL, NULL,
				  g_cclosure_user_marshal_VOID__INT_BOXED,
				  G_TYPE_NONE, 2,
				  G_TYPE_INT, POPPLER_TYPE_RECTANGLE);
}
G_DEFINE_TYPE(Findbar, findbar, GTK_TYPE_VBOX);

GtkWidget *findbar_new(PopplerDocument* document)
{
	Findbar *bar = g_object_new(TYPE_FINDBAR, NULL);
	bar->document = document;
	return GTK_WIDGET(bar) ;
}
