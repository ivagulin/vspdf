#ifndef FINDDIALOG_H_
#define FINDDIALOG_H_

#include <gtk/gtkdialog.h>
#include <glib/poppler.h>

#define TYPE_FINDDIALOG (finddialog_get_type ())
#define FINDDIALOG(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_FINDDIALOG, FindDialog))

typedef struct _FindDialog
{
	GtkDialog parent;
	PopplerDocument *document;
	GtkProgress *progress;
	GtkEntry *entry;
	int current_page;
	GList *places;
} FindDialog;


typedef struct _FindDialogClass
{
	GtkDialogClass parent;
} FindDialogClass;

FindDialog* finddialog_new(GtkWindow *parent, PopplerDocument *document, int current_page);
GType finddialog_get_type();

#endif /* FINDDIALOG_H_ */
