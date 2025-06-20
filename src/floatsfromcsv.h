#ifndef FLOATSFROMCSV_H_
#define FLOATSFROMCSV_H_
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <stdint.h>

// Returns true if filename has the given extension
static bool float_hasExtension(const char *filename, const char *extension) {
    size_t flen = strlen(filename);
    size_t elen = strlen(extension);
    if(flen < elen) return false;
    return strcmp(filename + flen - elen, extension) == 0;
}

static uint32_t *read_float_file_gzip(const char *filename, size_t *howmany) {
    gzFile fp = gzopen(filename, "rb");
    if (!fp) {
        printf("Could not open file %s\n", filename);
        return NULL;
    }
    char line[256];
    size_t capacity = 1024;
    uint32_t *values = (uint32_t*)malloc(capacity * sizeof(uint32_t));
    if(!values) return NULL;
    size_t count = 0;
    while (gzgets(fp, line, sizeof(line))) {
        char *ptr = strrchr(line, ',');
        if(ptr) {
            double d = atof(ptr + 1);
            float f = (float)d;
            uint32_t v;
            memcpy(&v, &f, sizeof(float));
            if(count == capacity) {
                capacity *= 2;
                values = (uint32_t*)realloc(values, capacity * sizeof(uint32_t));
                if(!values) { gzclose(fp); return NULL; }
            }
            values[count++] = v;
        }
    }
    gzclose(fp);
    *howmany = count;
    return values;
}

static uint32_t **read_all_float_files(const char *dirname, const char *extension, size_t **howmany, size_t *count) {
    struct dirent **entry_list;
    int ci = scandir(dirname, &entry_list, 0, alphasort);
    if (ci < 0) return NULL;
    size_t c = (size_t)ci;
    size_t truec = 0;
    for (size_t i = 0; i < c; i++) {
        if (float_hasExtension(entry_list[i]->d_name, extension)) ++truec;
    }
    *count = truec;
    *howmany = (size_t*)malloc(sizeof(size_t) * (*count));
    uint32_t **answer = (uint32_t**)malloc(sizeof(uint32_t*) * (*count));
    size_t dirlen = strlen(dirname);
    char *modifdirname = (char*)dirname;
    if (modifdirname[dirlen - 1] != '/') {
        modifdirname = (char*)malloc(dirlen + 2);
        strcpy(modifdirname, dirname);
        modifdirname[dirlen] = '/';
        modifdirname[dirlen + 1] = '\0';
        dirlen++;
    }
    for (size_t i = 0, pos = 0; i < c; i++) {
        if (!float_hasExtension(entry_list[i]->d_name, extension)) continue;
        size_t filelen = strlen(entry_list[i]->d_name);
        char *fullpath = (char*)malloc(dirlen + filelen + 1);
        strcpy(fullpath, modifdirname);
        strcpy(fullpath + dirlen, entry_list[i]->d_name);
        answer[pos] = read_float_file_gzip(fullpath, &((*howmany)[pos]));
        pos++;
        free(fullpath);
    }
    if (modifdirname != dirname) free(modifdirname);
    for (size_t i = 0; i < c; ++i) free(entry_list[i]);
    free(entry_list);
    return answer;
}

#endif /* FLOATSFROMCSV_H_ */
