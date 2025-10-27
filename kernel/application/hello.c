// Screenfetch-like hello command: print ASCII KOS logo and basic OS info
#include <lib/libc/stdio.h>
#include <lib/libc/stdint.h>
#include "app.h"
#include "app_log.h"

static void print_ascii_logo(void) {
	const char* art[] = {
		"      __  __   ",
		" __ _/ /_/ /__ ",
		"/ _` / __/ / _ ",
		"\\__,_\\__/_/   OS",
	};
	for (unsigned i = 0; i < sizeof(art)/sizeof(art[0]); ++i) {
		kos_puts((const int8_t*)art[i]);
		kos_puts((const int8_t*)"\n");
	}
}

static void print_info_lines(void) {
	const int8_t* cmd = kos_cmdline();
	int32_t ac = kos_argc();
	app_log((const int8_t*)"OS:       %s\n", (const int8_t*)"KOS");
	app_log((const int8_t*)"Kernel:   %s\n", (const int8_t*)"dev");
	app_log((const int8_t*)"Build:    %s %s\n", (const int8_t*)__DATE__, (const int8_t*)__TIME__);
	app_log((const int8_t*)"Shell:    %s\n", (const int8_t*)"KOS Shell");
	app_log((const int8_t*)"Cmdline:  %s\n", cmd ? cmd : (const int8_t*)"(none)");
	app_log((const int8_t*)"Args:     %d\n", (unsigned)ac);
}

void app_hello(void) {
	print_ascii_logo();
	print_info_lines();
}

#ifndef APP_EMBED
int main(void) {
	app_hello();
	return 0;
}
#endif
