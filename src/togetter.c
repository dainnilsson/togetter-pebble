#include "pebble.h"

#define COLLECTED_MASK 0x80

enum {
  MSG_KEY_HEADER = 0,
  MSG_KEY_ITEMS = 1,
  MSG_KEY_SELECT = 2
};

typedef struct ListItem {
  uint8_t offset;
  uint8_t combined;
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
  return num_items == 0 ? 1 : num_items;
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
  if(num_items == 0) {
    menu_cell_basic_draw(ctx, cell_layer, "[EMPTY]", NULL, NULL);
    return;
  }
  ListItem* item = &items[cell_index->row];
  GBitmap* icon = (COLLECTED_MASK & item->combined) == 0 ? icon_unchecked : icon_checked;
  uint8_t amount = ~COLLECTED_MASK & item->combined;
  menu_cell_basic_draw(ctx, cell_layer, names + item->offset, NULL, icon);
}

// Here we capture when a user selects a menu item
void menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  ListItem* item = &items[cell_index->row];
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  if (iter == NULL) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "null iter");
    return;
  }

  Tuplet tuple = TupletInteger(MSG_KEY_SELECT, cell_index->row);
  dict_write_tuplet(iter, &tuple);
  dict_write_end(iter);

  if(item->combined == 0) {
    // List select: Do nothing...
  } else {
    // Item select
    item->combined = item->combined ^ COLLECTED_MASK;

    if((COLLECTED_MASK & item->combined) != 0) {
      menu_layer_set_selected_next(menu_layer, false, MenuRowAlignCenter, true);
    }

    layer_mark_dirty(menu_layer_get_layer(menu_layer));
  }
  
  app_message_outbox_send();
}

//Long click select
void menu_select_long_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  if (iter == NULL) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "null iter");
    return;
  }

  Tuplet tuple = TupletInteger(MSG_KEY_SELECT, -1);
  dict_write_tuplet(iter, &tuple);
  dict_write_end(iter);
  app_message_outbox_send();
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
  menu_layer_set_selected_index(menu_layer, (MenuIndex) { .row = 0, .section = 0 }, MenuRowAlignCenter, true);
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
    .select_long_click = menu_select_long_callback,
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