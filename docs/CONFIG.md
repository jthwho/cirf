# CIRF Configuration Reference

This document provides a complete reference for the CIRF JSON configuration file format.

## File Format

Configuration files must be valid JSON. All paths are relative to the configuration file's directory unless otherwise specified.

## Top-Level Structure

```json
{
    "metadata": { ... },
    "entries": [ ... ]
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `metadata` | object | No | Key/value metadata for the root folder |
| `entries` | array | Yes | Array of entry objects |

## Entry Types

### File Entry

Includes a single file in the virtual filesystem.

```json
{
    "type": "file",
    "path": "virtual/path/file.txt",
    "source": "./actual/path/file.txt",
    "mime": "text/plain",
    "metadata": {
        "key": "value"
    }
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | Yes | Must be `"file"` |
| `path` | string | Yes | Virtual path within the filesystem |
| `source` | string | Yes | Path to source file on disk |
| `mime` | string | No | MIME type (auto-detected if omitted) |
| `metadata` | object | No | Key/value metadata pairs |

**Path Handling:**
- Virtual paths use forward slashes regardless of platform
- Source paths are relative to the config file directory
- Virtual paths must not start with `/`
- Virtual paths must not contain `..`

**Generated Symbol Names:**

Virtual paths are converted to C symbol names for direct access:
- Path separators (`/`) become underscores
- Dots and special characters become underscores
- Files: `{name}_file_{path}` (e.g., `config/data.json` → `myres_file_config_data_json`)
- Folders: `{name}_dir_{path}` (e.g., `config` → `myres_dir_config`)

All symbols use the common types from `<cirf/types.h>`:
- Files are `const cirf_file_t * const`
- Folders are `const cirf_folder_t`
- Root is `const cirf_folder_t {name}_root`

### Folder Entry

Creates a folder with optional explicit metadata and nested entries.

```json
{
    "type": "folder",
    "path": "virtual/folder",
    "metadata": {
        "key": "value"
    },
    "entries": [
        { ... }
    ]
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | Yes | Must be `"folder"` |
| `path` | string | Yes | Virtual path for the folder |
| `metadata` | object | No | Key/value metadata pairs |
| `entries` | array | No | Nested file/folder/glob entries |

**Notes:**
- Folders are created implicitly when needed by file paths
- Use explicit folder entries to add metadata to folders
- Nested entry paths are relative to the folder path

### Glob Entry

Matches multiple files using a pattern.

```json
{
    "type": "glob",
    "pattern": "./resources/**/*.png",
    "target": "images/",
    "metadata": {
        "key": "value"
    }
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | Yes | Must be `"glob"` |
| `pattern` | string | Yes | Glob pattern for matching files |
| `target` | string | Yes | Virtual directory for matched files |
| `metadata` | object | No | Metadata applied to all matched files |

**Pattern Syntax:**
- `*` - Match any characters except `/`
- `?` - Match exactly one character
- `**` - Match any number of directories

**Examples:**
```json
{"pattern": "./images/*.png", "target": "textures/"}
{"pattern": "./data/**/*.json", "target": "configs/"}
{"pattern": "./shaders/*.{vert,frag}", "target": "shaders/"}
```

**File Naming:**
- Files are placed in target with their original filename
- Directory structure from `**` patterns is preserved

## Metadata

Metadata consists of string key-value pairs attached to files or folders.

```json
{
    "metadata": {
        "version": "1.0",
        "author": "Name",
        "license": "MIT"
    }
}
```

**Constraints:**
- Keys must be valid C identifiers (alphanumeric and underscore)
- Values are arbitrary strings
- Metadata is accessible at runtime via generated API

## MIME Type Detection

When `mime` is not specified, CIRF detects the type from the file extension:

| Extension | MIME Type |
|-----------|-----------|
| `.txt` | `text/plain` |
| `.html`, `.htm` | `text/html` |
| `.css` | `text/css` |
| `.js` | `application/javascript` |
| `.json` | `application/json` |
| `.xml` | `application/xml` |
| `.png` | `image/png` |
| `.jpg`, `.jpeg` | `image/jpeg` |
| `.gif` | `image/gif` |
| `.svg` | `image/svg+xml` |
| `.ico` | `image/x-icon` |
| `.woff` | `font/woff` |
| `.woff2` | `font/woff2` |
| `.ttf` | `font/ttf` |
| `.otf` | `font/otf` |
| `.wav` | `audio/wav` |
| `.mp3` | `audio/mpeg` |
| `.ogg` | `audio/ogg` |
| `.mp4` | `video/mp4` |
| `.webm` | `video/webm` |
| `.pdf` | `application/pdf` |
| `.zip` | `application/zip` |
| `.gz` | `application/gzip` |
| `.glsl`, `.vert`, `.frag` | `text/plain` |
| `.c`, `.h` | `text/x-c` |
| `.md` | `text/markdown` |
| (unknown) | `application/octet-stream` |

## Complete Example

```json
{
    "metadata": {
        "app_name": "MyGame",
        "version": "2.1.0",
        "build_date": "2024-01-15"
    },
    "entries": [
        {
            "type": "folder",
            "path": "textures",
            "metadata": {
                "category": "graphics"
            },
            "entries": [
                {
                    "type": "file",
                    "path": "player.png",
                    "source": "./assets/sprites/player.png",
                    "metadata": {
                        "width": "64",
                        "height": "64"
                    }
                },
                {
                    "type": "glob",
                    "pattern": "./assets/sprites/enemies/*.png",
                    "target": "enemies/"
                }
            ]
        },
        {
            "type": "folder",
            "path": "audio",
            "metadata": {
                "category": "sound"
            }
        },
        {
            "type": "glob",
            "pattern": "./assets/sounds/**/*.wav",
            "target": "audio/"
        },
        {
            "type": "file",
            "path": "config/defaults.json",
            "source": "./config/game_defaults.json",
            "metadata": {
                "editable": "false"
            }
        },
        {
            "type": "file",
            "path": "shaders/basic.vert",
            "source": "./shaders/basic.vert",
            "mime": "text/x-glsl"
        },
        {
            "type": "file",
            "path": "shaders/basic.frag",
            "source": "./shaders/basic.frag",
            "mime": "text/x-glsl"
        }
    ]
}
```

## Error Handling

CIRF validates configuration files and reports errors for:

- Invalid JSON syntax
- Missing required fields
- Invalid paths (containing `..` or absolute paths)
- Non-existent source files
- Duplicate virtual paths
- Invalid metadata keys

Error messages include the location in the config file when possible.

## Using Generated Resources

### Direct Access (No Runtime Library)

```c
#include "myres.h"

/* Access files directly via generated symbols */
const unsigned char *data = myres_file_textures_player_png->data;
size_t size = myres_file_textures_player_png->size;
const char *mime = myres_file_textures_player_png->mime;
```

### With Runtime Library

```c
#include "myres.h"
#include <cirf/runtime.h>

/* Look up files by path at runtime */
const cirf_file_t *file = cirf_find_file(&myres_root, "textures/player.png");
if (file) {
    process_file(file->data, file->size);
}

/* Get metadata */
const char *version = cirf_get_metadata(myres_root.metadata,
                                         myres_root.metadata_count, "version");
```
