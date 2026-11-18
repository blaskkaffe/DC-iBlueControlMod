/* iBlueControlMod: configuration profiles. See iblue_profile.h. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <sys/stat.h>
#include "adapter/adapter.h"
#include "adapter/config.h"
#include "adapter/gameid.h"
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

void iblue_profile_list(struct brctl_profiles *out) {
    memset(out, 0, sizeof(*out));
    out->magic = BRCTL_PROFILES_MAGIC;
    for (uint32_t i = 0; i < BRCTL_PROFILES && i < IBLUE_PROFILES; i++) {
        struct profile_head head;
        char path[24];
        FILE *f;

        path_of(i, path, sizeof(path));
        f = fopen(path, "rb");
        if (f == NULL) {
            continue;
        }
        if (fread(&head, sizeof(head), 1, f) == 1 && head.magic == PROFILE_MAGIC) {
            uint8_t modes[2] = {0, 0};

            out->used[i] = 1;
            if (fseek(f, (long)(sizeof(head) + offsetof(struct config, out_cfg)), SEEK_SET) == 0 &&
                    fread(modes, sizeof(modes), 1, f) == 1) {
                out->dev_mode[i] = modes[0];
                out->acc_mode[i] = modes[1];
            }
        }
        fclose(f);
    }
}

/* ---- Games ---- */

static bool game_file_exists(const char *full) {
    struct stat st;
    char path[32];

    snprintf(path, sizeof(path), "/fs/%s", full);
    return full[0] && stat(path, &st) == 0;
}

uint32_t iblue_game_set(const char *id) {
    char full[24] = "";
    size_t n = strnlen(id, BRCTL_GAME_ID_MAX + 1);

    if (n > BRCTL_GAME_ID_MAX) {
        return BRCTL_RES_BAD;
    }
    for (size_t i = 0; i < n; i++) {
        char c = id[i];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '-' || c == '_')) {
            return BRCTL_RES_BAD; /* the id becomes part of a file name */
        }
    }
    if (n) {
        snprintf(full, sizeof(full), "DC_%s", id);
    }
    gid_set(full);
    config_init(GAMEID_CFG); /* the game's file if it has one, else the normal configuration */
    return BRCTL_RES_OK;
}

uint32_t iblue_game_save(void) {
    if (!gid_get()[0]) {
        return BRCTL_RES_NO_GAME;
    }
    config_update(GAMEID_CFG);
    return game_file_exists(gid_get()) ? BRCTL_RES_OK : BRCTL_RES_FAILED;
}

uint32_t iblue_game_delete(void) {
    char path[32];

    if (!gid_get()[0]) {
        return BRCTL_RES_NO_GAME;
    }
    snprintf(path, sizeof(path), "/fs/%s", gid_get());
    remove(path);
    config_init(DEFAULT_CFG);
    return BRCTL_RES_OK;
}

void iblue_game_info(struct brctl_game *out) {
    const char *full = gid_get();

    memset(out, 0, sizeof(*out));
    if (full[0]) {
        out->flags = BRCTL_GAME_SET | (game_file_exists(full) ? BRCTL_GAME_HAS_OWN : 0) |
            (config_get_src() == GAMEID_CFG ? BRCTL_GAME_IN_USE : 0);
        strncpy(out->id, strncmp(full, "DC_", 3) == 0 ? full + 3 : full, sizeof(out->id) - 1);
    }
}

#endif /* CONFIG_BLUERETRO_IBLUE_RGB_LED */
