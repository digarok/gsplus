extern void debug_init();
extern void debug_poll_only();
extern void debug_cpu_test();

// used in sim65816.c
void debug_server_poll();
int debug_events_waiting();
void debug_handle_event();
