#include <gtk/gtk.h>
#include <stdbool.h>
#include "../common/structures.c"
#include "connector.c"

#define WIDTH 800
#define HEIGHT 480
#define APP_ID "org.drmargarido.MailingListsClient"

// TODO: Make the app behave well and display information when the requests
// asking for data to the server fail. Popup window may be a good solution.
// TODO: Make handlers not block when waiting for the server response to be
// received.

int selected_game;
Connector * connector;
GtkWidget * window;
GtkWidget * notebook;

GtkWidget * emails_list;
GtkWidget * total_label;

GtkWidget * subject_entry;
GtkWidget * email_area;


static void write_email_clicked(GtkWidget *widget, gpointer data){
  gtk_notebook_next_page(GTK_NOTEBOOK(notebook));
}

static void back_clicked(GtkWidget *widget, gpointer data){
  gtk_notebook_prev_page(GTK_NOTEBOOK(notebook));
}

static void destroy_widget(GtkWidget *widget, gpointer data){
  gtk_widget_destroy(widget);
}

static void game_clicked(GtkWidget *widget, gpointer data){
  Game * game = (Game *) data;
  selected_game = game->id;

  EmailsList * list = get_game_emails(connector,  game->id);
  if(list){
    gtk_container_foreach(GTK_CONTAINER(emails_list), destroy_widget, NULL);
    for(int i=0; i < list->total; i++){
      GtkWidget * email_label = gtk_label_new(&list->emails[i * EMAIL_SIZE]);
      gtk_container_add(GTK_CONTAINER(emails_list), email_label);
    }
    char label_text[100];
    snprintf(label_text, 100, "Registered Emails: %d", list->total);
    gtk_label_set_text(GTK_LABEL(total_label), label_text);
    gtk_widget_show_all(window);
  }
}

static void send_email_clicked(GtkWidget * widget, gpointer data){
  char subject[KB];
  strncpy(subject, gtk_entry_get_text(GTK_ENTRY(subject_entry)), KB);

  GtkTextIter start;
  GtkTextIter end;
  GtkTextBuffer * buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(email_area));
  gtk_text_buffer_get_bounds(buffer, &start, &end);
  char * body = gtk_text_buffer_get_text(buffer, &start, &end, false);

  if(strlen(subject) == 0 || strlen(body) == 0){
    // If any element is empty do not send email
    g_free(body);
    return;
  }

  int result = send_email(connector, selected_game, subject, body);
  g_free(body);

  if(result != 0){
    return;
  }

  //TODO: Show popup notification with the result information

  // Clear fields after success
  gtk_entry_set_text(GTK_ENTRY(subject_entry), "");
  gtk_text_buffer_delete(buffer, &start, &end);
}

void init_games_area(GtkWidget * container){
  GtkWidget * games_scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(
    GTK_SCROLLED_WINDOW(games_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC
  );
  gtk_container_add(GTK_CONTAINER(container), games_scroll);

  GtkWidget * games_list = gtk_button_box_new(GTK_ORIENTATION_VERTICAL);
  gtk_button_box_set_layout(GTK_BUTTON_BOX(games_list), GTK_BUTTONBOX_START);
  gtk_container_add(GTK_CONTAINER(games_scroll), games_list);

  GamesList * list = get_games(connector);
  if(list){
    for(int i=0; i < list->total; i++){
      GtkWidget * game_button = gtk_button_new_with_label(list->games[i].name);
      gtk_container_add(GTK_CONTAINER(games_list), game_button);
      g_signal_connect(game_button, "clicked", G_CALLBACK(game_clicked), &list->games[i]);
    }
  }
}

void init_emails_area(GtkWidget * container){
  GtkWidget * area = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start(GTK_BOX(container), area, true, true, 0);

  GtkWidget * emails_scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(
    GTK_SCROLLED_WINDOW(emails_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC
  );
  gtk_box_pack_start(GTK_BOX(area), emails_scroll, true, true, 0);

  emails_list = gtk_list_box_new();
  gtk_container_add(GTK_CONTAINER(emails_scroll), emails_list);

  GtkWidget * bottom_row = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_button_box_set_layout(GTK_BUTTON_BOX(bottom_row), GTK_BUTTONBOX_SPREAD);
  gtk_box_pack_start(GTK_BOX(area), bottom_row, false, true, 10);

  total_label = gtk_label_new("Registered Emails: 0");
  gtk_container_add(GTK_CONTAINER(bottom_row), total_label);

  GtkWidget * send_email_button = gtk_button_new_with_label("Write Email To All");
  gtk_container_add(GTK_CONTAINER(bottom_row), send_email_button);
  g_signal_connect(send_email_button, "clicked", G_CALLBACK(write_email_clicked), NULL);
}

void init_writing_area(GtkWidget * container){
  GtkWidget * subject_label = gtk_label_new("Subject");
  gtk_widget_set_margin_top(subject_label, 10);
  gtk_container_add(GTK_CONTAINER(container), subject_label);
  subject_entry = gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(container), subject_entry, false, true, 10);

  gtk_container_add(GTK_CONTAINER(container), gtk_label_new("Body"));
  GtkWidget * email_scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(
    GTK_SCROLLED_WINDOW(email_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC
  );
  gtk_box_pack_start(GTK_BOX(container), email_scroll, true, true, 0);
  email_area = gtk_text_view_new();
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(email_area), GTK_WRAP_WORD_CHAR);
  gtk_container_add(GTK_CONTAINER(email_scroll), email_area);

  GtkWidget * bottom_row = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_button_box_set_layout(GTK_BUTTON_BOX(bottom_row), GTK_BUTTONBOX_SPREAD);
  gtk_box_pack_start(GTK_BOX(container), bottom_row, false, true, 10);

  GtkWidget * back_button = gtk_button_new_with_label("Back");
  gtk_container_add(GTK_CONTAINER(bottom_row), back_button);
  g_signal_connect(back_button, "clicked", G_CALLBACK(back_clicked), NULL);

  GtkWidget * send_button = gtk_button_new_with_label("Send");
  gtk_container_add(GTK_CONTAINER(bottom_row), send_button);
  g_signal_connect(send_button, "clicked", G_CALLBACK(send_email_clicked), NULL);
}

static void activate(GtkApplication* app, gpointer user_data){
  window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW (window), "Mailing Lists");
  gtk_window_set_default_size(GTK_WINDOW (window), WIDTH, HEIGHT);

  notebook = gtk_notebook_new();
  gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), false);
  gtk_container_add(GTK_CONTAINER(window), notebook);

  GtkWidget * main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  init_games_area(main_box);
  init_emails_area(main_box);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), main_box, NULL);

  GtkWidget * email_writing_area = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  init_writing_area(email_writing_area);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), email_writing_area, NULL);

  gtk_widget_show_all(window);
}

int start_gui(Connector * conn){
  connector = conn;
  selected_game = 0;

  GtkApplication *app = gtk_application_new(APP_ID, G_APPLICATION_FLAGS_NONE);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

  int status = g_application_run(G_APPLICATION(app), 0, NULL);
  g_object_unref(app);

  return status;
}
