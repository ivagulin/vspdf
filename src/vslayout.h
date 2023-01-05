#ifndef _VSLAYOUT_H_
#define _VSLAYOUT_H_

#include <gtk/gtkcontainer.h>
#include <glib/poppler.h>

#define TYPE_VSLAYOUT (vslayout_get_type ())
#define VSLAYOUT(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_VSLAYOUT, VSLayout))

typedef enum {
	ZOOM_TO_PAGE = 0,
	ZOOM_TO_WIDTH,
	ZOOM_CUSTOM,
} ZoomMode ;

typedef struct _VSLayout
{
	GtkContainer parent;
	GList *children;

	ZoomMode zmode;
	double scale;

	PopplerDocument *document;
	double max_height;
	double max_width;
	int n_pages;
} VSLayout;

typedef struct _VSLayoutClass
{
	GtkContainerClass parent;
}VSLayoutClass;

GType vslayout_get_type();
void vslayout_set_zoom_mode(VSLayout *vslayout, ZoomMode new_mode);
ZoomMode vslayout_get_zoom_mode(VSLayout *vslayout);
int vslayout_get_page_position(VSLayout *vslayout, int page_num);
void vslayout_set_selection(VSLayout *vslayout, int page_num, PopplerRectangle *r);


#endif //_VSLAYOUT_H_
