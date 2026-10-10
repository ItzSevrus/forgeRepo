#include "forge.h"
#include <stdio.h>
#include <string.h>

static void help(void)
{
    printf("Forge %s — user-local package manager\n\n", FORGE_VERSION);
    puts("Usage: forge <command> [arguments]");
    puts("\nCommands:");
    puts("  setup                         Initialize Forge directories and defaults");
    puts("  config                        Show configuration");
    puts("  config get prefix             Get install prefix");
    puts("  config set prefix <path>      Set absolute user install prefix");
    puts("  repo list                     List configured repositories");
    puts("  repo add <name> <github-url>  Add a public GitHub repository");
    puts("  repo remove <name>            Remove a repository from the registry");
    puts("  repo sync [name]              Refresh lightweight remote package catalogs");
    puts("  repo validate [name] [--deep] Validate cached catalogs (deep fetches manifests)");
    puts("  search <query>                Search package names in cached catalogs");
    puts("  info <name>@<version>         Fetch package metadata (@latest is the default)");
    puts("  install <name>@<version> [--yes] Install a version (@latest is the default)");
    puts("  list                          List installed packages and repositories");
    puts("  upgrade                       Check installed packages for updates");
    puts("  upgrade <package>             Upgrade one installed package");
    puts("  clean                         Remove Forge cache and leftover build files");
    puts("  uninstall <package>           Remove files recorded for an installed package");
    puts("  help                          Show this help");
    puts("Examples: forge install hello-world@latest | forge install hello-world@v1.0.0");
    puts("Forge does not clone repositories. Run 'forge repo sync' before searching.");
    puts("Package build scripts execute as your user; inspect repositories before installing.");
}

int main(int argc, char **argv)
{
    if (argc < 2 || !strcmp(argv[1], "help") || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
        help(); return 0;
    }
    if (!strcmp(argv[1], "--version") || !strcmp(argv[1], "version")) {
        printf("forge %s\n", FORGE_VERSION); return 0;
    }
    ForgeConfig cfg;
    if (forge_init(&cfg) != 0) return 1;
    if (!strcmp(argv[1], "setup")) return forge_setup(&cfg);
    if (!strcmp(argv[1], "config")) return forge_config_command(&cfg, argc, argv);
    if (!strcmp(argv[1], "repo")) return forge_repo_command(&cfg, argc, argv);
    if (!strcmp(argv[1], "search")) {
        if (argc != 3) { fprintf(stderr, "Usage: forge search <query>\n"); return 1; }
        return forge_search(&cfg, argv[2]);
    }
    if (!strcmp(argv[1], "info")) {
        if (argc != 3) { fprintf(stderr, "Usage: forge info <package>\n"); return 1; }
        return forge_info(&cfg, argv[2]);
    }
    if (!strcmp(argv[1], "install")) {
        if (argc < 3 || argc > 4) { fprintf(stderr, "Usage: forge install <package> [--yes]\n"); return 1; }
        int yes = argc == 4 && !strcmp(argv[3], "--yes");
        if (argc == 4 && !yes) { fprintf(stderr, "Unknown option: %s\n", argv[3]); return 1; }
        return forge_install(&cfg, argv[2], yes);
    }
    if (!strcmp(argv[1], "list")) return forge_list_installed(&cfg);
    if (!strcmp(argv[1], "upgrade")) {
        if (argc > 3) { fprintf(stderr, "Usage: forge upgrade [package]\n"); return 1; }
        return forge_upgrade(&cfg, argc == 3 ? argv[2] : NULL);
    }
    if (!strcmp(argv[1], "clean")) {
        if (argc != 2) { fprintf(stderr, "Usage: forge clean\n"); return 1; }
        return forge_clean_cache(&cfg);
    }
    if (!strcmp(argv[1], "uninstall") || !strcmp(argv[1], "remove")) {
        if (argc != 3) { fprintf(stderr, "Usage: forge uninstall <package>\n"); return 1; }
        return forge_uninstall(&cfg, argv[2]);
    }
    fprintf(stderr, "Forge: unknown command '%s'.\n\n", argv[1]); help(); return 1;
}
