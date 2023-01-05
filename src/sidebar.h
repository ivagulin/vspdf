#ifndef _SIDEBAR_H_
#define _SIDEBAR_H_

#include <glib/poppler.h>
#include <gtk/gtkframe.h>

#define TYPE_SIDEBAR (sidebar_get_type ())
#define SIDEBAR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_SIDEBAR, Sidebar))

typedef enum SidebarMode {
	SIDEBAR_MODE_INDEX,
	SIDEBAR_MODE_THUMB,
	SIDEBAR_MODE_FIND,
} SidebarMode;

typedef struct _Sidebar
{
	GtkFrame parent;
	PopplerDocument *document;
	gboolean have_index;
	GtkWidget *seeker;
} Sidebar;

typedef struct _SidebarClass
{
	GtkFrameClass parent;
	void (*page_changed)(Sidebar*, int);
} SidebarClass;

GtkWidget *sidebar_new(PopplerDocument*);
void sidebar_set_mode(Sidebar*, SidebarMode mode);
SidebarMode sidebar_get_mode(Sidebar*);
gboolean sidebar_have_index(Sidebar*);
GType sidebar_get_type();

#endif //_SIDEBAR_H_
