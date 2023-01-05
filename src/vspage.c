#include "vspage.h"

#include <gtk/gtkclipboard.h>
#include <strings.h>
#include "marshal.h"

static GtkWidgetClass *parent_class = NULL;
enum Props{
	PROP_PAGE = 1,
};

static gboolean is_rect_empty(PopplerRectangle *rect)
{
	return rect->x1 == rect->x2 && rect->y1 == rect->y2;
}

static gboolean render(VSPage *self)
{
	if(!self->pixmap)
		return FALSE;

	gdouble width, height;
	poppler_page_get_size(self->page, &width, &height);

	cairo_t *cr = gdk_cairo_create(GDK_DRAWABLE(self->pixmap));
	cairo_scale(cr, self->scale, self->scale);

	//render page
	cairo_save(cr);
	poppler_page_render(self->page, cr);

	cairo_destroy(cr);
	gtk_widget_queue_draw(GTK_WIDGET(self));

	//set some flags
	self->render_job = 0;
	self->rendered = TRUE;
 
	return FALSE;
}

static void render_cancel(VSPage *self)
{
	if(self->render_job){
		g_source_remove(self->render_job);
		self->render_job = 0;
	}
}

static void render_queue(VSPage *self)
{
	render_cancel(self);
	if(self->rendered == FALSE)
		self->render_job = g_idle_add((GSourceFunc)render, self);
}

static gboolean expose_event(GtkWidget *widget, GdkEventExpose *event)
{
	VSPage *self = VSPAGE(widget);
	GdkGC *gc = gdk_gc_new(GDK_DRAWABLE(widget->window));
	PopplerColor black={0,0,0}, white = {G_MAXUINT16, G_MAXUINT16, G_MAXUINT16};

	if(self->pixmap)
		gdk_draw_drawable(GDK_DRAWABLE(widget->window), gc, GDK_DRAWABLE(self->pixmap),
						  0,0, 0,0, -1,-1);

	//render selection if not empty
	if(!is_rect_empty(&self->selection)){
		cairo_t *cr = gdk_cairo_create(GDK_DRAWABLE(widget->window));
		//cairo_translate(cr, self->dst.x, self->dst.y);
		cairo_scale(cr, self->scale, self->scale);
		poppler_page_render_selection(self->page, cr, &self->selection, NULL, POPPLER_SELECTION_WORD, &white, &black);
		cairo_destroy(cr);
	}

	g_object_unref(gc);
	return TRUE;	
}

static void pixmap_destroy(VSPage *self)
{
	if(self->pixmap){
		g_object_unref(self->pixmap);
		self->pixmap = NULL;
	}
}

static void pixmap_create(VSPage *self)
{
	GtkWidget *widget = GTK_WIDGET(self);
	gdouble page_w, page_h;
	poppler_page_get_size(self->page, &page_w, &page_h);

	//destroy old
	pixmap_destroy(self);

	//create pixmap
	int scaled_w = page_w*self->scale;
	int scaled_h = page_h*self->scale;
	if(scaled_w <= 0 || scaled_h <= 0)
		return;

	self->pixmap = gdk_pixmap_new(widget->window, scaled_w, scaled_h, -1);
	self->rendered = FALSE;
	
	//fill with white and draw black border
	cairo_t *cr = gdk_cairo_create(GDK_DRAWABLE(self->pixmap));
	cairo_set_source_rgb(cr, 1,1,1);
	cairo_rectangle(cr, 0,0, scaled_w,scaled_h);
	cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, 0,0,0);
	cairo_stroke(cr);
	cairo_destroy(cr);	

	if(self->visibility != GDK_VISIBILITY_FULLY_OBSCURED)
		render_queue(self);	
}

static gboolean configure_event(GtkWidget *widget, GdkEventConfigure *event)
{
	VSPage *self = VSPAGE(widget);

	gdouble page_w, page_h;
	poppler_page_get_size(self->page, &page_w, &page_h);

	//calc scale
	gdouble sx = widget->allocation.width / page_w;
	gdouble sy = widget->allocation.height/ page_h;
	self->scale = MIN(sx, sy);

	if(self->visibility != GDK_VISIBILITY_FULLY_OBSCURED)
		pixmap_create(self);

	return TRUE;
}

static gboolean press_event(GtkWidget *widget, GdkEventButton *event)
{
	VSPage *self = VSPAGE(widget);

	if(event->type == GDK_BUTTON_PRESS && event->button == 1){
		GdkCursor *cursor = gdk_cursor_new (GDK_XTERM);
		gdk_window_set_cursor (widget->window, cursor);
		gdk_flush ();

		self->selection.x2 = self->selection.x1 = (event->x) / self->scale;
		self->selection.y2 = self->selection.y1 = (event->y) / self->scale;
		gtk_widget_queue_draw(GTK_WIDGET(self));
	}

	return TRUE;
}

static gboolean motion_event (GtkWidget *widget, GdkEventMotion *event)
{
	VSPage *self = VSPAGE(widget);

	self->selection.x2 = (event->x) / self->scale;
	self->selection.y2 = (event->y) / self->scale;
	gtk_widget_queue_draw(GTK_WIDGET(self));

	gdk_event_request_motions(event);

	return TRUE;
}

static gboolean visibility_changed(GtkWidget *widget, GdkEventVisibility *event)
{
	VSPage *self = VSPAGE(widget);

	self->visibility = event->state;
	if(self->visibility == GDK_VISIBILITY_FULLY_OBSCURED){
		pixmap_destroy(self);
		render_cancel(self);
	}else{
		if(self->pixmap == NULL)
			pixmap_create(self);
	}
	
	return FALSE;
}

static gboolean release_event(GtkWidget *widget, GdkEventButton *event)
{
	VSPage *self = VSPAGE(widget);

	gdk_window_set_cursor (widget->window, NULL);
	gdk_flush ();

	PopplerRectangle rect = {self->selection.x1, self->selection.y2,
							 self->selection.x2, self->selection.y1};
	gchar *text = NULL;
	GtkClipboard *cb = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
	text = poppler_page_get_selected_text(self->page, POPPLER_SELECTION_WORD, &rect);
	if(text)
		gtk_clipboard_set_text(cb, text, -1);
	g_free(text);

	return TRUE;
}

static void finalize(GObject *obj)
{
	VSPage *self = VSPAGE(obj);
	
	pixmap_destroy(self);
	if(self->page)
		g_object_unref(self->page);

	G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	VSPage *self = VSPAGE(object);

	switch (prop_id){
	case PROP_PAGE:
		self->page = g_value_get_object (value);
		g_object_ref(self->page);
		break;
	}

}

G_DEFINE_TYPE(VSPage, vspage, GTK_TYPE_DRAWING_AREA);
static void vspage_class_init(VSPageClass *class)
{
	parent_class = g_type_class_peek_parent (class);

	G_OBJECT_CLASS(class)->finalize = finalize;
	G_OBJECT_CLASS(class)->set_property = set_property;

	GTK_WIDGET_CLASS(class)->expose_event = expose_event;
	GTK_WIDGET_CLASS(class)->configure_event = configure_event;
	GTK_WIDGET_CLASS(class)->visibility_notify_event = visibility_changed;

	GTK_WIDGET_CLASS(class)->motion_notify_event = motion_event;
	GTK_WIDGET_CLASS(class)->button_press_event = press_event;
	GTK_WIDGET_CLASS(class)->button_release_event = release_event;

	g_object_class_install_property (G_OBJECT_CLASS(class),
									 PROP_PAGE,
									 g_param_spec_object ("page",
														  "Poppler page",
														  "Poppler page which this widget displays",
														  POPPLER_TYPE_PAGE,
														  G_PARAM_CONSTRUCT_ONLY|G_PARAM_WRITABLE));
}

static void vspage_init(VSPage *self)
{
	self->scale = 1;
	self->visibility = GDK_VISIBILITY_FULLY_OBSCURED;
	gtk_widget_add_events(GTK_WIDGET(self), GDK_SCROLL_MASK|GDK_BUTTON1_MOTION_MASK|GDK_POINTER_MOTION_HINT_MASK|
						  GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK|GDK_KEY_PRESS_MASK|GDK_VISIBILITY_NOTIFY_MASK);
	GTK_WIDGET_SET_FLAGS(GTK_WIDGET(self), GTK_CAN_FOCUS);	
}

void vspage_set_selection(VSPage *self, PopplerRectangle *sel)
{
	self->selection = *sel;
	gtk_widget_queue_draw(GTK_WIDGET(self));
}

