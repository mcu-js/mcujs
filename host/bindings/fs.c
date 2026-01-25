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

/* External helpers from bindings.c */
extern void js_set_function(jerry_value_t object, const char *name, 
                            jerry_external_handler_t handler);
extern void js_register_global(const char *name, jerry_value_t object);
extern size_t js_get_string_arg(const jerry_value_t args[], jerry_length_t argc,
                                jerry_length_t index, char *buffer, size_t buffer_size);

/* Maximum file size we can read into memory */
#define MAX_FILE_SIZE (32 * 1024)

/*
 * Create an error object
 */
static jerry_value_t create_error(const char *code, const char *message) {
    jerry_value_t error = jerry_error_sz(JERRY_ERROR_COMMON, message);
    
    /* Add code property like Node.js errors */
    jerry_value_t code_str = jerry_string_sz(code);
    jerry_value_t code_prop = jerry_string_sz("code");
    jerry_object_set(error, code_prop, code_str);
    jerry_value_free(code_prop);
    jerry_value_free(code_str);
    
    return error;
}

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
    size_t path_len = js_get_string_arg(args, argc, 0, path, sizeof(path));
    if (path_len == 0) {
        return create_error("ERR_INVALID_ARG_TYPE", "path must be a string");
    }
    
    /* Open the file */
    fs_file_t file;
    fs_result_t result = fs_open(&file, path, FS_MODE_READ);
    if (result == FS_ERROR_NOT_FOUND) {
        return create_error("ENOENT", "no such file or directory");
    } else if (result != FS_OK) {
        return create_error("EIO", "failed to open file");
    }
    
    /* Get file size */
    size_t file_size;
    result = fs_size(&file, &file_size);
    if (result != FS_OK) {
        fs_close(&file);
        return create_error("EIO", "failed to get file size");
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
        return create_error("EIO", "failed to read file");
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
    size_t path_len = js_get_string_arg(args, argc, 0, path, sizeof(path));
    if (path_len == 0) {
        return create_error("ERR_INVALID_ARG_TYPE", "path must be a string");
    }
    
    /* Get data argument */
    jerry_value_t str_val = jerry_value_to_string(args[1]);
    if (jerry_value_is_exception(str_val)) {
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
        return create_error("EIO", "failed to open file for writing");
    }
    
    /* Write data */
    size_t bytes_written;
    result = fs_write(&file, data, data_size, &bytes_written);
    fs_close(&file);
    free(data);
    
    if (result != FS_OK) {
        return create_error("EIO", "failed to write file");
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
    size_t path_len = js_get_string_arg(args, argc, 0, path, sizeof(path));
    if (path_len == 0) {
        return create_error("ERR_INVALID_ARG_TYPE", "path must be a string");
    }
    
    /* Get data argument */
    jerry_value_t str_val = jerry_value_to_string(args[1]);
    if (jerry_value_is_exception(str_val)) {
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
        return create_error("EIO", "failed to open file for appending");
    }
    
    /* Write data */
    size_t bytes_written;
    result = fs_write(&file, data, data_size, &bytes_written);
    fs_close(&file);
    free(data);
    
    if (result != FS_OK) {
        return create_error("EIO", "failed to append to file");
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
    size_t path_len = js_get_string_arg(args, argc, 0, path, sizeof(path));
    if (path_len == 0) {
        return jerry_boolean(false);
    }
    
    fs_result_t result = fs_exists(path);
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
    size_t path_len = js_get_string_arg(args, argc, 0, path, sizeof(path));
    if (path_len == 0) {
        return create_error("ERR_INVALID_ARG_TYPE", "path must be a string");
    }
    
    fs_result_t result = fs_remove(path);
    if (result == FS_ERROR_NOT_FOUND) {
        return create_error("ENOENT", "no such file or directory");
    } else if (result != FS_OK) {
        return create_error("EIO", "failed to delete file");
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
    jerry_object_set_index(data->array, data->index++, name);
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
        size_t path_len = js_get_string_arg(args, argc, 0, path, sizeof(path));
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
        return create_error("EIO", "failed to read directory");
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
    
    if (strcmp(entry->name, data->target) == 0) {
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
    size_t path_len = js_get_string_arg(args, argc, 0, path, sizeof(path));
    if (path_len == 0) {
        return create_error("ERR_INVALID_ARG_TYPE", "path must be a string");
    }
    
    /* Extract filename from path */
    const char *filename = path;
    if (path[0] == '/') {
        filename = path + 1;
    }
    
    /* Handle root directory */
    if (filename[0] == '\0' || strcmp(path, "/") == 0) {
        jerry_value_t stat_obj = jerry_object();
        
        jerry_value_t size_prop = jerry_string_sz("size");
        jerry_value_t size_val = jerry_number(0);
        jerry_object_set(stat_obj, size_prop, size_val);
        jerry_value_free(size_prop);
        jerry_value_free(size_val);
        
        jerry_value_t is_dir_prop = jerry_string_sz("isDirectory");
        jerry_value_t is_dir_val = jerry_boolean(true);
        jerry_object_set(stat_obj, is_dir_prop, is_dir_val);
        jerry_value_free(is_dir_prop);
        jerry_value_free(is_dir_val);
        
        jerry_value_t is_file_prop = jerry_string_sz("isFile");
        jerry_value_t is_file_val = jerry_boolean(false);
        jerry_object_set(stat_obj, is_file_prop, is_file_val);
        jerry_value_free(is_file_prop);
        jerry_value_free(is_file_val);
        
        return stat_obj;
    }
    
    /* Look up file in directory */
    stat_data_t data = {
        .target = filename,
        .found = false,
        .size = 0,
        .is_dir = false
    };
    
    fs_list_dir("/", stat_callback, &data);
    
    if (!data.found) {
        return create_error("ENOENT", "no such file or directory");
    }
    
    /* Create stat object */
    jerry_value_t stat_obj = jerry_object();
    
    jerry_value_t size_prop = jerry_string_sz("size");
    jerry_value_t size_val = jerry_number(data.size);
    jerry_object_set(stat_obj, size_prop, size_val);
    jerry_value_free(size_prop);
    jerry_value_free(size_val);
    
    jerry_value_t is_dir_prop = jerry_string_sz("isDirectory");
    jerry_value_t is_dir_val = jerry_boolean(data.is_dir);
    jerry_object_set(stat_obj, is_dir_prop, is_dir_val);
    jerry_value_free(is_dir_prop);
    jerry_value_free(is_dir_val);
    
    jerry_value_t is_file_prop = jerry_string_sz("isFile");
    jerry_value_t is_file_val = jerry_boolean(!data.is_dir);
    jerry_object_set(stat_obj, is_file_prop, is_file_val);
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
    size_t path_len = js_get_string_arg(args, argc, 0, path, sizeof(path));
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
        return create_error("EIO", "failed to create directory");
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
    size_t old_len = js_get_string_arg(args, argc, 0, old_path, sizeof(old_path));
    if (old_len == 0) {
        return create_error("ERR_INVALID_ARG_TYPE", "oldPath must be a string");
    }
    
    /* Get newPath argument */
    char new_path[64];
    size_t new_len = js_get_string_arg(args, argc, 1, new_path, sizeof(new_path));
    if (new_len == 0) {
        return create_error("ERR_INVALID_ARG_TYPE", "newPath must be a string");
    }
    
    fs_result_t result = fs_rename(old_path, new_path);
    if (result == FS_ERROR_NOT_FOUND) {
        return create_error("ENOENT", "no such file or directory");
    } else if (result == FS_ERROR_EXISTS) {
        return create_error("EEXIST", "file already exists");
    } else if (result != FS_OK) {
        return create_error("EIO", "failed to rename file");
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
