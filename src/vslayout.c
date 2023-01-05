#include "vslayout.h"
#include "vspage.h"

static GtkContainerClass *parent_class; 

enum Props {
	PROP_DOCUMENT = 1,
};

typedef struct _LayoutChild
{
	double width;
	double height;
	VSPage *vspage;
} LayoutChild;


static void vsremove(GtkContainer *container, GtkWidget *widget)
{
}

static void forall(GtkContainer *container, gboolean include_internals, 
				   GtkCallback callback, gpointer callback_data)
{
	VSLayout *self = VSLAYOUT(container);
	for(GList *it = self->children; it; it=it->next){
		LayoutChild *child = it->data;
		callback(GTK_WIDGET(child->vspage), callback_data);
	}
}

static void realize(GtkWidget *widget)
{
	GdkWindowAttr attributes;
	gint attributes_mask;

	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

	//window attrs
	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = widget->allocation.x;
	attributes.y = widget->allocation.y;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.colormap = gtk_widget_get_colormap (widget);
	attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK;

	//create window
	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
	widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);

	//setup some stuff
	gdk_window_set_user_data (widget->window, widget);
	widget->style = gtk_style_attach (widget->style, widget->window);
	gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}


static void size_request(GtkWidget *widget, GtkRequisition *requisition)
{
	VSLayout *self = VSLAYOUT(widget);
	double height = 0;
	
	//calculate height of vslayout
	for(GList *it = self->children; it; it=it->next){
		LayoutChild *child = it->data;
		height += child->height * self->scale;
	}
	
	requisition->height = height;
}

static void size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	VSLayout *self = VSLAYOUT(widget);
	GTK_WIDGET_CLASS(parent_class)->size_allocate(widget, allocation);

	//calc scale
	double new_scale = allocation->width / self->max_width;
	if(new_scale != self->scale){
		self->scale = new_scale;
		gtk_widget_queue_resize(widget);
	}

	double last_height = 0;
	//distribute space among vspages
	for(GList *it = self->children; it; it=it->next){
		LayoutChild *child=it->data;
		GtkAllocation allocation = {0, last_height, child->width*self->scale, child->height*self->scale};
		gtk_widget_size_allocate(GTK_WIDGET(child->vspage), &allocation);
		last_height += allocation.height;
	}
}

static void finalize(GObject *obj)
{
	VSLayout *self = VSLAYOUT(obj);
	g_list_foreach(self->children, (GFunc)g_free, NULL);
	g_list_free(self->children);

	G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	VSLayout *self = VSLAYOUT(object);

	switch (prop_id){
	case PROP_DOCUMENT:
		self->document = g_value_get_object (value);
		self->n_pages = poppler_document_get_n_pages(self->document);
		g_object_ref(self->document);
		break;
	}

}

static void constructor(GObject* obj)
{
	VSLayout *self = VSLAYOUT(obj);

	self->scale = 1;

	for(int i=0; i<self->n_pages; i++){
		PopplerPage *page = poppler_document_get_page(self->document, i);
		if(!page)
			continue;

		//save size
		LayoutChild *child = g_new0(LayoutChild, 1);
		poppler_page_get_size(page, &child->width, &child->height);

		//max values
		self->max_width = MAX(self->max_width, child->width);
		self->max_height = MAX(self->max_height, child->height);
		
		//create vspage
		child->vspage = g_object_new(TYPE_VSPAGE, "page", page, NULL);
		gtk_widget_set_parent(GTK_WIDGET(child->vspage), GTK_WIDGET(self));

		//add to collection
		self->children = g_list_append(self->children, child);
	}
}

G_DEFINE_TYPE(VSLayout, vslayout, GTK_TYPE_CONTAINER);
static void vslayout_class_init(VSLayoutClass *class)
{
	GObjectClass *gobject_class = (GObjectClass*) class;
  
	gobject_class->finalize = finalize;
	gobject_class->set_property = set_property;
	gobject_class->constructed = constructor;

	GTK_WIDGET_CLASS(class)->realize = realize;
	GTK_WIDGET_CLASS(class)->size_allocate = size_allocate;
	GTK_WIDGET_CLASS(class)->size_request = size_request;

	GTK_CONTAINER_CLASS(class)->forall = forall;
	GTK_CONTAINER_CLASS(class)->remove = vsremove;

	g_object_class_install_property (gobject_class,
									 PROP_DOCUMENT,
									 g_param_spec_object ("document",
														  "Poppler document",
														  "Poppler document which this vslayout draw",
														  POPPLER_TYPE_DOCUMENT,
														  G_PARAM_CONSTRUCT_ONLY|G_PARAM_WRITABLE));

	parent_class = g_type_class_peek_parent (class);
}

static void vslayout_init(VSLayout *self)
{
}

int vslayout_get_page_position(VSLayout *vslayout, int page_num)
{
	GList *it = vslayout->children;
	int position = 0;
	
	if(page_num >= vslayout->n_pages)
		return -1;

	while(page_num--){
		LayoutChild *child = it->data;
		position += child->height * vslayout->scale;
		it = it->next;
	}

	return position;
}

