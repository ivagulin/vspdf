#include <gtk/gtk.h>
#include <glib/poppler.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>
#include <sys/time.h>

#include "vslayout.h"
#include "sidebar.h"
#include "finddialog.h"
#include "stock.h"

static GtkWidget* window = NULL;
static PopplerDocument *document = NULL;
static GtkScrolledWindow *sw = NULL;
static VSLayout *vslayout = NULL;

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

static void set_page(gpointer *data, int page_num, PopplerRectangle * rect)
{
	int position = vslayout_get_page_position(vslayout, page_num);
	if(position != -1){
		GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment(sw);
		gtk_adjustment_set_value(adj, position);
	}
}

static void show_find_dialog(void)
{
	FindDialog* fd = finddialog_new(GTK_WINDOW(window), document, 0);
	g_signal_connect(fd, "page-selected", G_CALLBACK(set_page), NULL);
}

static void set_sidebar_mode(GtkRadioAction *action, GtkRadioAction *current, gpointer data)
{
	Sidebar *sidebar = data;
	SidebarMode new_mode = gtk_radio_action_get_current_value(action);
	sidebar_set_mode(sidebar, new_mode);
}

static const GtkActionEntry actions[] = {
	{ "Find", "gtk-find", NULL,
	  "<control>F", "Find text in document", show_find_dialog },
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
	"  <toolbar name='Toolbar'>"
	"    <toolitem action='SidebarModeIndex'/>"
	"    <toolitem action='SidebarModeThumb'/>"
	"    <toolitem action='SidebarModeFind'/>"
	"    <separator/>"
	"  </toolbar>"
	"  <accelerator action='Find'/>"
	"</ui>";

int main(int argc, char *argv[])
{
	GError *err = NULL;
	gtk_init(&argc, &argv);

	if(argc < 2){
		static char *m_argv[] = {"vspdf", "/home/igor/ch01.pdf"};
		argv = m_argv;
	}

	//create window
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	if(!window)
		g_error("gtk_window_new failed");

	//load document
	gchar* uri = get_uri(argv[1]);
	document = poppler_document_new_from_file(uri, NULL, &err);
	if(err != NULL)
		g_error("Document loading failed: %s", err->message);
	g_free(uri);

	//create vslayout
	vslayout = g_object_new(TYPE_VSLAYOUT, "document", document, NULL);
	if(!vslayout)
		g_error("vslayout creation failed");

	//create index
	GtkWidget *sb = sidebar_new(document);
	if(!sb)
		g_error("sidebar creation failed");
	g_signal_connect(sb, "page-selected", G_CALLBACK(set_page), NULL);
	
	stock_init();

	//add accelerators
	GtkActionGroup *action_group = gtk_action_group_new ("vspdf");
	gtk_action_group_add_actions(action_group, actions, G_N_ELEMENTS(actions), 0);
	gtk_action_group_add_radio_actions(action_group, sidebar_mode_actions,
			G_N_ELEMENTS(sidebar_mode_actions),
			sidebar_have_index(SIDEBAR(sb)) ? SIDEBAR_MODE_INDEX : SIDEBAR_MODE_THUMB,
					   G_CALLBACK(set_sidebar_mode), sb);
	GtkUIManager *ui_manager = gtk_ui_manager_new();
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	gtk_ui_manager_add_ui_from_string(ui_manager, ui_xml, -1, &err);
	if(err != NULL)
		g_error("gtk_ui_manager_add_ui_from_string failed: %s", err->message);
	gtk_window_add_accel_group (GTK_WINDOW(window), gtk_ui_manager_get_accel_group (ui_manager));

	//pack sw
	sw = (GtkScrolledWindow*)gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(sw, GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_add_with_viewport(sw, GTK_WIDGET(vslayout));

	//pack hpaned
	const int DEFAULT_SIDEBAR_WIDTH = 200;
	GtkWidget *hpaned = gtk_hpaned_new();
	gtk_paned_add1(GTK_PANED(hpaned), sb);
	gtk_paned_add2(GTK_PANED(hpaned), GTK_WIDGET(sw));
	gtk_paned_set_position(GTK_PANED(hpaned), DEFAULT_SIDEBAR_WIDTH);
	
	//create vbox for toolbar/mains-screen/status
	GtkWidget *vbox = gtk_vbox_new(0, 2);
	gtk_box_pack_start(GTK_BOX(vbox), gtk_ui_manager_get_widget(ui_manager, "/Toolbar"), 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hpaned, 1, 1, 0);

	//pack hpaned to window
	gtk_widget_set_usize(GTK_WIDGET(window), 600 + DEFAULT_SIDEBAR_WIDTH, 800);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
	gtk_widget_show_all(GTK_WIDGET(window));
	gtk_main();

	return 0;
}
