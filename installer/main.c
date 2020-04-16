/*
 * Simple kplugin loader by xerpi
 */

#include <stdio.h>
#include <taihen.h>
#include <psp2/ctrl.h>
#include <psp2/io/fcntl.h>
#include "debugScreen.h"

#define printf(...) psvDebugScreenPrintf(__VA_ARGS__)

#define MOD_PATH "ux0:app/SKGHFW100/psp2hfwik.skprx"

typedef struct hfw_config_struct {
  uint32_t orig_fwv;
  uint32_t fw_fwv;
  uint32_t kasm_sz;
  uint32_t ussm_sz;
} __attribute__((packed)) hfw_config_struct;

static int update_mode = 1;

int parse_cfg() {
	hfw_config_struct hfwcfg;
	SceUID fd = sceIoOpen("os0:hfw_cfg.bin", SCE_O_RDONLY, 0);
	if (fd < 0)
		return -1;
	sceIoRead(fd, &hfwcfg, 0x10);
	sceIoClose(fd);
	printf("\n bootloaders: 0x%lX\n firmware: 0x%lX\n ussm size: 0x%lX\n kprxauth size: 0x%lX\n\n", hfwcfg.orig_fwv, hfwcfg.fw_fwv, hfwcfg.ussm_sz, hfwcfg.kasm_sz);
	return 0;
}

int ex(const char *fname) {
	FILE *file;
	if ((file = fopen(fname, "r"))) {
		fclose(file);
		return 1;
	}
	return 0;
}

void wait_key_press()
{
	SceCtrlData pad;

	printf("Press START to %s.\n", (update_mode) ? "flash HFW" : "restore OFW");

	while (1) {
		sceCtrlPeekBufferPositive(0, &pad, 1);
		if (pad.buttons & SCE_CTRL_START)
			break;
		sceKernelDelayThread(200 * 1000);
	}
}

void exit_key_press(int mode)
{
	SceCtrlData pad;

	printf("Press %s.\n", (mode) ? "CIRCLE to exit" : "CROSS to reboot");

	while (1) {
		sceCtrlPeekBufferPositive(0, &pad, 1);
		if (pad.buttons & SCE_CTRL_CIRCLE)
			sceKernelExitProcess(0);
		if (pad.buttons & SCE_CTRL_CROSS)
			scePowerRequestColdReset();
		sceKernelDelayThread(200 * 1000);
	}
}

int __attribute__((optimize("O0"))) main(int argc, char *argv[])
{
	SceUID mod_id;

	psvDebugScreenInit();

	printf("HFW installer v0.5 by SKGleba\n");
	
	if (ex("os0:hfw_cfg.bin"))
		update_mode = 0;
	else {
		if ((!ex("ux0:data/hfw/os0.bin")) || (!ex("ux0:data/hfw/vs0.bin")) || (!ex("ux0:eex/data/bootmgr.e2xp")) || (!ex("ux0:eex/data/zss_ussm.self"))) {
			printf("file not found!\n");
			exit_key_press(1);
		}
	}
	
	printf("FW status: %s\n\n", (update_mode) ? "probably OFW" : "probably HFW");
	
	wait_key_press();
	
	printf("\nPlease wait, working...\n");

	tai_module_args_t argg;
	argg.size = sizeof(argg);
	argg.pid = KERNEL_PID;
	argg.args = 0;
	argg.argp = NULL;
	argg.flags = 0;
	mod_id = taiLoadStartKernelModuleForUser(MOD_PATH, &argg);

	if (mod_id < 0) {
		printf("Error loading " MOD_PATH ": 0x%08X\n", mod_id);
		exit_key_press(1);
	}

	if (update_mode) {
		if (ex("os0:hfw_cfg.bin") == 0)
			printf("flash probably failed!\n");
		parse_cfg();
	} else {
		if (ex("os0:hfw_cfg.bin") == 1)
			printf("restore probably failed!\n");
	}
	
	printf("\nall done!\n\n");
	
	exit_key_press(0);

	return 0;
}
