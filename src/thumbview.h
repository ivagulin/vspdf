#ifndef _THUMBVIEW_H_
#define _THUMBVIEW_H_

#include <gtk/gtkdrawingarea.h>
#include <glib/poppler.h>

#define TYPE_THUMBVIEW (thumbview_get_type ())
#define THUMBVIEW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_THUMBVIEW, Thumbview))

typedef struct _Thumbview
{
	GtkDrawingArea parent;
	GHashTable *thumbs;
	PopplerDocument *document;

	//scroll
	int first_page;
	int n_pages;
	int focused_page;
	int last_rendered;
	GtkAdjustment *adj;

	//page scale
	double scale;
	double scaled_w;
	double scaled_h;
	
} Thumbview;


typedef struct _ThumbviewClass
{
	GtkDrawingAreaClass parent;
	void (*set_scroll_adjustments)(Thumbview*, GtkAdjustment *, GtkAdjustment *);
	void (*page_changed)(Thumbview*, int);
} ThumbviewClass;

GtkWidget *thumbview_new(PopplerDocument*);
GType thumbview_get_type();

#endif //_THUMBVIEW_H_
