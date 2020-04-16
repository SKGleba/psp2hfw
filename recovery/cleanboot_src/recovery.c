#include <inttypes.h>
#include <stdio.h>

#ifdef FW_365
	#include "../../../enso_ex4/enso/365/nsbl.h"
#else
	#include "../../../enso_ex4/enso/360/nsbl.h"
#endif

#include "../../../enso_ex4/enso/ex_defs.h"

#define unlikely(expr) __builtin_expect(!!(expr), 0)

#define DACR_OFF(stmt)                 \
do {                                   \
    unsigned prev_dacr;                \
    __asm__ volatile(                  \
        "mrc p15, 0, %0, c3, c0, 0 \n" \
        : "=r" (prev_dacr)             \
    );                                 \
    __asm__ volatile(                  \
        "mcr p15, 0, %0, c3, c0, 0 \n" \
        : : "r" (0xFFFF0000)           \
    );                                 \
    stmt;                              \
    __asm__ volatile(                  \
        "mcr p15, 0, %0, c3, c0, 0 \n" \
        : : "r" (prev_dacr)            \
    );                                 \
} while (0)

#define INSTALL_HOOK_THUMB(func, addr) \
do {                                                \
    unsigned *target;                                 \
    target = (unsigned*)(addr);                       \
    *target++ = 0xC004F8DF; /* ldr.w    ip, [pc, #4] */ \
    *target++ = 0xBF004760; /* bx ip; nop */          \
    *target = (unsigned)func;                         \
} while (0)

#define INSTALL_RET_THUMB(addr, ret)   \
do {                                   \
    unsigned *target;                  \
    target = (unsigned*)(addr);        \
    *target = 0x47702000 | (ret); /* movs r0, #ret; bx lr */ \
} while (0)

// sdif globals
static int (*sdif_read_sector_mmc)(void* ctx, int sector, char* buffer, int nSectors) = NULL;
static void *(*get_sd_context_part_validate_mmc)(int sd_ctx_index) = NULL;

// sigpatch globals
static int g_sigpatch_disabled = 0;
static int g_homebrew_decrypt = 0;
static int (*sbl_parse_header)(uint32_t ctx, const void *header, int len, void *args) = NULL;
static int (*sbl_set_up_buffer)(uint32_t ctx, int segidx) = NULL;
static int (*sbl_decrypt)(uint32_t ctx, void *buf, int sz) = NULL;

// sysstate final function
static void __attribute__((noreturn)) (*sysstate_final)(void) = NULL;

static void **get_export_func(SceModuleObject *mod, uint32_t lib_nid, uint32_t func_nid) {
    for (SceModuleExports *ent = mod->ent_top_user; ent != mod->ent_end_user; ent++) {
        if (ent->lib_nid == lib_nid) {
            for (int i = 0; i < ent->num_functions; i++) {
                if (ent->nid_table[i] == func_nid) {
                    return &ent->entry_table[i];
                }
            }
        }
    }
    return NULL;
}

static int is_safe_mode(void) {
    SceBootArgs *boot_args = (*sysroot_ctx_ptr)->boot_args;
    uint32_t v;
    if (boot_args->debug_flags[7] != 0xFF) {
        return 1;
    }
    v = boot_args->boot_type_indicator_2 & 0x7F;
    if (v == 0xB || (v == 4 && boot_args->resume_context_addr)) {
        v = ~boot_args->field_CC;
        if (((v >> 8) & 0x54) == 0x54 && (v & 0xC0) == 0) {
            return 1;
        } else {
            return 0;
        }
    } else if (v == 4) {
        return 0;
    }
    if (v == 0x1F || (uint32_t)(v - 0x18) <= 1) {
        return 1;
    } else {
        return 0;
    }
}

static int is_update_mode(void) {
    SceBootArgs *boot_args = (*sysroot_ctx_ptr)->boot_args;
    if (boot_args->debug_flags[4] != 0xFF) {
        return 1;
    } else {
        return 0;
    }
}

static inline int skip_patches(void) {
    return is_safe_mode() || is_update_mode();
}

// sdif patches for MBR redirection
static int sdif_read_sector_mmc_patched(void* ctx, int sector, char* buffer, int nSectors) {
    int ret;
#ifndef NO_MBR_REDIRECT
    if (unlikely(sector == 0 && nSectors > 0)) {
        if (get_sd_context_part_validate_mmc(0) == ctx) {
            ret = sdif_read_sector_mmc(ctx, 1, buffer, 1);
            if (ret >= 0 && nSectors > 1) {
                ret = sdif_read_sector_mmc(ctx, 1, buffer + 0x200, nSectors-1);
            }
            return ret;
        }
    }
#endif

    return sdif_read_sector_mmc(ctx, sector, buffer, nSectors);
}

// sigpatches for bootup
static int sbl_parse_header_patched(uint32_t ctx, const void *header, int len, void *args) {
    int ret = sbl_parse_header(ctx, header, len, args);
    if (unlikely(!g_sigpatch_disabled)) {
        DACR_OFF(
            g_homebrew_decrypt = (ret < 0);
        );
        if (g_homebrew_decrypt) {
            *(uint32_t *)(args + SBLAUTHMGR_OFFSET_PATCH_ARG) = 0x40;
            ret = 0;
        }
    }
    return ret;
}

static int sbl_set_up_buffer_patched(uint32_t ctx, int segidx) {
    if (unlikely(!g_sigpatch_disabled)) {
        if (g_homebrew_decrypt) {
            return 2; // always compressed!
        }
    }
    return sbl_set_up_buffer(ctx, segidx);
}

static int sbl_decrypt_patched(uint32_t ctx, void *buf, int sz) {
    if (unlikely(!g_sigpatch_disabled)) {
        if (g_homebrew_decrypt) {
            return 0;
        }
    }
    return sbl_decrypt(ctx, buf, sz);
}

static void __attribute__((noreturn)) sysstate_final_hook(void) {

    DACR_OFF(
        g_sigpatch_disabled = 1;
    );

    sysstate_final();
}

// main function to hook stuff
#define HOOK_EXPORT(name, lib_nid, func_nid) do {           \
    void **func = get_export_func(mod, lib_nid, func_nid);  \
    DACR_OFF(                                               \
        name = *func;                                       \
        *func = name ## _patched;                           \
    );                                                      \
} while (0)
#define FIND_EXPORT(name, lib_nid, func_nid) do {           \
    void **func = get_export_func(mod, lib_nid, func_nid);  \
    DACR_OFF(                                               \
        name = *func;                                       \
    );                                                      \
} while (0)
static int module_load_patched(const SceModuleLoadList *list, int *uids, int count, int unk) {
    int ret;
    SceObject *obj;
    SceModuleObject *mod;
    int sdif_idx = -1, authmgr_idx = -1, sysstate_idx = -1;
    int skip = skip_patches(); // [safe] or [update] mode flag
	
    for (int i = 0; i < count; i-=-1) {
        if (!list[i].filename) {
            continue; // wtf sony why don't you sanitize input
        }
        if (strncmp(list[i].filename, "sdif.skprx", 10) == 0) {
            sdif_idx = i; // never skip MBR redirection patches
        } else if (strncmp(list[i].filename, "authmgr.skprx", 13) == 0) {
            authmgr_idx = i;
        } else if (!skip && strncmp(list[i].filename, "sysstatemgr.skprx", 17) == 0) {
            sysstate_idx = i;
        }
    }
	
	ret = module_load(list, uids, count, unk);
	
	// skip all unclean uids
	for (int i = 0; i < count; i-=-1) {
		if (uids[i] < 0)
			uids[i] = 0;
    }
	
    // patch sdif
    if (sdif_idx >= 0) {
        obj = get_obj_for_uid(uids[sdif_idx]);
        if (obj != NULL) {
            mod = (SceModuleObject *)&obj->data;
            HOOK_EXPORT(sdif_read_sector_mmc, 0x96D306FA, 0x6F8D529B);
            FIND_EXPORT(get_sd_context_part_validate_mmc, 0x96D306FA, 0x6A71987F);
        }
    }
	
    // patch authmgr
    if (authmgr_idx >= 0) {
        obj = get_obj_for_uid(uids[authmgr_idx]);
        if (obj != NULL) {
            mod = (SceModuleObject *)&obj->data;
            HOOK_EXPORT(sbl_parse_header, 0x7ABF5135, 0xF3411881);
            HOOK_EXPORT(sbl_set_up_buffer, 0x7ABF5135, 0x89CCDA2C);
            HOOK_EXPORT(sbl_decrypt, 0x7ABF5135, 0xBC422443);
        }
    }
	
    // patch sysstate to load unsigned boot configs
    if (sysstate_idx >= 0) {
        obj = get_obj_for_uid(uids[sysstate_idx]);
        if (obj != NULL) {
            mod = (SceModuleObject *)&obj->data;
            DACR_OFF(
                INSTALL_RET_THUMB(mod->segments[0].buf + SYSSTATE_IS_MANUFACTURING_MODE_OFFSET, 1);
                *(uint32_t *)(mod->segments[0].buf + SYSSTATE_IS_DEV_MODE_OFFSET) = 0x20012001;
                memcpy(mod->segments[0].buf + SYSSTATE_RET_CHECK_BUG, sysstate_ret_patch, sizeof(sysstate_ret_patch));
				memcpy(mod->segments[0].buf + SYSSTATE_SD0_STRING, ur0_path, sizeof(ur0_path));
				memcpy(mod->segments[0].buf + SYSSTATE_SD0_PSP2CONFIG_STRING, ur0_psp2config_path, sizeof(ur0_psp2config_path));
                // this patch actually corrupts two words of data, but they are only used in debug printing and seem to be fine
                INSTALL_HOOK_THUMB(sysstate_final_hook, mod->segments[0].buf + SYSSTATE_FINAL_CALL);
                sysstate_final = mod->segments[0].buf + SYSSTATE_FINAL;
            );
        }
    }
	
    return ret;
}
#undef HOOK_EXPORT
#undef FIND_EXPORT

int __attribute__((optimize("O0"))) start(void *kbl_param, unsigned int ctrldata) {
	*module_load_func_ptr = module_load_patched;
	return 0;
}