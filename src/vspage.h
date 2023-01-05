#ifndef _VSPAGE_H_
#define _VSPAGE_H_

#include <gtk/gtkdrawingarea.h>
#include <glib/poppler.h>

#define TYPE_VSPAGE (vspage_get_type ())
#define VSPAGE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_VSPAGE, VSPage))

typedef struct _VSPage
{
	GtkDrawingArea parent;

	GdkPixmap *pixmap;
	PopplerRectangle selection; //in page coords

	PopplerPage *page;
	gdouble scale; //widget_size / page_size

	guint render_job;
	gboolean rendered;
	GdkVisibilityState visibility;

}VSPage;

typedef struct _VSPageClass
{
	GtkDrawingAreaClass parent;
}VSPageClass;

GType vspage_get_type();
void vspage_get_selection(VSPage *vspage, PopplerRectangle *selection);
void vspage_set_selection(VSPage *vspage, PopplerRectangle *selection);

#endif //_VSPAGE_H_
