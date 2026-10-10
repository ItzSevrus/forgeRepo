#define _POSIX_C_SOURCE 200809L
#include "forge.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static char *trim(char *s)
{
    while (isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) *--end = '\0';
    return s;
}

static int copy_string_value(const char *value, char *out, size_t outsz)
{
    while (isspace((unsigned char)*value)) value++;
    if (*value != '"' && *value != '\'') return -1;
    char quote = *value++;
    size_t n = 0;
    while (*value && *value != quote) {
        char c = *value++;
        if (c == '\\' && *value) {
            char e = *value++;
            if (e == 'n') c = '\n';
            else if (e == 't') c = '\t';
            else c = e;
        }
        if (n + 1 >= outsz) return -1;
        out[n++] = c;
    }
    if (*value != quote) return -1;
    out[n] = '\0';
    return 0;
}

static int parse_array(const char *value, char out[][FORGE_MAX_NAME], size_t *count)
{
    *count = 0;
    while (isspace((unsigned char)*value)) value++;
    if (*value++ != '[') return -1;
    while (*value) {
        while (isspace((unsigned char)*value) || *value == ',') value++;
        if (*value == ']') return 0;
        if (*value != '"' && *value != '\'') return -1;
        char quote = *value++;
        char word[FORGE_MAX_NAME]; size_t n = 0;
        while (*value && *value != quote) {
            if (n + 1 >= sizeof(word)) return -1;
            word[n++] = *value++;
        }
        if (*value++ != quote || n == 0 || *count >= FORGE_MAX_DEPS) return -1;
        word[n] = '\0';
        if (!forge_valid_name(word)) return -1;
        snprintf(out[*count], FORGE_MAX_NAME, "%s", word);
        (*count)++;
        while (isspace((unsigned char)*value)) value++;
        if (*value == ']') return 0;
        if (*value != ',') return -1;
        value++;
    }
    return -1;
}

int forge_manifest_parse(const char *text, ForgePackage *pkg, char *err, size_t errsz)
{
    memset(pkg, 0, sizeof(*pkg));
    char *copy = strdup(text);
    if (!copy) { snprintf(err, errsz, "out of memory"); return -1; }
    char section[64] = "";
    char *save = NULL;
    for (char *line = strtok_r(copy, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        char *s = trim(line);
        if (!*s || *s == '#') continue;
        /* Strip comments outside simple quoted strings. */
        int quoted = 0; char q = 0;
        for (char *p = s; *p; p++) {
            if ((*p == '"' || *p == '\'') && (!quoted || *p == q)) { if (!quoted) { quoted = 1; q = *p; } else quoted = 0; }
            else if (*p == '#' && !quoted) { *p = '\0'; break; }
        }
        s = trim(s); if (!*s) continue;
        if (*s == '[') {
            char *end = strchr(s, ']');
            if (!end || end[1] != '\0') { snprintf(err, errsz, "malformed section header: %.80s", s); free(copy); return -1; }
            *end = '\0';
            snprintf(section, sizeof(section), "%s", trim(s + 1));
            continue;
        }
        char *eq = strchr(s, '=');
        if (!eq) { snprintf(err, errsz, "expected key = value: %.80s", s); free(copy); return -1; }
        *eq++ = '\0';
        char *key = trim(s), *value = trim(eq);
        char *dest = NULL; size_t destsz = 0;
        if (strcmp(section, "package") == 0) {
            if (!strcmp(key, "name")) { dest = pkg->name; destsz = sizeof(pkg->name); }
            else if (!strcmp(key, "version")) { dest = pkg->version; destsz = sizeof(pkg->version); }
            else if (!strcmp(key, "description")) { dest = pkg->description; destsz = sizeof(pkg->description); }
            else if (!strcmp(key, "license")) { dest = pkg->license; destsz = sizeof(pkg->license); }
        } else if (strcmp(section, "source") == 0) {
            if (!strcmp(key, "url")) { dest = pkg->source_url; destsz = sizeof(pkg->source_url); }
            else if (!strcmp(key, "checksum")) { dest = pkg->checksum; destsz = sizeof(pkg->checksum); }
        } else if (strcmp(section, "build") == 0) {
            if (!strcmp(key, "script")) { dest = pkg->build_script; destsz = sizeof(pkg->build_script); }
            else if (!strcmp(key, "command")) { dest = pkg->build_command; destsz = sizeof(pkg->build_command); }
            else if (!strcmp(key, "compilers")) {
                if (parse_array(value, pkg->compilers, &pkg->compiler_count)) { snprintf(err, errsz, "invalid build.compilers array"); free(copy); return -1; }
            }
        } else if (strcmp(section, "dependencies") == 0) {
            if (!strcmp(key, "runtime")) {
                if (parse_array(value, pkg->runtime_deps, &pkg->runtime_dep_count)) { snprintf(err, errsz, "invalid dependencies.runtime array"); free(copy); return -1; }
            } else if (!strcmp(key, "build")) {
                if (parse_array(value, pkg->build_deps, &pkg->build_dep_count)) { snprintf(err, errsz, "invalid dependencies.build array"); free(copy); return -1; }
            }
        } else if (strcmp(section, "dependencies.system") == 0) {
            if (!strcmp(key, "apt")) {
                if (parse_array(value, pkg->apt_deps, &pkg->apt_dep_count)) { snprintf(err, errsz, "invalid dependencies.system.apt array"); free(copy); return -1; }
            } else if (!strcmp(key, "pacman")) {
                if (parse_array(value, pkg->pacman_deps, &pkg->pacman_dep_count)) { snprintf(err, errsz, "invalid dependencies.system.pacman array"); free(copy); return -1; }
            } else if (!strcmp(key, "dnf")) {
                if (parse_array(value, pkg->dnf_deps, &pkg->dnf_dep_count)) { snprintf(err, errsz, "invalid dependencies.system.dnf array"); free(copy); return -1; }
            } else if (!strcmp(key, "zypper")) {
                if (parse_array(value, pkg->zypper_deps, &pkg->zypper_dep_count)) { snprintf(err, errsz, "invalid dependencies.system.zypper array"); free(copy); return -1; }
            }
        }
        if (dest && copy_string_value(value, dest, destsz) != 0) {
            snprintf(err, errsz, "invalid string for %s.%s", section, key); free(copy); return -1;
        }
    }
    free(copy);
    if (!pkg->build_command[0] && pkg->build_script[0])
        snprintf(pkg->build_command, sizeof(pkg->build_command), "./%s", pkg->build_script);
    return forge_manifest_validate(pkg, err, errsz);
}

int forge_manifest_validate(const ForgePackage *pkg, char *err, size_t errsz)
{
    if (!forge_valid_name(pkg->name)) { snprintf(err, errsz, "missing or invalid package.name"); return -1; }
    if (!pkg->version[0]) { snprintf(err, errsz, "missing package.version"); return -1; }
    if (!pkg->description[0]) { snprintf(err, errsz, "missing package.description"); return -1; }
    if (!pkg->license[0]) { snprintf(err, errsz, "missing package.license"); return -1; }
    if (strncmp(pkg->source_url, "https://", 8) != 0 && strncmp(pkg->source_url, "http://", 7) != 0) {
        snprintf(err, errsz, "source.url must be an HTTP(S) URL"); return -1;
    }
    if (!pkg->build_script[0]) { snprintf(err, errsz, "missing build.script"); return -1; }
    if (strchr(pkg->build_command, '\n') || strchr(pkg->build_command, '\r')) { snprintf(err, errsz, "build.command must be a single-line shell command"); return -1; }
    if (pkg->build_script[0] == '/' || strstr(pkg->build_script, "..") || strchr(pkg->build_script, '\\')) {
        snprintf(err, errsz, "build.script must be a safe relative path"); return -1;
    }
    for (const char *p = pkg->build_script; *p; p++)
        if (!(isalnum((unsigned char)*p) || *p == '-' || *p == '_' || *p == '.' || *p == '/')) {
            snprintf(err, errsz, "build.script contains unsupported characters"); return -1;
        }
    if (pkg->runtime_dep_count > FORGE_MAX_DEPS || pkg->build_dep_count > FORGE_MAX_DEPS ||
        pkg->apt_dep_count > FORGE_MAX_DEPS || pkg->pacman_dep_count > FORGE_MAX_DEPS ||
        pkg->dnf_dep_count > FORGE_MAX_DEPS || pkg->zypper_dep_count > FORGE_MAX_DEPS ||
        pkg->compiler_count > 16) {
        snprintf(err, errsz, "too many dependencies or compiler checks"); return -1;
    }
    return 0;
}
