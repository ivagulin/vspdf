#include <gtk/gtk.h>

#include "sidebar.h"
#include "marshal.h"
#include "findbar.h"
#include "thumbview.h"
#include "index.h"

static void page_changed(Sidebar *self, int page)
{
	int signal = g_signal_lookup("page-changed", G_TYPE_FROM_INSTANCE(self->seeker));
	if(signal)
		g_signal_emit_by_name(self->seeker, "page-changed", page);
}

static void page_selected_up(Sidebar *self, int page, PopplerRectangle *rect)
{
	g_signal_emit_by_name(self, "page-selected", page, rect);
}

void sidebar_set_mode(Sidebar* self, SidebarMode mode)
{

	switch(mode){
	case SIDEBAR_MODE_INDEX:
		self->seeker = index_new(self->document);
		break;
	case SIDEBAR_MODE_THUMB:
		self->seeker = thumbview_new(self->document);
		break;
	case SIDEBAR_MODE_FIND:
		self->seeker = findbar_new(self->document);
		break;
	}

	GtkWidget *child = gtk_bin_get_child(GTK_BIN(self));
	if(child)
		gtk_container_remove(GTK_CONTAINER(self), child);

	if(mode == SIDEBAR_MODE_FIND){
		gtk_container_add(GTK_CONTAINER(self), self->seeker);
	} else {
		GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
		GtkPolicyType h_policy = (mode == SIDEBAR_MODE_THUMB ? GTK_POLICY_NEVER : GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), h_policy, GTK_POLICY_AUTOMATIC);
		gtk_container_add(GTK_CONTAINER(sw), self->seeker);
		gtk_container_add(GTK_CONTAINER(self), sw);
	}

	gtk_widget_show_all(GTK_WIDGET(self));
	g_signal_connect_swapped(self->seeker, "page-selected", G_CALLBACK(page_selected_up), self);
}

static void sidebar_init(Sidebar *self)
{
}

static void sidebar_class_init(SidebarClass *class)
{
	class->page_changed = page_changed;
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
				  G_STRUCT_OFFSET (SidebarClass, page_changed), NULL, NULL,
				  g_cclosure_marshal_VOID__INT,
				  G_TYPE_NONE, 1, G_TYPE_INT);
}
G_DEFINE_TYPE(Sidebar, sidebar, GTK_TYPE_FRAME);

GtkWidget *sidebar_new(PopplerDocument* doc)
{
	Sidebar *self = g_object_new(TYPE_SIDEBAR, NULL);

	self->document = doc;
	PopplerIndexIter *iter = poppler_index_iter_new(doc);
	if(iter){
		self->have_index = TRUE;
		poppler_index_iter_free(iter);
	}

	if(self->have_index)
		sidebar_set_mode(self, SIDEBAR_MODE_INDEX);
	else
		sidebar_set_mode(self, SIDEBAR_MODE_THUMB);

	return GTK_WIDGET(self);
}

gboolean sidebar_have_index(Sidebar *self)
{
	return self->have_index;
}
