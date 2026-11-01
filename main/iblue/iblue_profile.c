/* iBlueControlMod: configuration profiles. See iblue_profile.h. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "adapter/config.h"
#include "iblue/iblue_profile.h"

#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED

#define PROFILE_MAGIC 0x31464F50 /* "POF1" */

struct profile_head {
    uint32_t magic;
    char name[IBLUE_PROFILE_NAME_MAX + 1];
} __attribute__((packed));

static int32_t current_slot = -1;

static void path_of(uint32_t slot, char *path, size_t len) {
    snprintf(path, len, "/fs/prof_%u", (unsigned)slot);
}

bool iblue_profile_info(uint32_t slot, char *name) {
    struct profile_head head;
    char path[24];
    FILE *f;
    size_t n;

    name[0] = '\0';
    if (slot >= IBLUE_PROFILES) {
        return false;
    }
    path_of(slot, path, sizeof(path));
    f = fopen(path, "rb");
    if (f == NULL) {
        return false;
    }
    n = fread(&head, sizeof(head), 1, f);
    fclose(f);
    if (n != 1 || head.magic != PROFILE_MAGIC) {
        return false;
    }
    memcpy(name, head.name, sizeof(head.name));
    name[IBLUE_PROFILE_NAME_MAX] = '\0';
    return true;
}

uint32_t iblue_profile_save(uint32_t slot, const char *name) {
    struct profile_head head = {.magic = PROFILE_MAGIC};
    char path[24];
    FILE *f;
    size_t n1, n2;

    if (slot >= IBLUE_PROFILES) {
        return IBLUE_PROF_BAD_ARG;
    }
    if (name[0] == '\0') {
        snprintf(head.name, sizeof(head.name), "Profile %u", (unsigned)(slot + 1));
    }
    else {
        strncpy(head.name, name, IBLUE_PROFILE_NAME_MAX);
    }
    path_of(slot, path, sizeof(path));
    f = fopen(path, "wb");
    if (f == NULL) {
        return IBLUE_PROF_FAILED;
    }
    n1 = fwrite(&head, sizeof(head), 1, f);
    n2 = fwrite(&config, sizeof(config), 1, f);
    fclose(f);
    if (n1 != 1 || n2 != 1) {
        remove(path);
        return IBLUE_PROF_FAILED;
    }
    return IBLUE_PROF_OK;
}

uint32_t iblue_profile_load(uint32_t slot) {
    struct profile_head head;
    struct config *tmp;
    char path[24];
    FILE *f;
    size_t n1, n2;
    uint32_t ret = IBLUE_PROF_OK;

    if (slot >= IBLUE_PROFILES) {
        return IBLUE_PROF_BAD_ARG;
    }
    path_of(slot, path, sizeof(path));
    f = fopen(path, "rb");
    if (f == NULL) {
        return IBLUE_PROF_EMPTY;
    }
    tmp = malloc(sizeof(*tmp)); /* 12 KB, only for a moment: read it all before the live configuration is touched */
    if (tmp == NULL) {
        fclose(f);
        return IBLUE_PROF_FAILED;
    }
    n1 = fread(&head, sizeof(head), 1, f);
    n2 = fread(tmp, sizeof(*tmp), 1, f);
    fclose(f);
    if (n1 != 1 || head.magic != PROFILE_MAGIC || n2 != 1 || tmp->magic != CONFIG_MAGIC) {
        ret = IBLUE_PROF_FAILED;
    }
    else {
        for (uint32_t i = 0; i < WIRED_MAX_DEV && ret == IBLUE_PROF_OK; i++) {
            if (tmp->in_cfg[i].map_size > ADAPTER_MAPPING_MAX || tmp->out_cfg[i].dev_mode > DEV_MOUSE ||
                    tmp->out_cfg[i].acc_mode > ACC_BOTH) {
                ret = IBLUE_PROF_FAILED;
            }
        }
    }
    if (ret == IBLUE_PROF_OK) {
        memcpy(&config, tmp, sizeof(config));
        config_update(config_get_src()); /* keeps it, and resets the wired side if the output mode changed */
        current_slot = (int32_t)slot;
        printf("# %s: profile %u \"%s\"\n", __FUNCTION__, (unsigned)slot, head.name);
    }
    free(tmp);
    return ret;
}

uint32_t iblue_profile_delete(uint32_t slot) {
    char path[24];

    if (slot >= IBLUE_PROFILES) {
        return IBLUE_PROF_BAD_ARG;
    }
    path_of(slot, path, sizeof(path));
    remove(path);
    if (current_slot == (int32_t)slot) {
        current_slot = -1;
    }
    return IBLUE_PROF_OK;
}

uint32_t iblue_profile_next(void) {
    char name[IBLUE_PROFILE_NAME_MAX + 1];

    for (uint32_t i = 1; i <= IBLUE_PROFILES; i++) {
        uint32_t slot = (uint32_t)(current_slot + (int32_t)i + IBLUE_PROFILES) % IBLUE_PROFILES;

        if (iblue_profile_info(slot, name)) {
            return iblue_profile_load(slot);
        }
    }
    return IBLUE_PROF_EMPTY;
}

#endif /* CONFIG_BLUERETRO_IBLUE_RGB_LED */
