/* The one place PadBridge PS5's version is set. Every ELF carries it in its name
 * (dist/PadBridge-PS5-<version>.elf), in its log's first line, in the console
 * notification and on the web page. Raise it with every build that leaves
 * this folder, and write what changed in CHANGELOG.md. */
#ifndef PADBRIDGE_VERSION_H
#define PADBRIDGE_VERSION_H

/* One #define line only: ps5.mk reads the version from it. */
#ifndef PADBRIDGE_VERSION
#define PADBRIDGE_VERSION "0.2.0-beta"
#endif

#endif
