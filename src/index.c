#include <gtk/gtktreestore.h>
#include <gtk/gtkcellrenderertext.h>
#include <strings.h>

#include "marshal.h"
#include "index.h"

#define PAGE_SELECTED_NAME "page-selected"

enum{
	COLUMN_TITLE,
	COLUMN_PAGE,
	COLUMN_PAGE_INT,
	COLUMN_MAX,
};

static void walk_index(GtkTreeStore *store, GtkTreeIter *parent, PopplerIndexIter *iter, PopplerDocument *document)
{
	do{
		PopplerAction* action = poppler_index_iter_get_action(iter);
		if(action->type != POPPLER_ACTION_GOTO_DEST)
			continue;

		//get page number
		PopplerActionGotoDest *action_dest = (PopplerActionGotoDest*)action;
		int pagenum = 0;
		if(action_dest->dest->page_num != 0)
			pagenum = action_dest->dest->page_num;
		else if(action_dest->dest->type == POPPLER_DEST_NAMED){
			PopplerDest *dest = poppler_document_find_dest(document, action_dest->dest->named_dest);
			if(dest){
				pagenum = dest->page_num;
				poppler_dest_free(dest);
			}
		}

		//insert value
		char *pagenum_str = g_strdup_printf("%d", pagenum);
		GtkTreeIter new_iter;
		gtk_tree_store_insert_with_values(store, &new_iter, parent, -1, 
										  COLUMN_TITLE, action_dest->title, 
										  COLUMN_PAGE, pagenum_str, 
										  COLUMN_PAGE_INT, pagenum,
										  -1);
		g_free(pagenum_str);

		//recurse if needed
		PopplerIndexIter *child = poppler_index_iter_get_child (iter);
		if(child)
			walk_index(store, &new_iter, child, document);

	}while(poppler_index_iter_next(iter));
}

GtkTreeModel* model_create(PopplerDocument* document)
{
	GtkTreeStore *store = gtk_tree_store_new(COLUMN_MAX, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);

	PopplerIndexIter *iter = poppler_index_iter_new (document);
	if(!iter){
		g_object_unref(store);
		return NULL;
	}
	walk_index (store, NULL, iter, document);
	poppler_index_iter_free (iter);	

	return GTK_TREE_MODEL(store);
}

typedef struct {
	GtkTreeView *tree;
	GtkTreePath *path;
	int diff;
	int goal;
} FindData ;

static inline gboolean row_expanded(GtkTreeView *tree, GtkTreeModel *model, GtkTreeIter *iter)
{
	GtkTreePath *path = gtk_tree_model_get_path(model, iter);
	return gtk_tree_view_row_expanded(tree, path);
}

static gboolean find_closest(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data)
{
	FindData *data = user_data;
	GtkTreeIter parent;
	int page_num = 0;

	if(gtk_tree_model_iter_parent(model, &parent, iter)
	   && !row_expanded(data->tree, model, &parent))
		return FALSE;
	
	gtk_tree_model_get(model, iter, COLUMN_PAGE_INT, &page_num, -1);

	int cur_diff = data->goal - page_num;
	if(cur_diff >= 0 && cur_diff < data->diff){
		if(data->path)
			gtk_tree_path_free(data->path);
		data->path = gtk_tree_path_copy(path);
		data->diff = cur_diff;
	}

	return FALSE;
}

static void page_changed(Index *self, int page);
static void cursor_changed(GtkTreeView *view, gpointer data)
{
	GtkTreePath *path;
	GtkTreeModel *model = gtk_tree_view_get_model(view);
	gtk_tree_view_get_cursor(view, &path, NULL);
	if(!path)
		return;
	
	GtkTreeIter iter;
	if(!gtk_tree_model_get_iter(model, &iter, path))
		return;

	int page_num = 0;
	gtk_tree_model_get(model, &iter, COLUMN_PAGE_INT, &page_num, -1);

	IndexClass *class = G_TYPE_INSTANCE_GET_CLASS(view, TYPE_INDEX, IndexClass);
	class->page_changed = NULL;
	g_signal_emit_by_name(view, PAGE_SELECTED_NAME, page_num - 1, NULL);
	class->page_changed = page_changed;
}

static void page_changed(Index *self, int page)
{
	GtkTreeView *tree = GTK_TREE_VIEW(self);
	GtkTreeModel *model = gtk_tree_view_get_model(tree);
	FindData data = {
		.path = NULL, .diff = G_MAXINT, 
		.goal = page+1, .tree = tree};
	
	gtk_tree_model_foreach(model, find_closest, &data);

	if(data.path){
		g_signal_handlers_block_by_func(self, cursor_changed, self);
		gtk_tree_view_set_cursor(tree, data.path, NULL, FALSE);
		gtk_tree_path_free(data.path);
		g_signal_handlers_unblock_by_func(self, cursor_changed, self);
	}
}

G_DEFINE_TYPE(Index, index, GTK_TYPE_TREE_VIEW);
static void index_class_init(IndexClass *class)
{
	g_signal_new (PAGE_SELECTED_NAME,
				  G_OBJECT_CLASS_TYPE (class),
				  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
				  0, NULL, NULL,
				  g_cclosure_user_marshal_VOID__INT_BOXED,
				  G_TYPE_NONE, 2,
				  G_TYPE_INT, POPPLER_TYPE_RECTANGLE);

	g_signal_new ("page-changed",
				  G_OBJECT_CLASS_TYPE (class),
				  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
				  G_STRUCT_OFFSET (IndexClass, page_changed),
				  NULL, NULL,
				  g_cclosure_marshal_VOID__INT,
				  G_TYPE_NONE, 1, G_TYPE_INT);
	class->page_changed = page_changed;
}

static void index_init(Index *self)
{
	GtkTreeView *tree = GTK_TREE_VIEW(self);
	
	//title column
	GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	int col = gtk_tree_view_insert_column_with_attributes(tree, -1, "Title", renderer, "text", COLUMN_TITLE, NULL);
	GtkTreeViewColumn *column = gtk_tree_view_get_column(tree, col-1);
	g_object_set(column, "expand", TRUE, NULL);
	
	//page column
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(tree, -1, "Page", renderer, "text", COLUMN_PAGE, NULL);

	//final config & signal connect
	gtk_tree_view_set_headers_visible(tree, FALSE);
	g_signal_connect(tree, "cursor-changed", G_CALLBACK(cursor_changed), tree);
}

GtkWidget* index_new(PopplerDocument* document)
{
	GtkTreeModel *model = model_create(document);
	if(!model)
		return NULL;
	
	GtkWidget *tree_w = g_object_new (TYPE_INDEX, NULL);
	gtk_tree_view_set_model(GTK_TREE_VIEW(tree_w), model);

	return tree_w;
}
