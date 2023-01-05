#include "sidebar.h"
#include "vspage.h"
#include "stock.h"
#include <gtk/gtkstock.h>


//App
static GtkUIManager *ui_manager = NULL;
static GtkWindow *window = NULL;
static GtkWidget *spin = NULL;
static GtkWidget *scale = NULL;
static VSPage *vspage = NULL;
static gchar* pdfname = NULL;
static GList* back_actions = NULL;
static GList* forward_actions = NULL;
static Sidebar *sidebar = NULL;

static PopplerDocument *document = NULL;
static int n_pages = 0;

static gboolean rects_equal(PopplerRectangle *r1, PopplerRectangle *r2)
{
	return memcmp(r1, r2, sizeof(PopplerRectangle)) == 0;
}

static gchar* get_uri(const gchar *fileName)
{
	gchar *uri;
    gchar *absolute = NULL;
    if ( g_path_is_absolute (fileName) ){
        absolute = g_strdup (fileName);
    } else {
        gchar *current = g_get_current_dir ();
        absolute = g_build_filename (current, fileName, NULL);
        g_free (current);
    }

	uri = g_strdup_printf("file://%s", absolute);
	g_free(absolute);

    return uri;
}

static void update_sensitivity()
{
	int index = vspage_get_page_index(vspage);
	g_list_foreach(back_actions, (GFunc)gtk_action_set_sensitive, (gpointer)(index!=0));
	g_list_foreach(forward_actions, (GFunc)gtk_action_set_sensitive, (gpointer)(index!=n_pages-1));
}

static gboolean page_change(int index)
{
	PopplerPage *page = vspage_get_page(vspage);
	if(POPPLER_IS_PAGE(page) && index == poppler_page_get_index(page))
		return FALSE;

	if(index < 0 || index >= n_pages)
		return FALSE;

	page = poppler_document_get_page(document, index);
	if(!page)
		return FALSE;

	vspage_set_page(vspage, page);

	if(spin && scale){
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), index+1);
		gtk_range_set_value(GTK_RANGE(scale), index+1);
	}

	if(sidebar){
		g_signal_emit_by_name(sidebar, "page-changed", index);
	}

	update_sensitivity();

	return TRUE;
}

static struct {
	GtkWidget *entry;
	GtkWidget *progress;
	int index;
	int id;
	GList *list;
	PopplerRectangle last;
} find;

static gboolean find_step(gpointer data)
{
	PopplerPage *page = poppler_document_get_page(document, find.index);
	int n_pages = poppler_document_get_n_pages(document);

	if(!page)
		return FALSE;	

	if(!find.list){
		const char *text = gtk_entry_get_text(GTK_ENTRY(find.entry));
		find.list = poppler_page_find_text(page, text);
	}
	
	gdouble h;
	poppler_page_get_size(page, NULL, &h);
	while(find.list){
		PopplerRectangle *r = find.list->data;
		PopplerRectangle resized = (PopplerRectangle){r->x1 + 0.5, h - r->y2 + 0.5, 
													  r->x2 - 0.5, h - r->y1 - 0.5};
		if(rects_equal(&resized, &find.last)){
			find.list = g_list_next(find.list);
			continue;
		}
		find.last = resized;

		//set to vspage
		if(find.index != vspage_get_page_index(vspage))
			page_change(find.index);
		vspage_set_selection(vspage, &find.last);

		break;
	}
	
	gtk_progress_set_value(GTK_PROGRESS(find.progress), find.index);
	find.index++;
		
	return find.list == NULL && find.index < n_pages;
}

static void find_start()
{
	if(strlen(gtk_entry_get_text(GTK_ENTRY(find.entry))) == 0)
	   return;
	find.index = vspage_get_page_index(vspage);
	find.id = g_idle_add(find_step, NULL);
}

static void find_dialog_cb(GtkWidget *dialog, int response, gpointer data)
{
	if(response == GTK_RESPONSE_ACCEPT){
		find_start();
		return;
	}

	gtk_object_destroy(GTK_OBJECT(dialog));
	if(find.id)
		g_source_remove(find.id);
}

static void find_show_dialog()
{
	GtkDialog* dialog = (GtkDialog*)
		gtk_dialog_new_with_buttons("Find text", window, GTK_DIALOG_MODAL|GTK_DIALOG_NO_SEPARATOR,
									GTK_STOCK_FIND, GTK_RESPONSE_ACCEPT,
									NULL);
	int n_pages = poppler_document_get_n_pages(document);
	
	//message with entry
	GtkWidget *label = gtk_label_new("Text to find:");
	GtkWidget *entry = gtk_entry_new();
	GtkWidget *hbox = gtk_hbox_new(0, 1);
	gtk_container_add(GTK_CONTAINER(hbox), label);
	gtk_container_add(GTK_CONTAINER(hbox), entry);
	gtk_container_add(GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), hbox);

	//progress
	GtkWidget *progress = gtk_progress_bar_new();
	gtk_progress_configure(GTK_PROGRESS(progress), vspage_get_page_index(vspage), 0, n_pages-1);
	gtk_container_add(GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), progress);
	g_signal_connect(dialog, "response", (GCallback)find_dialog_cb, NULL);
	g_signal_connect(entry, "activate", (GCallback)find_start, NULL);

	//reset find params
	find.list = NULL;
	find.entry = entry;
	find.progress = progress;
	bzero(&find.last, sizeof(find.last));
			
	gtk_widget_show_all(GTK_WIDGET(dialog));
}

void move(GtkAction *action, gpointer user_data)
{
	const char *name = gtk_action_get_name(action);

	if(strcmp(name, "GoToPreviousPage") == 0){
		page_change(vspage_get_page_index(vspage)-1);
	}else if(strcmp(name, "GoToNextPage") == 0){
		page_change(vspage_get_page_index(vspage)+1);
	}else if(strcmp(name, "GoToLastPage") == 0){
		page_change(n_pages-1);
	}else if(strcmp(name, "GoToFirstPage") == 0){
		page_change(0);
	}
}

static void set_zoom_mode(GtkRadioAction *action, GtkRadioAction *current, gpointer data)
{
	ZoomMode new_mode = gtk_radio_action_get_current_value(action);
	vspage_set_zoom_mode(vspage, new_mode);
}

static void set_sidebar_mode(GtkRadioAction *action, GtkRadioAction *current, gpointer data)
{
	SidebarMode new_mode = gtk_radio_action_get_current_value(action);
	sidebar_set_mode(sidebar, new_mode);
}

static const GtkActionEntry actions[] = {
	{ "GoToFirstPage", "gtk-goto-first", NULL, 
	  "<control>Home", "Go to first page", G_CALLBACK(move) },
	{ "GoToPreviousPage", "gtk-go-back", NULL, 
	  "<control>Page_Up", "Go to previous page", G_CALLBACK(move) },
	{ "GoToNextPage", "gtk-go-forward", NULL, 
	  "<control>Page_Down", "Go to next page", G_CALLBACK(move) },
	{ "GoToLastPage", "gtk-goto-last", NULL, 
	  "<control>End", "Go to last page", G_CALLBACK(move) },
	{ "Find", "gtk-find", NULL, 
	  "<control>F", "Find text in document", find_show_dialog },
};

static const GtkRadioActionEntry zoom_mode_actions[] = {
	{"ZoomToPage", GTK_STOCK_ZOOM_FIT, NULL, NULL,
	 "Zoom to fit page.", ZOOM_TO_PAGE},
	{"ZoomToWidth", STOCK_ZOOM_TO_WIDTH, NULL, NULL,
	 "Zoom to fit page width.", ZOOM_TO_WIDTH},
};

static const GtkRadioActionEntry sidebar_mode_actions[] = {
	{"SidebarModeIndex", STOCK_INDEX, NULL, NULL,
	 "Show document index.", SIDEBAR_MODE_INDEX},
	{"SidebarModeThumb", STOCK_THUMBNAILS, NULL, NULL,
	 "Show document thumbs.", SIDEBAR_MODE_THUMB},
	{"SidebarModeFind", GTK_STOCK_FIND, NULL, NULL,
	 "Seidebar find pane.", SIDEBAR_MODE_FIND},
};

static const char *ui_xml = 
	"<ui>"
	"  <toolbar name=\"Toolbar\">"
	"    <toolitem action=\"GoToFirstPage\"/>"
	"    <toolitem action=\"GoToPreviousPage\"/>"
	"    <toolitem action=\"GoToNextPage\"/>"
	"    <toolitem action=\"GoToLastPage\"/>"
	"    <separator/>"
	"    <toolitem action=\"Find\"/>"
	"    <separator/>"
	"    <toolitem action=\"SidebarModeIndex\"/>"
	"    <toolitem action=\"SidebarModeThumb\"/>"
	"    <toolitem action=\"SidebarModeFind\"/>"
	"    <separator/>"
	"    <toolitem action=\"ZoomToPage\"/>"
	"    <toolitem action=\"ZoomToWidth\"/>"
	"  </toolbar>"
	"  <accelerator action=\"Find\"/>"
	"  <accelerator action=\"GoToFirstPage\"/>"
	"  <accelerator action=\"GoToPreviousPage\"/>"
	"  <accelerator action=\"GoToNextPage\"/>"
	"  <accelerator action=\"GoToLastPage\"/>"
	"</ui>";

static void load_document(char *fname)
{
	gchar* uri = get_uri(fname);
	document = poppler_document_new_from_file(uri, NULL, NULL);
	g_free(uri);
	
	//init some variables
	pdfname = g_strdup(g_basename(fname));
	n_pages = poppler_document_get_n_pages(document);		

	//set initial page
	page_change(0);
}

static void position_spin_changed(GtkSpinButton *spin, gpointer data)
{
	GtkRange *scale = data;
	int new_val = gtk_spin_button_get_value(spin);

	if(page_change(new_val - 1))
		gtk_range_set_value(scale, new_val);
}

static void position_scale_changed(GtkRange *range, gpointer  spin)
{
	int new_val = gtk_range_get_value(range);
	if(page_change(new_val - 1))
		gtk_spin_button_set_value(spin, new_val);
}

static GtkWidget* position_create()
{
	GtkWidget *hbox = gtk_hbox_new(FALSE, 2);
	spin = gtk_spin_button_new_with_range(1, n_pages, 1);
	scale = gtk_hscale_new_with_range(1, n_pages, 1);
	char *label_str = g_strdup_printf("of %d", n_pages);
	GtkWidget *label = gtk_label_new(label_str);
	g_free(label_str);

	GdkScreen *screen = gdk_screen_get_default();
	gdouble dpi = gdk_screen_get_resolution (screen);
	
	gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
	gtk_widget_set_size_request(scale, dpi * 1.5, -1);

	gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), scale, TRUE, TRUE, 0);

	g_signal_connect(spin, "value-changed", G_CALLBACK(position_spin_changed), scale);
	g_signal_connect(scale, "value-changed", G_CALLBACK(position_scale_changed), spin);

	return hbox;
}

static void on_page_selected(gpointer *instance, int page_num, PopplerRectangle *rect, gpointer data)
{
	page_change(page_num);
	if(rect)
		vspage_set_selection(vspage, rect);
}

static GtkWidget* create_toolbar()
{
	//create action group 
	GtkActionGroup *action_group = gtk_action_group_new ("VSPdf");
	gtk_action_group_add_actions(action_group, actions, G_N_ELEMENTS(actions), 0);
	gtk_action_group_add_radio_actions(action_group, zoom_mode_actions, G_N_ELEMENTS(zoom_mode_actions), 
									   ZOOM_TO_PAGE, G_CALLBACK(set_zoom_mode), NULL);
	gtk_action_group_add_radio_actions(action_group, sidebar_mode_actions, G_N_ELEMENTS(sidebar_mode_actions), 
									   sidebar_have_index(sidebar) ? SIDEBAR_MODE_INDEX : SIDEBAR_MODE_THUMB,
									   G_CALLBACK(set_sidebar_mode), NULL);

	//create ui manager
	GError *err = NULL;
	ui_manager = gtk_ui_manager_new();
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	int merge_id = gtk_ui_manager_add_ui_from_string(ui_manager, ui_xml, -1, NULL);
	if(merge_id == 0){
        g_critical ("Error building UI manager: %s\n", err->message);
		g_error_free (err);
		return NULL;
	}
	gtk_window_add_accel_group (window, gtk_ui_manager_get_accel_group (ui_manager));

	//create toolbar
	GtkWidget *toolbar = gtk_ui_manager_get_widget(ui_manager, "/Toolbar");
	GtkWidget *position = position_create();
	GtkToolItem *item = gtk_tool_item_new();
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);
	gtk_container_add(GTK_CONTAINER(item), position);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
	
	GtkAction *action = gtk_ui_manager_get_action(ui_manager, "/Toolbar/SidebarModeIndex");
	gtk_action_set_sensitive(action, sidebar_have_index(sidebar));

	//init sensisitvity lists
	back_actions = g_list_append(back_actions, 
								 gtk_ui_manager_get_action(ui_manager, "/Toolbar/GoToFirstPage"));
	back_actions = g_list_append(back_actions, 
								 gtk_ui_manager_get_action (ui_manager, "/Toolbar/GoToPreviousPage"));
	forward_actions = g_list_append(forward_actions, 
									gtk_ui_manager_get_action (ui_manager, "/Toolbar/GoToNextPage"));
	forward_actions = g_list_append(forward_actions, 
									gtk_ui_manager_get_action (ui_manager, "/Toolbar/GoToLastPage"));
	update_sensitivity();

	return toolbar;
}

int main(int argc, char *argv[])
{
	gtk_init(&argc, &argv);

	if(argc < 2){
		static char *m_argv[] = {"vspdf", "/home/igor/ch01.pdf"};
		argv = m_argv;
	}

	stock_init();

	//create inital widgets, that don't needs document
	window = (GtkWindow*)gtk_window_new(GTK_WINDOW_TOPLEVEL);
	vspage = g_object_new(TYPE_VSPAGE, NULL);
	if(!window || !vspage)
		return 1;

	//load document
	load_document(argv[1]);
	if(!document)
		return 1;

	//hpaned
	sidebar = (Sidebar*)sidebar_new(document);
	GtkWidget *pan = gtk_hpaned_new();
	gtk_paned_pack2(GTK_PANED(pan), GTK_WIDGET(vspage), TRUE, TRUE);
	gtk_paned_pack1(GTK_PANED(pan), GTK_WIDGET(sidebar), FALSE, TRUE);
	g_signal_connect(sidebar, "page-selected", G_CALLBACK(on_page_selected), NULL);
	
	GtkWidget *toolbar = create_toolbar();
	if(!toolbar)
		return 1;
	
	//main window
	GtkWidget *vbox = gtk_vbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), pan, TRUE, TRUE, 0);
	gtk_window_set_title(window, pdfname);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_window_set_position(window, GTK_WIN_POS_CENTER);

	//init window size
	GdkScreen *screen = gdk_screen_get_default();
	int height = gdk_screen_get_height(screen);
	int width = height * 0.2 + height / 4.0 * 3.0;
	gtk_widget_set_size_request(GTK_WIDGET(sidebar), height*0.2, -1);
	gtk_window_set_default_size(window, width, height);

	//connect signals
	g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

	//run
	gtk_widget_show_all(GTK_WIDGET(window));
	gtk_main();

	g_object_unref(ui_manager);
	g_object_unref(document);
	g_free(pdfname);

	return 0;
}
