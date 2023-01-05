#include "thumbview.h"
#include "marshal.h"

const int min_width = 100;
const int border = 10;
const int page_gap = 20;
const int page_to_num_gap = 3;

static gboolean render_thumbs(gpointer data)
{
	Thumbview *self = THUMBVIEW(data);

	//find first uncached page
	int n_pages = MIN(poppler_document_get_n_pages(self->document), self->first_page + self->n_pages);
	int page_to_render = self->last_rendered + 1;
	for(; page_to_render<n_pages; page_to_render++){
		if(g_hash_table_lookup(self->thumbs, &page_to_render) == NULL)
			break;
	}
	if(page_to_render == n_pages)
		return FALSE;

	PopplerPage *page = poppler_document_get_page(self->document, page_to_render);

	//create pixmap and render
	GdkPixmap *pix = gdk_pixmap_new(GTK_WIDGET(self)->window, self->scaled_w, self->scaled_h, -1);
	cairo_t *cr = gdk_cairo_create(pix);
	cairo_scale(cr, self->scale, self->scale);
	cairo_set_source_rgb(cr, 1,1,1);
	cairo_rectangle(cr, 0,0, self->scaled_w / self->scale, self->scaled_h / self->scale);
	cairo_fill(cr);
	poppler_page_render(page, cr);

	//release
	cairo_destroy(cr);
	g_object_unref(page);

	//finish
	self->last_rendered = page_to_render;
	g_hash_table_insert(self->thumbs, g_memdup(&page_to_render, sizeof(int)), pix);
	gtk_widget_queue_draw(GTK_WIDGET(self));

	return TRUE;
}

static gboolean expose_event(GtkWidget *widget, GdkEventExpose *event)
{
	Thumbview *self = THUMBVIEW(widget);
	cairo_t *cr = gdk_cairo_create(widget->window);

	//fill bg
	cairo_set_source_rgb(cr, 0.2,0.2,0.2);
	cairo_rectangle(cr, 0,0, widget->allocation.width, widget->allocation.height);
	cairo_fill(cr);

	GdkPixmap *empty = gdk_pixmap_new(widget->window, self->scaled_w, self->scaled_h, -1);
	cairo_t *tmp_cr = gdk_cairo_create(empty);
	cairo_rectangle(tmp_cr, 0,0, self->scaled_w, self->scaled_h);
	cairo_set_source_rgb(tmp_cr, 1,1,1);
	cairo_fill(tmp_cr);
	cairo_destroy(tmp_cr);

	for(int i=0; i<self->n_pages; i++){
		int page = i+self->first_page;
		if(page >= poppler_document_get_n_pages(self->document))
			break;

		cairo_save(cr);

		int start_point = i*self->scaled_h + i*page_gap + border;
		cairo_translate(cr, border, start_point);

		//draw selection
		if(page == self->focused_page){
			cairo_set_source_rgb(cr, 0.7,0.7,0.7);
			cairo_rectangle(cr, -border, -2, self->scaled_w + border*2, self->scaled_h + 4);
			cairo_fill(cr);
		}

		//draw border
		cairo_rectangle(cr, 0,0, self->scaled_w, self->scaled_h);
		cairo_set_source_rgb(cr, 0,0,0);
		cairo_fill_preserve(cr);

		//draw page
		GdkPixmap *pix = g_hash_table_lookup(self->thumbs, &page);
		if(!pix)
			pix = empty;
		gdk_cairo_set_source_pixmap(cr, pix, 0,0);
		cairo_fill(cr);

		//draw page_num
		cairo_text_extents_t extent;
		char *page_num = g_strdup_printf("%d", i+self->first_page + 1);
		cairo_text_extents(cr, page_num, &extent);
		double text_x = self->scaled_w / 2 - extent.width / 2;
		double text_y = self->scaled_h + extent.height + page_to_num_gap;
		
		cairo_set_source_rgb(cr, 1,1,1);
		cairo_rectangle(cr, text_x - 2, text_y - extent.height - 2, extent.width+4, extent.height + 4);
		cairo_fill(cr);

		cairo_set_source_rgb(cr, 0,0,0);
		cairo_move_to(cr, text_x, text_y);
		cairo_show_text(cr, page_num);
		cairo_fill(cr);

		g_free(page_num);
		
		cairo_restore(cr);
	}

	cairo_destroy(cr);
	return TRUE;
}

static void scroll_update(Thumbview *self)
{
	self->adj->lower = 0;
	self->adj->upper = poppler_document_get_n_pages(self->document);
	self->adj->step_increment = 1;
	self->adj->page_increment = 1;
	self->adj->page_size = MAX(1, self->n_pages - 1);
	gtk_adjustment_changed(self->adj);

	for(int i=0; i<self->adj->upper; i++){
		if(i < self->first_page || i>self->first_page + self->n_pages)
			g_hash_table_remove(self->thumbs, &i);
	}
}

static void scroll_changed(Thumbview *self, GtkAdjustment *adj)
{
	self->first_page = adj->value;
	self->last_rendered = self->first_page - 1;
	g_idle_add_full(G_PRIORITY_LOW, render_thumbs, self, NULL);
	gtk_widget_queue_draw(GTK_WIDGET(self));
}

static void scroll_set(Thumbview *self, GtkAdjustment *horizontal, GtkAdjustment *vertical)
{
	//release current
	if(self->adj){
		g_object_unref(self->adj);
		self->adj = NULL;
	}

	if(!vertical)
		return;

	//set new
	g_object_ref(vertical);
	self->adj = vertical;

	g_signal_connect_swapped(self->adj, "value-changed", G_CALLBACK(scroll_changed), self);
	scroll_update(self);
}

static void page_changed(Thumbview *self, int page)
{
	self->focused_page = page;
	gtk_adjustment_clamp_page(self->adj, page, page);
	gtk_widget_queue_draw(GTK_WIDGET(self));
}

static gboolean configure_event(GtkWidget *widget, GdkEventConfigure *event)
{
	Thumbview *self = THUMBVIEW(widget);

	//rest cache
	g_hash_table_remove_all(self->thumbs);
	self->last_rendered = self->first_page - 1;
	g_idle_add_full(G_PRIORITY_LOW, render_thumbs, self, NULL);

	//calc page values
	double page_w, page_h;
	PopplerPage *page = poppler_document_get_page(self->document, self->first_page);
	poppler_page_get_size(page, &page_w, &page_h);
	self->scale = (widget->allocation.width - border*2) / page_w;
	self->scaled_w = page_w * self->scale;
	self->scaled_h = page_h * self->scale;
	self->n_pages = widget->allocation.height / (self->scaled_h + page_gap) + 1;
	g_object_unref(page);
	
	scroll_update(self);

	return TRUE;
}

static gboolean press_event(GtkWidget *widget, GdkEventButton *event)
{
	Thumbview *self = THUMBVIEW(widget);
	self->focused_page = self->first_page + (event->y - border) / (self->scaled_h + page_gap);
	g_signal_emit_by_name(self, "page-selected", self->focused_page, NULL);
	gtk_widget_queue_draw(widget);
	return TRUE;
}


static void finalize(GObject *obj)
{
	Thumbview *self = THUMBVIEW(obj);
	if(self->document)
		g_object_unref(self->document);
	if(self->adj)
		g_object_unref(self->adj);
	g_hash_table_destroy(self->thumbs);
}

static void thumbview_init(Thumbview *self)
{
	gtk_widget_set_size_request(GTK_WIDGET(self), min_width, -1);
	self->first_page = 0;
	self->n_pages = 1;
	self->focused_page = -1;
	self->thumbs = g_hash_table_new_full(g_int_hash, g_int_equal, g_free, g_object_unref);
	gtk_widget_add_events(GTK_WIDGET(self), GDK_BUTTON_PRESS_MASK);
}

static void thumbview_class_init(ThumbviewClass *class)
{
	G_OBJECT_CLASS(class)->finalize = finalize;
	GTK_WIDGET_CLASS(class)->expose_event = expose_event;
	GTK_WIDGET_CLASS(class)->configure_event = configure_event;
	GTK_WIDGET_CLASS(class)->button_press_event = press_event;
 
	GTK_WIDGET_CLASS(class)->set_scroll_adjustments_signal =
		g_signal_new ("set-scroll-adjustments",
					  G_OBJECT_CLASS_TYPE (class),
					  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
					  G_STRUCT_OFFSET (ThumbviewClass, set_scroll_adjustments),
					  NULL, NULL,
					  g_cclosure_user_marshal_VOID__OBJECT_OBJECT,
					  G_TYPE_NONE, 2,
					  GTK_TYPE_ADJUSTMENT,
					  GTK_TYPE_ADJUSTMENT);
	class->set_scroll_adjustments = scroll_set;

	g_signal_new ("page-selected",
				  G_OBJECT_CLASS_TYPE (class),
				  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
				  0, NULL, NULL,
				  g_cclosure_user_marshal_VOID__INT_BOXED,
				  G_TYPE_NONE, 2,
				  G_TYPE_INT, POPPLER_TYPE_RECTANGLE);
	
	g_signal_new ("page-changed",
				  G_OBJECT_CLASS_TYPE (class),
				  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
				  G_STRUCT_OFFSET (ThumbviewClass, page_changed),
				  NULL, NULL,
				  g_cclosure_marshal_VOID__INT,
				  G_TYPE_NONE, 1, G_TYPE_INT);
	class->page_changed = page_changed;
}
G_DEFINE_TYPE(Thumbview, thumbview, GTK_TYPE_DRAWING_AREA);

GtkWidget *thumbview_new(PopplerDocument* document)
{
	GtkWidget *retval = g_object_new(TYPE_THUMBVIEW, NULL);
	THUMBVIEW(retval)->document = document;
	g_object_ref(document);
	return retval;
}
