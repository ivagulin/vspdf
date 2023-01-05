#ifndef _INDEX_H_
#define _INDEX_H_

#include <gtk/gtktreeview.h>
#include <glib/poppler.h>

#define TYPE_INDEX (index_get_type ())
#define INDEX(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_INDEX, Index))

typedef struct _Index
{
	GtkTreeView parent;
} Index;


typedef struct _IndexClass
{
	GtkTreeViewClass parent;
	void (*page_changed)(Index*, int);
} IndexClass;

GtkWidget *index_new(PopplerDocument*);
GType index_get_type();

#endif //_INDEX_H_
