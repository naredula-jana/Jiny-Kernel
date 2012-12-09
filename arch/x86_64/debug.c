/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *   x86_64/debug.c
 *   Author: Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
 *
 */
#include "common.h"
#include "mach_dep.h"
#define MAX_TRACE 1024
static uint64_t trace[MAX_TRACE+10];
static int tr_index=0;
static void debugFault(struct fault_ctx *ctx) {

	ctx->istack_frame->rflags =(ctx->istack_frame->rflags) | 0x10000; /* set RF bit */

	trace[tr_index]=ctx->istack_frame->rip;
	tr_index++;
	if (tr_index > MAX_TRACE){
		tr_index = 0;
	}

}

static int debug_enabled=0;
void start_debugtrace(){
	static int init_intr = 0;
	if (init_intr == 0) {
		ar_registerInterrupt(1, debugFault, "debugFault", NULL);
		init_intr = 1;
	}
	debug_enabled=1;
}


