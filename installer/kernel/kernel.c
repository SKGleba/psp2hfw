// This code will "clone" EMMC to GCSD.

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <vitasdkkern.h>

typedef struct hfw_config_struct {
  uint32_t orig_fwv;
  uint32_t fw_fwv;
  uint32_t kasm_sz;
  uint32_t ussm_sz;
} __attribute__((packed)) hfw_config_struct;

static int opret = 0;

static void *fsp_buf;

static int copyd(const char *src, const char *dst, uint32_t fsz) {
	int gibsz = 0;
	SceUID fd = ksceIoOpen(src, SCE_O_RDWR, 0777);
	if (fd < 0)
		return 1;
	SceUID wfd;
	if (fsz == 0) {
		SceIoStat stat;
		ksceIoGetstat(src, &stat);
		fsz = stat.st_size;
		gibsz = stat.st_size;
		wfd = ksceIoOpen(dst, SCE_O_WRONLY | SCE_O_TRUNC | SCE_O_CREAT, 6);
	} else
		wfd = ksceIoOpen(dst, SCE_O_RDWR, 0777);
	if (wfd < 0) {
		ksceIoClose(fd);
		return 1;
	}
	uint32_t size = fsz;
	if (size > 0x1000000)
		size = 0x1000000;
	uint32_t i = 0;
	while ((i + size) < (fsz + 1)) {
		memset(fsp_buf, 0, size);
		ksceIoRead(fd, fsp_buf, size);
		ksceIoWrite(wfd, fsp_buf, size);
		i = i + size;
	}
	if (i < fsz) {
		size = fsz - i;
		memset(fsp_buf, 0, size);
		ksceIoRead(fd, fsp_buf, size);
		ksceIoWrite(wfd, fsp_buf, size);
	}
	ksceIoClose(fd);
	ksceIoClose(wfd);
	return gibsz;
}

static void create_hfw_cfg(uint32_t ussmsz, uint32_t kadecsz) {
	if (ussmsz == 1)
		ussmsz = 0;
	if (kadecsz == 1)
		kadecsz = 0;
	SceUID fd = ksceIoOpen("os0:psp2bootconfig.skprx", SCE_O_RDONLY, 0);
	if (fd < 0)
		return;
	char tbuf[0xa0];
	ksceIoRead(fd, &tbuf, 0xa0);
	ksceIoClose(fd);
	hfw_config_struct hfwcfg;
	hfwcfg.orig_fwv = (uint32_t)ksceKernelSysrootGetSystemSwVersion();
	hfwcfg.fw_fwv = *(uint32_t *)(tbuf + 0x92);
	hfwcfg.kasm_sz = kadecsz;
	hfwcfg.ussm_sz = ussmsz;
	fd = ksceIoOpen("os0:hfw_cfg.bin", SCE_O_WRONLY | SCE_O_TRUNC | SCE_O_CREAT, 6);
	ksceIoWrite(fd, &hfwcfg, 0x10);
	ksceIoClose(fd);
}

static int install_hfw() {
	opret = 4;
	if (copyd("sdstor0:int-lp-act-os", "sdstor0:int-lp-ina-os", 0x1000000) == 0) { // backup act2ina for recovery
		if (copyd("ux0:data/hfw/os0.bin", "sdstor0:int-lp-act-os", 0x1000000) == 0) { // flash os0
			ksceIoUmount(0x200, 0, 0, 0);
			ksceIoUmount(0x200, 1, 0, 0);
			ksceIoMount(0x200, NULL, 2, 0, 0, 0);
			int bmgrcp = copyd("ux0:eex/data/bootmgr.e2xp", "os0:bootmgr.e2xp", 0); // copy bootmgr
			int ussmcp = copyd("ux0:eex/data/zss_ussm.self", "os0:zss_ussm.self", 0); // copy origfw update_service_sm.self
			int kadecp = copyd("ux0:eex/data/zss_ka.elf", "os0:zss_ka.elf", 0); // optional: kprxauth_sm (dec)
			copyd("ux0:data/hfw/patches.e2xd", "os0:patches.e2xd", 0); // optional: precompiled e2x patches file
			copyd("ux0:eex/data/bootlogo.raw", "os0:bootlogo.raw", 0); // optional: current bootlogo
			if ((ussmcp > 1) && (bmgrcp > 1)) {
				if (copyd("ux0:data/hfw/vs0.bin", "sdstor0:int-lp-ign-vsh", 0x10000000) == 0) { // flash vs0
					create_hfw_cfg(ussmcp, kadecp);
					opret = 0;
				}
			} else
				opret = 3;
		} else
			opret = 2;
	} else
		opret = 1;
	return opret;
}

static void restore_oldfw() {
	if (copyd("ux0:data/hfw/os0_r.bin", "sdstor0:int-lp-act-os", 0x1000000) == 1)
		copyd("sdstor0:int-lp-ina-os", "sdstor0:int-lp-act-os", 0x1000000);
	ksceIoUmount(0x200, 0, 0, 0);
	ksceIoUmount(0x200, 1, 0, 0);
	ksceIoMount(0x200, NULL, 2, 0, 0, 0);
	copyd("ux0:data/hfw/patches_r.e2xd", "os0:patches.e2xd", 0); // optional: precompiled e2x patches file
	copyd("ux0:data/hfw/vs0_r.bin", "sdstor0:int-lp-ign-vsh", 0x10000000);
	return;
}

static int siofix(void *func) {
	int ret = 0;
	int res = 0;
	int uid = 0;
	ret = uid = ksceKernelCreateThread("siofix", func, 64, 0x10000, 0, 0, 0);
	if (ret < 0){ret = -1; goto cleanup;}
	if ((ret = ksceKernelStartThread(uid, 0, NULL)) < 0) {ret = -1; goto cleanup;}
	if ((ret = ksceKernelWaitThreadEnd(uid, &res, NULL)) < 0) {ret = -1; goto cleanup;}
	ret = res;
cleanup:
	if (uid > 0) ksceKernelDeleteThread(uid);
	return ret;
}

void _start() __attribute__ ((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args) {
	int state = 0;
	ENTER_SYSCALL(state); // not required but y not
	
	int mblk = ksceKernelAllocMemBlock("clone_buf", 0x10C0D006, 0x1000000, NULL); // 16MiB memblock
	if (mblk < 0)
		return SCE_KERNEL_START_FAILED;
	ksceKernelGetMemBlockBase(mblk, (void**)&fsp_buf);
	
	SceIoStat stat;
	int restore = ksceIoGetstat("os0:hfw_cfg.bin", &stat);
	if (restore < 0)
		siofix(install_hfw);
	else
		siofix(restore_oldfw);
	
	ksceKernelFreeMemBlock(mblk);
	
	EXIT_SYSCALL(state);
	
	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
	return SCE_KERNEL_STOP_SUCCESS;
}