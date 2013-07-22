/**
 * Pebble Watchface for Rabb Ferenc
 *
 * Written by Matthew Tole <pebble@matthewtole.com>
 * Started 23rd July 2013
 *
 * GitHub Repository: https://github.com/smallstoneapps/rabb-ferenc/
*/

#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
#include <math.h>

#define MY_UUID { 0x75, 0x07, 0x9D, 0x5F, 0x24, 0x8C, 0x43, 0x60, 0x9D, 0x6F, 0x82, 0xCC, 0x04, 0x1A, 0xB7, 0x31 }

PBL_APP_INFO(MY_UUID, "Rabb Ferenc", "Small Stone Apps", 1, 0,  DEFAULT_MENU_ICON, APP_INFO_WATCH_FACE);

#define PI 3.14159

#define VIBRATION_SLEEP_START 22
#define VIBRATION_SLEEP_END 5

#define COLOR_FOREGROUND GColorWhite
#define COLOR_BACKGROUND GColorBlack

#define ICON_VIBE_ON 0
#define ICON_VIBE_OFF 1

#define FONT_ANALOG_NUMBERS 0
#define FONT_DIGITAL 1
#define FONT_DATE 2
#define FONT_COUNTDOWN 3

#define ANALOG_RADIUS 32
#define ANALOG_OFFSET 12
#define ANALOG_LENGTH_SECOND 27
#define ANALOG_LENGTH_MINUTE 21
#define ANALOG_LENGTH_HOUR 15
#define ANALOG_WIDTH_HOUR 3
#define ANALOG_WIDTH_MINUTE 3
#define ANALOG_INNER_RADIUS 3

#define COUNTDOWN_MINUTE 50

const char *date_days[] = { "Vasárnap", "Hétfő", "Kedd", "Szerda", "Csütörtök", "Péntek", "Szombat"  };
const char *date_months[] = { "Jan", "Febr", "Márc", "Ápr", "Máj", "Jún", "Júl", "Aug", "Szept", "Okt", "Nov", "Dec" };

void handle_init(AppContextRef ctx);
void handle_deinit(AppContextRef ctx);
void handle_tick(AppContextRef ctx, PebbleTickEvent *t);
void handle_window_appear(Window* me);
void load_bitmaps();
void load_fonts();
void unload_bitmaps();

void create_digital_clock();
void create_analog_clock();
void create_countdown();
void create_date();

void update_analog_clock(PblTm* now);
void update_digital_clock(PblTm* now);
void update_countdown(PblTm* now);
void update_date(PblTm* now);

void do_vibration(PblTm* now);
bool sleep_time(PblTm* now);

void analog_clock_update_background(Layer* me, GContext* ctx);
void analog_clock_update_hour(Layer* me, GContext* ctx);
void analog_clock_update_minute(Layer* me, GContext* ctx);
void analog_clock_update_second(Layer* me, GContext* ctx);
void countdown_update(Layer* me, GContext* ctx);

double degtorad(double deg);
double angle_from_minute(int min);

Window window;
TextLayer layer_clock_digital;
TextLayer layer_date;
Layer layer_clock_analog_background;
Layer layer_clock_analog_hour;
Layer layer_clock_analog_minute;
Layer layer_clock_analog_second;
Layer layer_countdown;
HeapBitmap icons[2];
GFont fonts[4];

PblTm* clock_time;

void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
    .deinit_handler = &handle_deinit,
    .tick_info = {
      .tick_handler = &handle_tick,
      .tick_units = SECOND_UNIT
    }
  };
  app_event_loop(params, &handlers);
}

void handle_init(AppContextRef ctx) {
  resource_init_current_app(&APP_RESOURCES);

  load_bitmaps();
  load_fonts();

  window_init(&window, "Watchface Window");
  window_stack_push(&window, true);
  window_set_background_color(&window, COLOR_BACKGROUND);
  window_set_window_handlers(&window, (WindowHandlers) {
    .appear = handle_window_appear
  });

  create_digital_clock();
  create_analog_clock();
  create_countdown();
  create_date();
}

void handle_deinit(AppContextRef ctx) {
  unload_bitmaps();
}

void handle_tick(AppContextRef ctx, PebbleTickEvent *t) {
  // Analog clock updates every second.
  update_analog_clock(t->tick_time);
  if (t->tick_time->tm_sec == 0) {
    // Digitial clock updates once a minute.
    update_digital_clock(t->tick_time);
    // Vibration countdown updates once a minute.
    update_countdown(t->tick_time);
    if (t->tick_time->tm_min == COUNTDOWN_MINUTE) {
      // Vibration happens at 50 minutes past the hour.
      do_vibration(t->tick_time);
    }
    else if (t->tick_time->tm_min == 0) {
      if (t->tick_time->tm_hour == 0) {
        // Date updates once a day.
        update_date(t->tick_time);
      }
    }
  }
}

void handle_window_appear(Window* me) {
  PblTm now;
  get_time(&now);
  update_analog_clock(&now);
  update_digital_clock(&now);
  update_countdown(&now);
  update_date(&now);
}

void load_bitmaps() {
  heap_bitmap_init(&icons[ICON_VIBE_ON], RESOURCE_ID_ICON_VIBE_ON);
  heap_bitmap_init(&icons[ICON_VIBE_OFF], RESOURCE_ID_ICON_VIBE_OFF);
}

void unload_bitmaps() {
  heap_bitmap_deinit(&icons[ICON_VIBE_ON]);
  heap_bitmap_deinit(&icons[ICON_VIBE_OFF]);
}

void load_fonts() {
  fonts[FONT_ANALOG_NUMBERS] = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ANALOG_10));
  fonts[FONT_DIGITAL] = fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
  fonts[FONT_DATE] = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  fonts[FONT_COUNTDOWN] =  fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
}

void create_digital_clock() {
  text_layer_init(&layer_clock_digital, GRect(0, 4, 144, 42));
  text_layer_set_text_color(&layer_clock_digital, COLOR_FOREGROUND);
  text_layer_set_background_color(&layer_clock_digital, GColorClear);
  text_layer_set_font(&layer_clock_digital, fonts[FONT_DIGITAL]);
  text_layer_set_text_alignment(&layer_clock_digital, GTextAlignmentCenter);
  layer_add_child(&window.layer, &layer_clock_digital.layer);
}

void create_analog_clock() {
  layer_init(&layer_clock_analog_background, GRect(0, 82, 86, 86));
  layer_clock_analog_background.update_proc = &analog_clock_update_background;
  layer_add_child(&window.layer, &layer_clock_analog_background);

  layer_init(&layer_clock_analog_hour, GRect(0, 82, 86, 86));
  layer_clock_analog_hour.update_proc = &analog_clock_update_hour;
  layer_add_child(&window.layer, &layer_clock_analog_hour);

  layer_init(&layer_clock_analog_minute, GRect(0, 82, 86, 86));
  layer_clock_analog_minute.update_proc = &analog_clock_update_minute;
  layer_add_child(&window.layer, &layer_clock_analog_minute);

  layer_init(&layer_clock_analog_second, GRect(0, 82, 86, 86));
  layer_clock_analog_second.update_proc = &analog_clock_update_second;
  layer_add_child(&window.layer, &layer_clock_analog_second);
}

void create_date() {
  text_layer_init(&layer_date, GRect(0, 54, 144, 22));
  text_layer_set_text_color(&layer_date, COLOR_FOREGROUND);
  text_layer_set_background_color(&layer_date, GColorClear);
  text_layer_set_font(&layer_date, fonts[FONT_DATE]);
  text_layer_set_text_alignment(&layer_date, GTextAlignmentCenter);
  layer_add_child(&window.layer, &layer_date.layer);
}

void create_countdown() {
  layer_init(&layer_countdown, GRect(90, 86, 50, 86));
  layer_countdown.update_proc = &countdown_update;
  layer_add_child(&window.layer, &layer_countdown);
}

void update_digital_clock(PblTm* now) {
  static char time_str[10];
  string_format_time(time_str, sizeof(time_str), "%H:%M", now);
  clock_time = now;
  text_layer_set_text(&layer_clock_digital, time_str);
}

void update_analog_clock(PblTm* now) {
  layer_mark_dirty(&layer_clock_analog_second);
  if (now->tm_sec == 0) {
    layer_mark_dirty(&layer_clock_analog_minute);
    if (now->tm_min % 12 == 0) {
      layer_mark_dirty(&layer_clock_analog_hour);
    }
  }
}

void update_countdown(PblTm* now) {
  layer_mark_dirty(&layer_countdown);
}

void update_date(PblTm* now) {
  static char date_str[50];
  snprintf(date_str, sizeof(date_str), "%s %d. %s", date_months[now->tm_mon], now->tm_mday, date_days[now->tm_wday]);
  text_layer_set_text(&layer_date, date_str);
}

void do_vibration(PblTm* now) {
  if (sleep_time(now)) {
    return;
  }
  static const uint32_t const vibe_segments[] = { 700, 100, 700, 100, 700 };
  VibePattern vibe_pattern = {
    .durations = vibe_segments,
    .num_segments = ARRAY_LENGTH(vibe_segments),
  };
  vibes_enqueue_custom_pattern(vibe_pattern);
}

void analog_clock_update_background(Layer* me, GContext* ctx) {
  graphics_context_set_fill_color(ctx, COLOR_FOREGROUND);
  graphics_context_set_stroke_color(ctx, COLOR_FOREGROUND);

  GPoint center = { ANALOG_OFFSET + ANALOG_RADIUS, ANALOG_OFFSET + ANALOG_RADIUS };

  GRect tmp;
  tmp.origin = (GPoint){ ANALOG_RADIUS + ANALOG_OFFSET - 20, 0 };
  tmp.size = (GSize){ 40, 10 };
  graphics_text_draw(ctx, "12", fonts[FONT_ANALOG_NUMBERS], tmp, 0, GTextAlignmentCenter, NULL);

  tmp.origin = (GPoint){ ANALOG_RADIUS * 2 + ANALOG_OFFSET + 3, ANALOG_RADIUS + ANALOG_OFFSET - 7 };
  tmp.size = (GSize){ 40, 10 };
  graphics_text_draw(ctx, "3", fonts[FONT_ANALOG_NUMBERS], tmp, 0, GTextAlignmentLeft, NULL);

  tmp.origin = (GPoint){ ANALOG_RADIUS + ANALOG_OFFSET - 20, ANALOG_RADIUS * 2 + ANALOG_OFFSET - 1};
  tmp.size = (GSize){ 40, 10 };
  graphics_text_draw(ctx, "6", fonts[FONT_ANALOG_NUMBERS], tmp, 0, GTextAlignmentCenter, NULL);

  tmp.origin = (GPoint){ 0 + 3, ANALOG_RADIUS + ANALOG_OFFSET - 7 };
  tmp.size = (GSize){ 40, 10 };
  graphics_text_draw(ctx, "9", fonts[FONT_ANALOG_NUMBERS], tmp, 0, GTextAlignmentLeft, NULL);


  for (int m = 0; m < 60; m += 1) {
    double angle = angle_from_minute(m);
    int line_length = 1;
    if (m % 5 == 0) {
      line_length = 3;
    }
    if (m % 15 == 0) {
      line_length = 5;
    }
    GPoint tick_outer = {
      ANALOG_OFFSET + ANALOG_RADIUS + (ANALOG_RADIUS * cos(angle)),
      ANALOG_OFFSET + ANALOG_RADIUS + (ANALOG_RADIUS * sin(angle))
    };
    GPoint tick_inner = {
      ANALOG_OFFSET + ANALOG_RADIUS + ((ANALOG_RADIUS - line_length) * cos(angle)),
      ANALOG_OFFSET + ANALOG_RADIUS + ((ANALOG_RADIUS - line_length) * sin(angle))
    };
    graphics_draw_line(ctx, tick_inner, tick_outer);
  }

  graphics_fill_circle(ctx, center, ANALOG_INNER_RADIUS);
}

void analog_clock_update_hour(Layer* me, GContext* ctx) {
  PblTm now;
  get_time(&now);
  graphics_context_set_fill_color(ctx, COLOR_FOREGROUND);
  graphics_context_set_stroke_color(ctx, COLOR_FOREGROUND);

  int hour_minutes = now.tm_hour * 5 + now.tm_min / 12;

  GPoint center = { ANALOG_OFFSET + ANALOG_RADIUS, ANALOG_OFFSET + ANALOG_RADIUS };
  GPoint outer = {
    ANALOG_OFFSET + ANALOG_RADIUS + (ANALOG_LENGTH_HOUR * cos(angle_from_minute(hour_minutes))),
    ANALOG_OFFSET + ANALOG_RADIUS + (ANALOG_LENGTH_HOUR * sin(angle_from_minute(hour_minutes)))
  };

  for (int w = 1; w <= ANALOG_WIDTH_HOUR; w += 1) {
    GPoint inner_1 = {
      ANALOG_OFFSET + ANALOG_RADIUS + (w * cos(angle_from_minute(hour_minutes - 15))),
      ANALOG_OFFSET + ANALOG_RADIUS + (w * sin(angle_from_minute(hour_minutes - 15)))
    };
    GPoint inner_2 = {
      ANALOG_OFFSET + ANALOG_RADIUS + (w * cos(angle_from_minute(hour_minutes + 15))),
      ANALOG_OFFSET + ANALOG_RADIUS + (w * sin(angle_from_minute(hour_minutes + 15)))
    };
    graphics_draw_line(ctx, inner_1, outer);
    graphics_draw_line(ctx, inner_2, outer);
    graphics_draw_line(ctx, center, outer);
  }
}

void analog_clock_update_minute(Layer* me, GContext* ctx) {
  PblTm now;
  get_time(&now);
  graphics_context_set_fill_color(ctx, COLOR_FOREGROUND);
  graphics_context_set_stroke_color(ctx, COLOR_FOREGROUND);

  GPoint center = { ANALOG_OFFSET + ANALOG_RADIUS, ANALOG_OFFSET + ANALOG_RADIUS };
  GPoint outer = {
    ANALOG_OFFSET + ANALOG_RADIUS + (ANALOG_LENGTH_MINUTE * cos(angle_from_minute(now.tm_min))),
    ANALOG_OFFSET + ANALOG_RADIUS + (ANALOG_LENGTH_MINUTE * sin(angle_from_minute(now.tm_min)))
  };
  for (int w = 1; w <= ANALOG_WIDTH_MINUTE; w += 1) {
    GPoint inner_1 = {
      ANALOG_OFFSET + ANALOG_RADIUS + (w * cos(angle_from_minute(now.tm_min - 15))),
      ANALOG_OFFSET + ANALOG_RADIUS + (w * sin(angle_from_minute(now.tm_min - 15)))
    };
    GPoint inner_2 = {
      ANALOG_OFFSET + ANALOG_RADIUS + (w * cos(angle_from_minute(now.tm_min + 15))),
      ANALOG_OFFSET + ANALOG_RADIUS + (w * sin(angle_from_minute(now.tm_min + 15)))
    };
    graphics_draw_line(ctx, inner_1, outer);
    graphics_draw_line(ctx, inner_2, outer);
    graphics_draw_line(ctx, center, outer);
  }
}

void analog_clock_update_second(Layer* me, GContext* ctx) {
  PblTm now;
  get_time(&now);
  graphics_context_set_fill_color(ctx, COLOR_FOREGROUND);
  graphics_context_set_stroke_color(ctx, COLOR_FOREGROUND);

  GPoint center = { ANALOG_OFFSET + ANALOG_RADIUS, ANALOG_OFFSET + ANALOG_RADIUS };

  GPoint second_outer = {
    ANALOG_OFFSET + ANALOG_RADIUS + (ANALOG_LENGTH_SECOND * cos(angle_from_minute(now.tm_sec))),
    ANALOG_OFFSET + ANALOG_RADIUS + (ANALOG_LENGTH_SECOND * sin(angle_from_minute(now.tm_sec)))
  };
  graphics_draw_line(ctx, second_outer, center);

}

void countdown_update(Layer* me, GContext* ctx) {
  PblTm now;
  get_time(&now);

  GRect icon_rect;
  icon_rect.size = (GSize){ 28, 28 };
  icon_rect.origin = (GPoint){ 11, 12 };

  GRect text_rect;
  text_rect.origin = (GPoint){ 0, 36 };
  text_rect.size = (GSize){ 50, 28 };

  static char countdown_str[5] = "--";

  if (! sleep_time(&now)) {
    int minutes_to_vibrate = 0;
    if (now.tm_min <= COUNTDOWN_MINUTE) {
      minutes_to_vibrate = COUNTDOWN_MINUTE - now.tm_min;
    }
    else {
      minutes_to_vibrate = 60 - (now.tm_min - COUNTDOWN_MINUTE);
    }
    snprintf(countdown_str, sizeof(countdown_str), "%d", minutes_to_vibrate);
    graphics_draw_bitmap_in_rect(ctx, &icons[ICON_VIBE_ON].bmp, icon_rect);
    graphics_text_draw(ctx, countdown_str, fonts[FONT_COUNTDOWN], text_rect, 0, GTextAlignmentCenter, NULL);
  }
  else {
    strncpy(countdown_str, "--", 2);
    graphics_draw_bitmap_in_rect(ctx, &icons[ICON_VIBE_OFF].bmp, icon_rect);
    graphics_text_draw(ctx, countdown_str, fonts[FONT_COUNTDOWN], text_rect, 0, GTextAlignmentCenter, NULL);
  }

}

bool sleep_time(PblTm* now) {
  if (now->tm_hour >= VIBRATION_SLEEP_START) {
    return true;
  }
  if (now->tm_hour == VIBRATION_SLEEP_START - 1 && now->tm_min > COUNTDOWN_MINUTE) {
    return true;
  }
  if (now->tm_hour <= VIBRATION_SLEEP_END) {
    return true;
  }
  return false;
}

double angle_from_minute(int min) {
  return degtorad(-90 + (6 * min));
}

double degtorad(double deg) {
  return (PI * deg / 180.0);
}