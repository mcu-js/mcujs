/*
 * mcujs - Filesystem Bindings
 * 
 * Node.js-compatible synchronous filesystem API:
 *   fs.readFileSync(path[, encoding]) - Read file contents
 *   fs.writeFileSync(path, data)      - Write data to file
 *   fs.appendFileSync(path, data)     - Append data to file
 *   fs.existsSync(path)               - Check if file exists
 *   fs.unlinkSync(path)               - Delete a file
 *   fs.readdirSync(path)              - List directory contents
 *   fs.statSync(path)                 - Get file information
 *   fs.renameSync(oldPath, newPath)   - Rename a file
 *   fs.mkdirSync(path)                - Create a directory
 */

#include "bindings.h"
#include "jerryscript.h"
#include "fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* External helpers from bindings.c */
extern void js_set_function(jerry_value_t object, const char *name, 
                            jerry_external_handler_t handler);
extern void js_register_global(const char *name, jerry_value_t object);

/* Maximum file size we can read into memory */
#define MAX_FILE_SIZE (32 * 1024)
#define PATH_ARG_TOO_LONG ((size_t)-1)

static size_t get_path_arg(const jerry_value_t args[], jerry_length_t argc,
                           jerry_length_t index, char *buffer, size_t buffer_size) {
    if (index >= argc || !jerry_value_is_string(args[index]) || buffer_size == 0) {
        return 0;
    }
    jerry_size_t length = jerry_string_size(args[index], JERRY_ENCODING_UTF8);
    if (length == 0) {
        return 0;
    }
    if (length >= buffer_size) {
        return PATH_ARG_TOO_LONG;
    }
    jerry_size_t copied = jerry_string_to_buffer(
        args[index], JERRY_ENCODING_UTF8, (jerry_char_t *)buffer, length);
    if (copied != length) {
        return PATH_ARG_TOO_LONG;
    }
    buffer[length] = '\0';
    return (size_t)length;
}

static void set_property_value(jerry_value_t object, jerry_value_t property,
                               jerry_value_t value) {
    jerry_value_t result = jerry_object_set(object, property, value);
    jerry_value_free(result);
}

/*
 * Create an error object
 */
static jerry_value_t create_error(const char *code, const char *message) {
    jerry_value_t error = jerry_error_sz(JERRY_ERROR_COMMON, message);
    
    /* Add code property like Node.js errors */
    jerry_value_t code_str = jerry_string_sz(code);
    jerry_value_t code_prop = jerry_string_sz("code");
    set_property_value(error, code_prop, code_str);
    jerry_value_free(code_prop);
    jerry_value_free(code_str);
    
    return jerry_throw_value(error, true);
}

#ifdef MCUJS_PLATFORM_ESP32
static jerry_value_t create_fs_error(fs_result_t result, const char *fallback) {
    if (result == FS_ERROR_BUSY) {
        return create_error("EBUSY",
                            "filesystem is owned by the USB host; eject MCUJS first");
    }
    if (result == FS_ERROR_NO_SPACE) {
        return create_error("ENOSPC", "no space left on device");
    }
    return create_error("EIO", fallback);
}
#define CREATE_FS_ERROR(result, fallback) create_fs_error((result), (fallback))
#else
#define CREATE_FS_ERROR(result, fallback) create_error("EIO", (fallback))
#endif

#define RETURN_IF_PATH_TOO_LONG(length) \
    do { \
        if ((length) == PATH_ARG_TOO_LONG) { \
            return create_error("ENAMETOOLONG", "path is too long"); \
        } \
    } while (0)

/*
 * fs.readFileSync(path[, encoding])
 * Returns: string (if encoding specified) or Buffer-like string
 */
static jerry_value_t fs_read_file_sync(const jerry_call_info_t *call_info_p,
                                        const jerry_value_t args[],
                                        const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 1) {
        return create_error("ERR_INVALID_ARG_TYPE", "path argument is required");
    }
    
    /* Get path argument */
    char path[64];
    size_t path_len = get_path_arg(args, argc, 0, path, sizeof(path));
    RETURN_IF_PATH_TOO_LONG(path_len);
    if (path_len == 0) {
        return create_error("ERR_INVALID_ARG_TYPE", "path must be a string");
    }
    
    /* Open the file */
    fs_file_t file;
    fs_result_t result = fs_open(&file, path, FS_MODE_READ);
    if (result == FS_ERROR_NOT_FOUND) {
        return create_error("ENOENT", "no such file or directory");
    } else if (result != FS_OK) {
        return CREATE_FS_ERROR(result, "failed to open file");
    }
    
    /* Get file size */
    size_t file_size;
    result = fs_size(&file, &file_size);
    if (result != FS_OK) {
        fs_close(&file);
        return CREATE_FS_ERROR(result, "failed to get file size");
    }
    
    if (file_size > MAX_FILE_SIZE) {
        fs_close(&file);
        return create_error("ERR_FS_FILE_TOO_LARGE", "file too large to read");
    }
    
    /* Allocate buffer and read file */
    char *buffer = malloc(file_size + 1);
    if (buffer == NULL) {
        fs_close(&file);
        return create_error("ENOMEM", "out of memory");
    }
    
    size_t bytes_read;
    result = fs_read(&file, buffer, file_size, &bytes_read);
    fs_close(&file);
    
    if (result != FS_OK) {
        free(buffer);
        return CREATE_FS_ERROR(result, "failed to read file");
    }
    
    buffer[bytes_read] = '\0';
    
    /* Return as string - use CESU8 encoding to preserve binary data */
    jerry_value_t str = jerry_string((const jerry_char_t *)buffer, bytes_read, JERRY_ENCODING_CESU8);
    free(buffer);
    
    return str;
}

/*
 * fs.writeFileSync(path, data)
 */
static jerry_value_t fs_write_file_sync(const jerry_call_info_t *call_info_p,
                                         const jerry_value_t args[],
                                         const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 2) {
        return create_error("ERR_INVALID_ARG_TYPE", "path and data arguments are required");
    }
    
    /* Get path argument */
    char path[64];
    size_t path_len = get_path_arg(args, argc, 0, path, sizeof(path));
    RETURN_IF_PATH_TOO_LONG(path_len);
    if (path_len == 0) {
        return create_error("ERR_INVALID_ARG_TYPE", "path must be a string");
    }
    
    /* Get data argument */
    jerry_value_t str_val = jerry_value_to_string(args[1]);
    if (jerry_value_is_exception(str_val)) {
        jerry_value_free(str_val);
        return create_error("ERR_INVALID_ARG_TYPE", "data must be convertible to string");
    }
    
    jerry_size_t data_size = jerry_string_size(str_val, JERRY_ENCODING_UTF8);
    char *data = malloc(data_size + 1);
    if (data == NULL) {
        jerry_value_free(str_val);
        return create_error("ENOMEM", "out of memory");
    }
    
    jerry_string_to_buffer(str_val, JERRY_ENCODING_UTF8, (jerry_char_t *)data, data_size);
    data[data_size] = '\0';
    jerry_value_free(str_val);
    
    /* Open file for writing (create/truncate) */
    fs_file_t file;
    fs_result_t result = fs_open(&file, path, FS_MODE_WRITE | FS_MODE_CREATE | FS_MODE_TRUNCATE);
    if (result == FS_ERROR_NO_SPACE) {
        free(data);
        return create_error("ENOSPC", "no space left on device");
    } else if (result != FS_OK) {
        free(data);
        return CREATE_FS_ERROR(result, "failed to open file for writing");
    }
    
    /* Write data */
    size_t bytes_written;
    result = fs_write(&file, data, data_size, &bytes_written);
    fs_result_t close_result = fs_close(&file);
    if (result == FS_OK && close_result != FS_OK) {
        result = close_result;
    }
    free(data);
    
    if (result == FS_ERROR_NO_SPACE) {
        return create_error("ENOSPC", "no space left on device");
    } else if (result != FS_OK) {
        return CREATE_FS_ERROR(result, "failed to write file");
    }
    
    /* Sync to flash */
    fs_sync();
    
    /* Notify host to refresh */
    fs_notify_host();
    
    return jerry_undefined();
}

/*
 * fs.appendFileSync(path, data)
 */
static jerry_value_t fs_append_file_sync(const jerry_call_info_t *call_info_p,
                                          const jerry_value_t args[],
                                          const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 2) {
        return create_error("ERR_INVALID_ARG_TYPE", "path and data arguments are required");
    }
    
    /* Get path argument */
    char path[64];
    size_t path_len = get_path_arg(args, argc, 0, path, sizeof(path));
    RETURN_IF_PATH_TOO_LONG(path_len);
    if (path_len == 0) {
        return create_error("ERR_INVALID_ARG_TYPE", "path must be a string");
    }
    
    /* Get data argument */
    jerry_value_t str_val = jerry_value_to_string(args[1]);
    if (jerry_value_is_exception(str_val)) {
        jerry_value_free(str_val);
        return create_error("ERR_INVALID_ARG_TYPE", "data must be convertible to string");
    }
    
    jerry_size_t data_size = jerry_string_size(str_val, JERRY_ENCODING_UTF8);
    char *data = malloc(data_size + 1);
    if (data == NULL) {
        jerry_value_free(str_val);
        return create_error("ENOMEM", "out of memory");
    }
    
    jerry_string_to_buffer(str_val, JERRY_ENCODING_UTF8, (jerry_char_t *)data, data_size);
    data[data_size] = '\0';
    jerry_value_free(str_val);
    
    /* Open file for appending */
    fs_file_t file;
    fs_result_t result = fs_open(&file, path, FS_MODE_WRITE | FS_MODE_CREATE | FS_MODE_APPEND);
    if (result == FS_ERROR_NO_SPACE) {
        free(data);
        return create_error("ENOSPC", "no space left on device");
    } else if (result != FS_OK) {
        free(data);
        return CREATE_FS_ERROR(result, "failed to open file for appending");
    }
    
    /* Write data */
    size_t bytes_written;
    result = fs_write(&file, data, data_size, &bytes_written);
    fs_result_t close_result = fs_close(&file);
    if (result == FS_OK && close_result != FS_OK) {
        result = close_result;
    }
    free(data);
    
    if (result == FS_ERROR_NO_SPACE) {
        return create_error("ENOSPC", "no space left on device");
    } else if (result != FS_OK) {
        return CREATE_FS_ERROR(result, "failed to append to file");
    }
    
    /* Sync to flash */
    fs_sync();
    
    /* Notify host to refresh */
    fs_notify_host();
    
    return jerry_undefined();
}

/*
 * fs.existsSync(path)
 * Returns: boolean
 */
static jerry_value_t fs_exists_sync(const jerry_call_info_t *call_info_p,
                                     const jerry_value_t args[],
                                     const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 1) {
        return jerry_boolean(false);
    }
    
    /* Get path argument */
    char path[64];
    size_t path_len = get_path_arg(args, argc, 0, path, sizeof(path));
    RETURN_IF_PATH_TOO_LONG(path_len);
    if (path_len == 0) {
        return jerry_boolean(false);
    }
    
    fs_result_t result = fs_exists(path);
#ifdef MCUJS_PLATFORM_ESP32
    if (result == FS_ERROR_BUSY) {
        return CREATE_FS_ERROR(result, "failed to inspect path");
    }
#endif
    return jerry_boolean(result == FS_OK);
}

/*
 * fs.unlinkSync(path)
 */
static jerry_value_t fs_unlink_sync(const jerry_call_info_t *call_info_p,
                                     const jerry_value_t args[],
                                     const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 1) {
        return create_error("ERR_INVALID_ARG_TYPE", "path argument is required");
    }
    
    /* Get path argument */
    char path[64];
    size_t path_len = get_path_arg(args, argc, 0, path, sizeof(path));
    RETURN_IF_PATH_TOO_LONG(path_len);
    if (path_len == 0) {
        return create_error("ERR_INVALID_ARG_TYPE", "path must be a string");
    }
    
    fs_result_t result = fs_remove(path);
    if (result == FS_ERROR_NOT_FOUND) {
        return create_error("ENOENT", "no such file or directory");
    } else if (result != FS_OK) {
        return CREATE_FS_ERROR(result, "failed to delete file");
    }
    
    /* Sync to flash */
    fs_sync();
    
    /* Notify host to refresh */
    fs_notify_host();
    
    return jerry_undefined();
}

/*
 * Callback data for readdirSync
 */
typedef struct {
    jerry_value_t array;
    uint32_t index;
} readdir_data_t;

/*
 * Callback for directory listing
 */
static bool readdir_callback(const fs_entry_t *entry, void *user_data) {
    readdir_data_t *data = (readdir_data_t *)user_data;
    
    jerry_value_t name = jerry_string_sz(entry->name);
    jerry_value_t set_result = jerry_object_set_index(data->array, data->index++, name);
    jerry_value_free(set_result);
    jerry_value_free(name);
    
    return true;  /* Continue iteration */
}

/*
 * fs.readdirSync(path)
 * Returns: Array of filenames
 */
static jerry_value_t fs_readdir_sync(const jerry_call_info_t *call_info_p,
                                      const jerry_value_t args[],
                                      const jerry_length_t argc) {
    (void)call_info_p;
    
    /* Default to root directory */
    char path[64] = "/";
    
    if (argc >= 1) {
        size_t path_len = get_path_arg(args, argc, 0, path, sizeof(path));
        RETURN_IF_PATH_TOO_LONG(path_len);
        if (path_len == 0) {
            /* Use default "/" */
            strcpy(path, "/");
        }
    }
    
    /* Create result array */
    jerry_value_t array = jerry_array(0);
    
    readdir_data_t data = {
        .array = array,
        .index = 0
    };
    
    fs_result_t result = fs_list_dir(path, readdir_callback, &data);
    if (result == FS_ERROR_NOT_FOUND) {
        jerry_value_free(array);
        return create_error("ENOENT", "no such file or directory");
    } else if (result != FS_OK) {
        jerry_value_free(array);
        return CREATE_FS_ERROR(result, "failed to read directory");
    }
    
    return array;
}

/*
 * Callback data for statSync
 */
typedef struct {
    const char *target;
    bool found;
    uint32_t size;
    bool is_dir;
} stat_data_t;

/*
 * Callback for stat lookup
 */
static bool stat_callback(const fs_entry_t *entry, void *user_data) {
    stat_data_t *data = (stat_data_t *)user_data;
    
    if (strcasecmp(entry->name, data->target) == 0) {
        data->found = true;
        data->size = entry->size;
        data->is_dir = entry->is_dir;
        return false;  /* Stop iteration */
    }
    
    return true;  /* Continue iteration */
}

/*
 * fs.statSync(path)
 * Returns: Object with size and isFile()/isDirectory() methods
 */
static jerry_value_t fs_stat_sync(const jerry_call_info_t *call_info_p,
                                   const jerry_value_t args[],
                                   const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 1) {
        return create_error("ERR_INVALID_ARG_TYPE", "path argument is required");
    }
    
    /* Get path argument */
    char path[64];
    size_t path_len = get_path_arg(args, argc, 0, path, sizeof(path));
    RETURN_IF_PATH_TOO_LONG(path_len);
    if (path_len == 0) {
        return create_error("ERR_INVALID_ARG_TYPE", "path must be a string");
    }
    
    /* Handle root directory */
    if (strcmp(path, "/") == 0) {
#ifdef MCUJS_PLATFORM_ESP32
        fs_result_t root_result = fs_exists("/");
        if (root_result != FS_OK) {
            return CREATE_FS_ERROR(root_result, "failed to stat root directory");
        }
#endif
        jerry_value_t stat_obj = jerry_object();
        
        jerry_value_t size_prop = jerry_string_sz("size");
        jerry_value_t size_val = jerry_number(0);
        set_property_value(stat_obj, size_prop, size_val);
        jerry_value_free(size_prop);
        jerry_value_free(size_val);
        
        jerry_value_t is_dir_prop = jerry_string_sz("isDirectory");
        jerry_value_t is_dir_val = jerry_boolean(true);
        set_property_value(stat_obj, is_dir_prop, is_dir_val);
        jerry_value_free(is_dir_prop);
        jerry_value_free(is_dir_val);
        
        jerry_value_t is_file_prop = jerry_string_sz("isFile");
        jerry_value_t is_file_val = jerry_boolean(false);
        set_property_value(stat_obj, is_file_prop, is_file_val);
        jerry_value_free(is_file_prop);
        jerry_value_free(is_file_val);
        
        return stat_obj;
    }
    
    /* Split the path into the containing directory and basename. */
    size_t normalized_len = strlen(path);
    while (normalized_len > 1 && path[normalized_len - 1] == '/') {
        path[--normalized_len] = '\0';
    }

    char directory[64] = "/";
    const char *filename = path;
    char *last_slash = strrchr(path, '/');
    if (last_slash != NULL) {
        filename = last_slash + 1;
        if (last_slash != path) {
            size_t directory_len = (size_t)(last_slash - path);
            if (directory_len >= sizeof(directory)) {
                return create_error("ENAMETOOLONG", "path is too long");
            }
            memcpy(directory, path, directory_len);
            directory[directory_len] = '\0';
        }
    }
    if (filename[0] == '\0') {
        return create_error("ENOENT", "no such file or directory");
    }

    /* Look up the basename in its containing directory. */
    stat_data_t data = {
        .target = filename,
        .found = false,
        .size = 0,
        .is_dir = false
    };
    
    fs_result_t list_result = fs_list_dir(directory, stat_callback, &data);
    if (list_result == FS_ERROR_NOT_FOUND) {
        return create_error("ENOENT", "no such file or directory");
    } else if (list_result != FS_OK) {
        return CREATE_FS_ERROR(list_result, "failed to stat file");
    }
    
    if (!data.found) {
        return create_error("ENOENT", "no such file or directory");
    }
    
    /* Create stat object */
    jerry_value_t stat_obj = jerry_object();
    
    jerry_value_t size_prop = jerry_string_sz("size");
    jerry_value_t size_val = jerry_number(data.size);
    set_property_value(stat_obj, size_prop, size_val);
    jerry_value_free(size_prop);
    jerry_value_free(size_val);
    
    jerry_value_t is_dir_prop = jerry_string_sz("isDirectory");
    jerry_value_t is_dir_val = jerry_boolean(data.is_dir);
    set_property_value(stat_obj, is_dir_prop, is_dir_val);
    jerry_value_free(is_dir_prop);
    jerry_value_free(is_dir_val);
    
    jerry_value_t is_file_prop = jerry_string_sz("isFile");
    jerry_value_t is_file_val = jerry_boolean(!data.is_dir);
    set_property_value(stat_obj, is_file_prop, is_file_val);
    jerry_value_free(is_file_prop);
    jerry_value_free(is_file_val);
    
    return stat_obj;
}

/*
 * fs.mkdirSync(path)
 */
static jerry_value_t fs_mkdir_sync(const jerry_call_info_t *call_info_p,
                                    const jerry_value_t args[],
                                    const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 1) {
        return create_error("ERR_INVALID_ARG_TYPE", "path argument is required");
    }
    
    /* Get path argument */
    char path[64];
    size_t path_len = get_path_arg(args, argc, 0, path, sizeof(path));
    RETURN_IF_PATH_TOO_LONG(path_len);
    if (path_len == 0) {
        return create_error("ERR_INVALID_ARG_TYPE", "path must be a string");
    }
    
    fs_result_t result = fs_mkdir(path);
    if (result == FS_ERROR_EXISTS) {
        return create_error("EEXIST", "file already exists");
    } else if (result == FS_ERROR_NOT_FOUND) {
        return create_error("ENOENT", "no such file or directory");
    } else if (result == FS_ERROR_NO_SPACE) {
        return create_error("ENOSPC", "no space left on device");
    } else if (result != FS_OK) {
        return CREATE_FS_ERROR(result, "failed to create directory");
    }
    
    /* Sync to flash */
    fs_sync();
    
    /* Notify host to refresh */
    fs_notify_host();
    
    return jerry_undefined();
}

/*
 * fs.renameSync(oldPath, newPath)
 */
static jerry_value_t fs_rename_sync(const jerry_call_info_t *call_info_p,
                                     const jerry_value_t args[],
                                     const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 2) {
        return create_error("ERR_INVALID_ARG_TYPE", "oldPath and newPath arguments are required");
    }
    
    /* Get oldPath argument */
    char old_path[64];
    size_t old_len = get_path_arg(args, argc, 0, old_path, sizeof(old_path));
    RETURN_IF_PATH_TOO_LONG(old_len);
    if (old_len == 0) {
        return create_error("ERR_INVALID_ARG_TYPE", "oldPath must be a string");
    }
    
    /* Get newPath argument */
    char new_path[64];
    size_t new_len = get_path_arg(args, argc, 1, new_path, sizeof(new_path));
    RETURN_IF_PATH_TOO_LONG(new_len);
    if (new_len == 0) {
        return create_error("ERR_INVALID_ARG_TYPE", "newPath must be a string");
    }
    
    fs_result_t result = fs_rename(old_path, new_path);
    if (result == FS_ERROR_NOT_FOUND) {
        return create_error("ENOENT", "no such file or directory");
    } else if (result == FS_ERROR_EXISTS) {
        return create_error("EEXIST", "file already exists");
    } else if (result != FS_OK) {
        return CREATE_FS_ERROR(result, "failed to rename file");
    }
    
    /* Sync to flash */
    fs_sync();
    
    /* Notify host to refresh */
    fs_notify_host();
    
    return jerry_undefined();
}

/*
 * Create filesystem module object
 */
jerry_value_t js_create_fs_module(void) {
    jerry_value_t fs = jerry_object();

    js_set_function(fs, "readFileSync", fs_read_file_sync);
    js_set_function(fs, "writeFileSync", fs_write_file_sync);
    js_set_function(fs, "appendFileSync", fs_append_file_sync);
    js_set_function(fs, "existsSync", fs_exists_sync);
    js_set_function(fs, "unlinkSync", fs_unlink_sync);
    js_set_function(fs, "readdirSync", fs_readdir_sync);
    js_set_function(fs, "statSync", fs_stat_sync);
    js_set_function(fs, "renameSync", fs_rename_sync);
    js_set_function(fs, "mkdirSync", fs_mkdir_sync);

    return fs;
}

/*
 * Register filesystem bindings
 */
void js_bind_fs(void) {
    jerry_value_t fs = js_create_fs_module();
    js_register_global("fs", fs);
    jerry_value_free(fs);
}
