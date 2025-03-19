//
// Created by tobin on 2024-08-30.
//

#ifndef SLFILE_H
#define SLFILE_H

#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <libgen.h>
#include <dirent.h>
#include <stdio.h>
#include <pthread.h>

#include "dialect.h"

typedef struct {
    char *name;
    char *full_path;
    u64 size;
    i64 modTime;
    i64 accTime;

    u8 isHidden: 1;
    u8 isDirectory: 1;

    u32 index;
} slfile_t;


typedef struct {
    float size_in_units;
    char unit[4];
} file_size_str_t;

static slfile_t slfile_new_pro(char *full_path_in, char *result_path_out, char *result_name_out) {
    slfile_t x = (slfile_t){0};

    struct stat s = (struct stat){0};
    stat(full_path_in, &s);

    // x.full_path = malloc(strlen(full_path_in) + 1);
    x.full_path = result_path_out;
    strcpy(x.full_path, full_path_in);


    x.modTime = s.st_mtime;
    x.accTime = s.st_atime;

    x.size = s.st_size;

    // x.name = malloc(strlen(slice) + 1);
    char *slice = basename(full_path_in);
    x.name = result_name_out;
    strcpy(x.name, slice);

    u64 fa = GetFileAttributes(full_path_in);

    x.isHidden = !!(fa & 0x00000002);
    x.isDirectory = !!(fa & 0x00000010);

    x.index = 0;

    return x;
}

static slfile_t slfile_new(char *full_path_in) {
    char *full_path_out = malloc(strlen(full_path_in) + 1);
    char *name_out = malloc(strlen(basename(full_path_in)) + 1);

    return slfile_new_pro(full_path_in, full_path_out, name_out);
}

typedef struct {
    //Whether files links `.` and `..` should be _kept_
    u8 keep_nav: 1;

    //Whether hidden files (not including `.` and `..` should be _skipped_
    u8 skip_hidden: 1;

    //Whether iteration should be done recursively
    u8 recurse: 1;

    u8 skip_files: 1;
    u8 skip_directories: 1;
} file_skip_flags;

static u8 dir_should_skip_file(struct dirent *de, file_skip_flags flags) {
    if (!flags.keep_nav && (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)) {
        return 1;
    }

    return 0;
}

static u0 slfile_free(slfile_t *xx) {
    free(xx->name);
    free(xx->full_path);

    xx->name = NULL;
    xx->full_path = NULL;
}

static u32 dir_count(char *dirPath, file_skip_flags flags) {
    struct dirent *dp;
    DIR *dfd;

    if ((dfd = opendir(dirPath)) == NULL) {
        printf("failed to open dir: `%s`", dirPath);
    }

    char filePath[512] = {0};

    char file_path_out[512] = {0};
    char file_name_out[512] = {0};

    u32 index = 0;

    while ((dp = readdir(dfd)) != NULL) {
        filePath[0] = '\0';

        if (dir_should_skip_file(dp, flags)) {
            continue;
        }

        //Place the dirpath
        strcat(filePath, dirPath);

        char last = filePath[strlen(filePath) - 1];
        if (!(last == '\\' || last == '/')) {
            strcat(filePath, "\\");
        }

        strcat(filePath, dp->d_name);

        // slfile_t xxx = slfile_new(filePath);
        slfile_t xxx = slfile_new_pro(filePath, file_path_out, file_name_out);
        u8 isDir = xxx.isDirectory;

        xxx.index = index++;


        if (flags.recurse && isDir) {
            index += dir_count(filePath, flags);
        }

        if (flags.skip_directories && isDir) {
            --index;
            continue;
        }

        if (flags.skip_files && !isDir) {
            --index;
            continue;
        }
    }
    closedir(dfd);

    return index;
}

u0 dir_iterate(char *dirPath, u0 (*function)(slfile_t *), file_skip_flags flags) {
    struct dirent *dp;
    DIR *dfd;

    if ((dfd = opendir(dirPath)) == NULL) {
        printf("failed to open dir: `%s`", dirPath);
    }

    u64 index = 0;

    char filePath[512] = {0};

    //TODO multithread this shiet.
    while ((dp = readdir(dfd)) != NULL) {
        filePath[0] = '\0';

        if (dir_should_skip_file(dp, flags)) {
            continue;
        }

        //Place the dirpath
        strcat(filePath, dirPath);

        char last = filePath[strlen(filePath) - 1];
        if (!(last == '\\' || last == '/')) {
            strcat(filePath, "\\");
        }

        strcat(filePath, dp->d_name);

        slfile_t xxx = slfile_new(filePath);
        u8 isDir = xxx.isDirectory;

        xxx.index = index++;


        if (flags.recurse && isDir) {
            function(&xxx);
            dir_iterate(filePath, function, flags);
            continue;
        }

        if (flags.skip_directories && isDir) {
            continue;
        }
        if (flags.skip_files && !isDir) {
            continue;
        }
        function(&xxx);
    }

    closedir(dfd);
}


/// Calculates a file_size_t from a u64 size_B
/// \param size_B : u64 : Represents the size in B
/// \param use_metric : u8 : A boolean for whether metric or binary should be used (MB vs MiB)
/// \return file_size_t : A file_size_t struct value
static file_size_str_t dir_calculate_file_size_t(u64 size_B, u8 use_metric) {
    double size = (double) size_B;

    if (use_metric) {
        if (size < 1000) {
            return (file_size_str_t){.size_in_units = size, .unit = "B"};
        } else if (size < (1000 * 1000)) {
            return (file_size_str_t){.size_in_units = size / 1000.0, .unit = "KB"};
        } else if (size < (1000 * 1000 * 1000)) {
            return (file_size_str_t){.size_in_units = size / 1000.0 / 1000.0, .unit = "MB"};
        } else if (size < (1000 * 1000 * 1000 * 1000)) {
            return (file_size_str_t){.size_in_units = size / 1000.0 / 1000.0 / 1000.0, .unit = "GB"};
        } else {
            return (file_size_str_t){.size_in_units = size / 1000.0 / 1000.0 / 1000.0, .unit = "TB"};
        }
    } else {
        if (size < 1024) {
            return (file_size_str_t){.size_in_units = size, .unit = "B"};
        } else if (size < (1024 * 1024)) {
            return (file_size_str_t){.size_in_units = size / 1024.0, .unit = "KiB"};
        } else if (size < (1024 * 1024 * 1024)) {
            return (file_size_str_t){.size_in_units = size / 1024.0 / 1024.0, .unit = "MiB"};
        } else if (size < (1024 * 1024 * 1024 * 1024)) {
            return (file_size_str_t){.size_in_units = size / 1024.0 / 1024.0 / 1024.0, .unit = "GiB"};
        } else {
            return (file_size_str_t){.size_in_units = size / 1024.0 / 1024.0 / 1024.0 / 2024.0, .unit = "GiB"};
        }
    }
}


#endif //SLFILE_H
