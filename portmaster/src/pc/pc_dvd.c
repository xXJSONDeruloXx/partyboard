/* GameCube DVD replacement: read-only access to a PortMaster game directory. */
#include "pc_platform.h"
#include "pc_prof.h"

#include <dolphin/dvd.h>
#include <dolphin/os.h>

#include <errno.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define DVD_ENTRY_MAX 1024

static char dvd_root[PATH_MAX] = ".";
static char dvd_entries[DVD_ENTRY_MAX][PATH_MAX];
static int dvd_entry_count;
static DVDDiskID dvd_disk_id = {
    {'G', 'M', 'P', 'E'}, {'0', '1'}, 0, 1, 0, 0, {0}
};
static u8 *dol_data;
static s32 dol_size;

void pm_dvd_set_root(const char *root)
{
    if (root == NULL || root[0] == '\0') {
        return;
    }
    strncpy(dvd_root, root, sizeof(dvd_root) - 1);
    dvd_root[sizeof(dvd_root) - 1] = '\0';
}

static int normalize_path(const char *path, char *out, size_t out_size)
{
    const char *start;
    size_t length;

    if (path == NULL || out_size == 0) {
        return 0;
    }
    start = path;
    while (*start == '/') {
        start++;
    }
    if (*start == '\0' || strstr(start, "..") != NULL) {
        return 0;
    }
    length = strlen(start);
    if (length >= out_size) {
        return 0;
    }
    memcpy(out, start, length + 1);
    return 1;
}

static int make_candidate(const char *relative, char *out, size_t out_size,
                          int files_subdir)
{
    int written;

    if (files_subdir) {
        written = snprintf(out, out_size, "%s/files/%s", dvd_root, relative);
    } else {
        written = snprintf(out, out_size, "%s/%s", dvd_root, relative);
    }
    return written >= 0 && (size_t)written < out_size;
}

static FILE *open_relative(const char *relative, char *resolved, size_t resolved_size)
{
    FILE *file;

    if (!make_candidate(relative, resolved, resolved_size, 0)) {
        return NULL;
    }
    file = fopen(resolved, "rb");
    if (file != NULL) {
        return file;
    }
    if (!make_candidate(relative, resolved, resolved_size, 1)) {
        return NULL;
    }
    return fopen(resolved, "rb");
}

static FILE *file_for_info(DVDFileInfo *file_info)
{
    return (FILE *)file_info->cb.addr;
}

DVDDiskID *DVDGetCurrentDiskID(void)
{
    return &dvd_disk_id;
}

void DVDInit(void)
{
    dvd_entry_count = 0;
    if (getenv("PARTYBOARD_GAME_ROOT") != NULL) {
        pm_dvd_set_root(getenv("PARTYBOARD_GAME_ROOT"));
    }
}

s32 DVDConvertPathToEntrynum(const char *path)
{
    char relative[PATH_MAX];
    char resolved[PATH_MAX];
    FILE *file;
    int i;

    if (!normalize_path(path, relative, sizeof(relative))) {
        return -1;
    }
    for (i = 0; i < dvd_entry_count; i++) {
        if (strcmp(dvd_entries[i], relative) == 0) {
            return i;
        }
    }
    if (dvd_entry_count >= DVD_ENTRY_MAX) {
        OSReport("[PM/DVD] entry table full\n");
        return -1;
    }
    file = open_relative(relative, resolved, sizeof(resolved));
    if (file == NULL) {
        return -1;
    }
    fclose(file);
    strncpy(dvd_entries[dvd_entry_count], relative, PATH_MAX - 1);
    dvd_entries[dvd_entry_count][PATH_MAX - 1] = '\0';
    return dvd_entry_count++;
}

BOOL DVDFastOpen(s32 entrynum, DVDFileInfo *file_info)
{
    char resolved[PATH_MAX];
    FILE *file;
    long length;

    if (entrynum < 0 || entrynum >= dvd_entry_count) {
        return FALSE;
    }
    file = open_relative(dvd_entries[entrynum], resolved, sizeof(resolved));
    if (file == NULL) {
        return FALSE;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return FALSE;
    }
    length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return FALSE;
    }
    memset(file_info, 0, sizeof(*file_info));
    file_info->cb.addr = file;
    file_info->cb.state = DVD_STATE_END;
    file_info->length = (u32)length;
    return TRUE;
}

BOOL DVDOpen(const char *file_name, DVDFileInfo *file_info)
{
    s32 entry = DVDConvertPathToEntrynum(file_name);
    return entry >= 0 ? DVDFastOpen(entry, file_info) : FALSE;
}

BOOL DVDClose(DVDFileInfo *file_info)
{
    FILE *file = file_for_info(file_info);
    if (file != NULL) {
        fclose(file);
    }
    file_info->cb.addr = NULL;
    return TRUE;
}

s32 DVDReadPrio(DVDFileInfo *file_info, void *address, s32 length,
                s32 offset, s32 priority)
{
    FILE *file = file_for_info(file_info);
    unsigned long long start = pc_prof_now_us();
    size_t read_count;
    (void)priority;

    if (file == NULL || length < 0 || offset < 0 ||
        (u32)offset > file_info->length) {
        return DVD_RESULT_FATAL_ERROR;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        return DVD_RESULT_FATAL_ERROR;
    }
    read_count = fread(address, 1, (size_t)length, file);
    pc_prof_report("dvd_read", length, start);
    return read_count == (size_t)length ? length : DVD_RESULT_FATAL_ERROR;
}

BOOL DVDReadAsyncPrio(DVDFileInfo *file_info, void *address, s32 length,
                      s32 offset, DVDCallback callback, s32 priority)
{
    s32 result = DVDReadPrio(file_info, address, length, offset, priority);
    if (callback != NULL) {
        callback(result, file_info);
    }
    return result >= 0;
}

s32 DVDSeekPrio(DVDFileInfo *file_info, s32 offset, s32 priority)
{
    (void)priority;
    return file_info != NULL && offset >= 0 &&
                   (u32)offset <= file_info->length ? offset : DVD_RESULT_FATAL_ERROR;
}

int DVDSeekAsyncPrio(DVDFileInfo *file_info, s32 offset,
                     void (*callback)(s32, DVDFileInfo *), s32 priority)
{
    s32 result = DVDSeekPrio(file_info, offset, priority);
    if (callback != NULL) {
        callback(result, file_info);
    }
    return result >= 0;
}

s32 DVDGetFileInfoStatus(const DVDFileInfo *file_info)
{
    return file_info != NULL && file_info->cb.addr != NULL ? DVD_STATE_END : DVD_STATE_FATAL_ERROR;
}

void *DVDGetFSTLocation(void) { return NULL; }
s32 DVDGetTransferredSize(DVDFileInfo *file_info) { (void)file_info; return 0; }
s32 DVDGetDriveStatus(void) { return DVD_STATE_END; }
s32 DVDGetCommandBlockStatus(const DVDCommandBlock *block)
{
    return block != NULL ? block->state : DVD_STATE_FATAL_ERROR;
}

BOOL DVDCheckDisk(void) { return TRUE; }
void DVDReset(void) {}
int DVDResetRequired(void) { return FALSE; }
BOOL DVDSetAutoInvalidation(BOOL enabled) { (void)enabled; return TRUE; }
void DVDPause(void) {}
void DVDResume(void) {}
int DVDSetAutoFatalMessaging(BOOL enabled) { (void)enabled; return TRUE; }

s32 DVDCancel(volatile DVDCommandBlock *block) { (void)block; return DVD_RESULT_CANCELED; }
int DVDCancelAsync(DVDCommandBlock *block, DVDCBCallback callback)
{
    (void)block;
    (void)callback;
    return TRUE;
}
s32 DVDCancelAll(void) { return DVD_RESULT_GOOD; }
int DVDCancelAllAsync(DVDCBCallback callback) { (void)callback; return TRUE; }

BOOL DVDLowReadDiskID(DVDDiskID *disk_id, DVDLowCallback callback)
{
    if (disk_id != NULL) {
        *disk_id = dvd_disk_id;
    }
    if (callback != NULL) {
        callback(DVD_RESULT_GOOD);
    }
    return TRUE;
}

const u8 *DVDGetDOLLocation(s32 *out_size)
{
    char resolved[PATH_MAX];
    FILE *file;
    long length;

    if (dol_data != NULL) {
        if (out_size != NULL) *out_size = dol_size;
        return dol_data;
    }
    file = open_relative("sys/main.dol", resolved, sizeof(resolved));
    if (file == NULL) {
        file = open_relative("main.dol", resolved, sizeof(resolved));
    }
    if (file == NULL || fseek(file, 0, SEEK_END) != 0) {
        if (file != NULL) fclose(file);
        return NULL;
    }
    length = ftell(file);
    if (length <= 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    dol_data = (u8 *)malloc((size_t)length);
    if (dol_data == NULL || fread(dol_data, 1, (size_t)length, file) != (size_t)length) {
        free(dol_data);
        dol_data = NULL;
        fclose(file);
        return NULL;
    }
    fclose(file);
    dol_size = (s32)length;
    if (out_size != NULL) *out_size = dol_size;
    return dol_data;
}

BOOL DVDPrepareStreamAsync(DVDFileInfo *file_info, u32 length, u32 offset,
                           DVDCallback callback)
{
    (void)file_info;
    (void)length;
    (void)offset;
    (void)callback;
    return TRUE;
}
s32 DVDPrepareStream(DVDFileInfo *file_info, u32 length, u32 offset)
{
    (void)file_info;
    (void)length;
    (void)offset;
    return DVD_RESULT_GOOD;
}
