#pragma once
typedef void *esp_timer_handle_t;
struct esp_timer_create_args_t { void (*callback)(void *); void *arg; int dispatch_method; const char *name; bool skip_unhandled_events; };
int esp_timer_create(const esp_timer_create_args_t *, esp_timer_handle_t *); int esp_timer_start_once(esp_timer_handle_t, uint64_t);
int esp_timer_start_periodic(esp_timer_handle_t, uint64_t); int esp_timer_stop(esp_timer_handle_t);
