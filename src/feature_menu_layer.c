#include "pebble.h"

#define NUM_MENU_SECTIONS 1
#define NUM_MENU_ITEMS 3

enum {
  MSG_KEY_HEADER = 0x0,
  MSG_KEY_ITEMS = 0x1,
};

typedef struct ListItem {
  char name[16];
  uint8_t amount;
  uint8_t collected;
} ListItem;

static char* header = "ToGetter - loading...      ";
static uint8_t num_items = 0;
static ListItem *items;

static Window *window;

// This is a menu layer
// You have more control than with a simple menu layer
static MenuLayer *menu_layer;
static GBitmap *icon_checked, *icon_unchecked;

// A callback is used to specify the amount of sections of menu items
// With this, you can dynamically add and remove sections
static uint16_t menu_get_num_sections_callback(MenuLayer *menu_layer, void *data) {
  return NUM_MENU_SECTIONS;
}

// Each section has a number of items;  we use a callback to specify this
// You can also dynamically add and remove items using this
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
  // This is a define provided in pebble.h that you may use for the default height
  return 44;
}

// Here we draw what each header is
static void menu_draw_header_callback(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data) {
  // Draw title text in the section header
  menu_cell_basic_header_draw(ctx, cell_layer, header);
}

// This is the menu item draw callback where you specify what each item should look like
static void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  // Use the row to specify which item we'll draw
  ListItem* item = &items[cell_index->row];
  GBitmap* icon = item->collected == 0 ? icon_unchecked : icon_checked;
  menu_cell_basic_draw(ctx, cell_layer, item->name, NULL, icon);
}

// Here we capture when a user selects a menu item
void menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  // TODO: Check/Uncheck item
  ListItem* item = &items[cell_index->row];
  item->collected = !item->collected;
  
  if(item->collected != 0) {
    //Select next item
    menu_layer_set_selected_next(menu_layer, false, MenuRowAlignCenter, true);
  }
  
  layer_mark_dirty(menu_layer_get_layer(menu_layer));
}


// Messaging
static void in_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *header_tuple = dict_find(iter, MSG_KEY_HEADER);
  Tuple *items_tuple = dict_find(iter, MSG_KEY_ITEMS);

  if (header_tuple) {
    strncpy(header + 11, header_tuple->value->cstring, 16);
  }
  if (items_tuple) {
    // num_items, (collected, amount, namelen, name)*
    char* data = items_tuple->value->cstring;
    uint8_t offset = 0;
    num_items = data[offset++];
    
    // Allocate new list
    free(items);
    items = malloc(num_items * sizeof(ListItem));
    
    // Parse items
    uint8_t i;
    for(i=0; i < num_items; i++) {
      ListItem* item = &items[i];
      item->collected = data[offset++];
      item->amount = data[offset++];
      uint8_t name_len = data[offset++];
      strncpy(item->name, &data[offset], name_len);
      offset += name_len;
    }
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
  
  // Now we prepare to initialize the menu layer
  // We need the bounds to specify the menu layer's viewport size
  // In this case, it'll be the same as the window's
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);
  
  // Setup items
  items = malloc(num_items * sizeof(ListItem));

  // Create the menu layer
  menu_layer = menu_layer_create(bounds);

  // Set all the callbacks for the menu layer
  menu_layer_set_callbacks(menu_layer, NULL, (MenuLayerCallbacks){
    .get_num_sections = menu_get_num_sections_callback,
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
  
  //Start messaging
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
