#if IS_ENABLED(CONFIG_LGE_DISPLAY_DEBUG)

#ifndef LGE_MDSS_DEBUG_H
#define LGE_MDSS_DEBUG_H


#include <linux/fs.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/path.h>
#include <linux/namei.h>

#include "mdss_dsi.h"

enum {
	DEBUG_DSI_CMD_TX = 1,
	DEBUG_DSI_CMD_RX, //TODO : need to debug for long packet
	DEBUG_PWR_SEQ_DELAY,
	DEBUG_PWR_ALWAYS_ON,
	DEBUG_BLMAP_CHANGE,
	DEBUG_DSI_TIMING_CHANGE,
	INVALID,
};

struct debug_file_info {
	char file_name[256];
	char *cbuf;
	int *ibuf;
	loff_t file_size;
	int data_len;
	int data_type;
	int event;
};

int lge_debug_event_trigger(struct mdss_panel_data *pdata,
	char *debug_file, int debug_event);

#endif

#endif