#ifndef _FINDBAR_H_
#define _FINDBAR_H_

#include <gtk/gtkvbox.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtktreeview.h>
#include <glib/poppler.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkcellrenderer.h>

#define TYPE_FINDBAR (findbar_get_type ())
#define FINDBAR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_FINDBAR, Findbar))

typedef struct _Findbar
{
	GtkVBox parent;
	GtkTreeView *view;
	GtkListStore *model;
	GtkEntry *entry;
	GtkButton *button;
	GtkProgressBar *bar;
	GtkCellRenderer *cell;
	PopplerDocument *document;
	int to_search;
	guint find_id;
} Findbar;


typedef struct _FindbarClass
{
	GtkVBoxClass parent;
} FindbarClass;

GtkWidget *findbar_new(PopplerDocument*);
GType findbar_get_type();

#endif //_FINDBAR_H_
