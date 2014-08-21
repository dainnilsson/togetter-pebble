#include "pebble.h"

enum {
  MSG_KEY_HEADER = 0x0,
  MSG_KEY_ITEMS = 0x1,
};

typedef struct ListItem {
  uint8_t offset;
  uint8_t amount;
  uint8_t collected;
} ListItem;

static Window *window;
static MenuLayer *menu_layer;
static GBitmap *icon_checked, *icon_unchecked;

static char* header = "ToGetter - loading...      ";  // Allocate 16 bytes for name
static uint8_t num_items = 0;
static ListItem *items;
static char* names;

// A callback is used to specify the number of rows
static uint16_t menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  return num_items;
}

// A callback is used to specify the height of the section header
static int16_t menu_get_header_height_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  // This is a define provided in pebble.h that you may use for the default height
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

// A callback is used to specify the height of a cell
static int16_t menu_get_cell_height_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  return 44;
}

// Here we draw what each header is
static void menu_draw_header_callback(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data) {
  // Draw title text in the section header
  menu_cell_basic_header_draw(ctx, cell_layer, header);
}

// This is the menu item draw callback where you specify what each item should look like
static void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  // TODO: Show amount.
  // TODO: Decrease font and show more rows on screen.
  // TODO: Make icons smaller.
  ListItem* item = &items[cell_index->row];
  GBitmap* icon = item->collected == 0 ? icon_unchecked : icon_checked;
  menu_cell_basic_draw(ctx, cell_layer, names + item->offset, NULL, icon);
}

// Here we capture when a user selects a menu item
void menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  // TODO: Update server
  ListItem* item = &items[cell_index->row];
  item->collected = !item->collected;
  
  if(item->collected != 0) {
    menu_layer_set_selected_next(menu_layer, false, MenuRowAlignCenter, true);
  }
  
  layer_mark_dirty(menu_layer_get_layer(menu_layer));
}


// Messaging
static void in_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *header_tuple = dict_find(iter, MSG_KEY_HEADER);
  Tuple *items_tuple = dict_find(iter, MSG_KEY_ITEMS);

  if (header_tuple) {
    // Keep "ToGetter - "
    strncpy(header + 11, header_tuple->value->cstring, 16);
  }
  if (items_tuple) {
    // data format: num_items | items | names
    num_items = items_tuple->value->data[0];
    free(items);
    items = malloc(items_tuple->length - 1);
    memcpy(items, items_tuple->value->data + 1, items_tuple->length - 1);
    names = (char*)(items + num_items);
  }
  
  menu_layer_reload_data(menu_layer);
  layer_mark_dirty(menu_layer_get_layer(menu_layer));
}

static void in_dropped_handler(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Dropped!");
}

static void app_message_init(void) {
  // Register message handlers
  app_message_register_inbox_received(in_received_handler);
  app_message_register_inbox_dropped(in_dropped_handler);
  // Init buffers
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}

// This initializes the menu upon window load
void window_load(Window *window) {
  // Load icons
  icon_checked = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MENU_ICON_CHECKED);
  icon_unchecked = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MENU_ICON_UNCHECKED);
  
  // Setup items
  items = malloc(num_items * sizeof(ListItem));

  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);
  menu_layer = menu_layer_create(bounds);

  // Set all the callbacks for the menu layer
  menu_layer_set_callbacks(menu_layer, NULL, (MenuLayerCallbacks){
    .get_num_rows = menu_get_num_rows_callback,
    .get_header_height = menu_get_header_height_callback,
//    .get_cell_height = menu_get_cell_height_callback,
    .draw_header = menu_draw_header_callback,
    .draw_row = menu_draw_row_callback,
    .select_click = menu_select_callback,
  });

  // Bind the menu layer's click config provider to the window for interactivity
  menu_layer_set_click_config_onto_window(menu_layer, window);

  // Add it to the window for display
  layer_add_child(window_layer, menu_layer_get_layer(menu_layer));
  
  // Start messaging
  app_message_init();
}

void window_unload(Window *window) {
  // Destroy the menu layer
  menu_layer_destroy(menu_layer);
  
  //Free items
  free(items);
}

int main(void) {
  window = window_create();

  // Setup the window handlers
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  window_stack_push(window, true /* Animated */);

  app_event_loop();

  window_destroy(window);
}