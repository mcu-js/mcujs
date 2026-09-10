#include "bindings.h"
#include "fs.h"
#include "runtime_features.h"
#include "runtime_registry.h"
#include "events_test_source.h"
#include "fs_binary_test_source.h"
#include <unistd.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Production console formatting; only the physical transport is stubbed. */
size_t usb_cdc_write(const char *data, size_t length) { return fwrite(data, 1, length, stdout); }

static jerry_value_t stub_module(void) {
    return jerry_object();
}

static jerry_value_t stub_handler(const jerry_call_info_t *call_info,
                                  const jerry_value_t args[],
                                  jerry_length_t argc);

jerry_value_t js_create_spi_module(void) { return stub_module(); }
jerry_value_t js_create_adc_module(void) { return stub_module(); }
#if MCUJS_FEATURE_IMAGE
jerry_value_t js_create_image_module(void) {
    jerry_value_t module = stub_module();
    js_set_function(module, "info", stub_handler);
    js_set_function(module, "decodeJPEG", stub_handler);
    js_set_function(module, "decodeBMP", stub_handler);
    js_set_function(module, "drawJPEG", stub_handler);
    js_set_function(module, "drawBMP", stub_handler);
    return module;
}
#endif
#if MCUJS_FEATURE_KEYBOARD
jerry_value_t js_create_keyboard_module(void) { return stub_module(); }
#endif
#if MCUJS_FEATURE_MOUSE
jerry_value_t js_create_mouse_module(void) { return stub_module(); }
#endif

static fs_result_t s_fs_operation_result = FS_ERROR_NOT_FOUND;
static const struct { const char *path, *source; } app_files[] = {
    {"/app/settings.json", "{\"color\":\"white\"}"},
    {"/app/lib/helper.js", "module.exports={ok:true};"},
    {"/app/nested/main.js", "exports.filename=__filename;exports.dirname=__dirname;exports.later=function(){return require('../settings.json').color;};"},
    {"/app/index.js", "globalThis.appRuns=(globalThis.appRuns||0)+1;globalThis.appEntry=require('./nested/main');"},
};
static const char *app_source(const char *path) {
    for (size_t i=0;i<sizeof(app_files)/sizeof(app_files[0]);i++)
        if (!strcmp(path,app_files[i].path)) return app_files[i].source;
    return NULL;
}

/* Real temporary binary storage behind the existing backend seam; failures can
 * be injected without relying on an SD card or changing production backends. */
static char s_binary_path[] = "/tmp/mcujs-binary-XXXXXX";
static unsigned s_open_files, s_io_calls;
static size_t s_largest_transfer;
static fs_result_t s_io_result = FS_OK, s_close_result = FS_OK;
static bool s_short_write;
static unsigned s_seek_fail_after;

fs_result_t fs_open(fs_file_t *file, const char *path, fs_mode_t mode) {
    if (s_fs_operation_result != FS_OK) return s_fs_operation_result;
    FILE *stream;
    if (!strcmp(path, "/app/binary.bin") || !strcmp(path, "/sd/binary.bin")) {
        stream = fopen(s_binary_path, mode & FS_MODE_TRUNCATE ? "wb+" : "rb");
    } else {
        const char *source = app_source(path);
        if (!source) return FS_ERROR_NOT_FOUND;
        stream = tmpfile();
        assert(stream);
        assert(fwrite(source, 1, strlen(source), stream) == strlen(source));
        rewind(stream);
    }
    if (!stream) return FS_ERROR_IO;
    file->internal = stream;
    file->is_open = true;
    s_open_files++;
    return FS_OK;
}

fs_result_t fs_close(fs_file_t *file) {
    assert(file->is_open && file->internal && s_open_files);
    assert(fclose(file->internal) == 0);
    file->internal = NULL;
    file->is_open = false;
    s_open_files--;
    return s_close_result;
}

fs_result_t fs_read(fs_file_t *file, void *buffer, size_t size, size_t *bytes_read) {
    s_io_calls++;
    if (size > s_largest_transfer) s_largest_transfer = size;
    *bytes_read = 0;
    if (s_fs_operation_result != FS_OK) return s_fs_operation_result;
    if (s_io_result != FS_OK) return s_io_result;
    *bytes_read = fread(buffer, 1, size, file->internal);
    return ferror(file->internal) ? FS_ERROR_IO : FS_OK;
}

fs_result_t fs_write(fs_file_t *file, const void *buffer, size_t size,
                     size_t *bytes_written) {
    s_io_calls++;
    if (size > s_largest_transfer) s_largest_transfer = size;
    *bytes_written = 0;
    if (s_fs_operation_result != FS_OK) return s_fs_operation_result;
    if (s_io_result != FS_OK) return s_io_result;
    *bytes_written = fwrite(buffer, 1, s_short_write && size ? size - 1 : size, file->internal);
    return ferror(file->internal) ? FS_ERROR_IO : FS_OK;
}

fs_result_t fs_seek(fs_file_t *file, uint32_t offset) {
    if (s_fs_operation_result != FS_OK) return s_fs_operation_result;
    if (s_seek_fail_after && --s_seek_fail_after == 0) return FS_ERROR_IO;
    return fseek(file->internal, offset, SEEK_SET) == 0 ? FS_OK : FS_ERROR_IO;
}

fs_result_t fs_size(fs_file_t *file, size_t *size) {
    if (s_fs_operation_result != FS_OK) return s_fs_operation_result;
    FILE *stream = file->internal;
    long position = ftell(stream);
    assert(position >= 0 && fseek(stream, 0, SEEK_END) == 0);
    long end = ftell(stream);
    assert(end >= 0 && fseek(stream, position, SEEK_SET) == 0);
    *size = (size_t)end;
    return FS_OK;
}

fs_result_t fs_exists(const char *path) {
    if (s_fs_operation_result==FS_OK) return app_source(path)?FS_OK:FS_ERROR_NOT_FOUND;
    return s_fs_operation_result;
}

fs_result_t fs_remove(const char *path) {
    (void)path;
    return s_fs_operation_result;
}

fs_result_t fs_rename(const char *old_path, const char *new_path) {
    (void)old_path;
    (void)new_path;
    return s_fs_operation_result;
}

fs_result_t fs_mkdir(const char *path) {
    (void)path;
    return s_fs_operation_result;
}

fs_result_t fs_list_dir(const char *path, fs_dir_callback_t callback,
                        void *user_data) {
    (void)path;
    (void)callback;
    (void)user_data;
    return s_fs_operation_result;
}

fs_result_t fs_sync(void) {
    return s_fs_operation_result == FS_ERROR_BUSY ? FS_ERROR_BUSY : FS_OK;
}

void fs_notify_host(void) {}

fs_result_t fs_access_status(void) {
    return s_fs_operation_result == FS_ERROR_BUSY ? FS_ERROR_BUSY : FS_OK;
}

bool fs_storage_ready(void) {
    return fs_access_status() == FS_OK;
}

void js_module_free_file(char *content) {
    free(content);
}

static jerry_value_t stub_handler(const jerry_call_info_t *call_info,
                                  const jerry_value_t args[],
                                  jerry_length_t argc) {
    (void)call_info;
    (void)args;
    (void)argc;
    return jerry_undefined();
}

static void print_exception(jerry_value_t exception);

static size_t heap_used(void) {
    jerry_heap_gc(JERRY_GC_PRESSURE_HIGH);
    jerry_heap_stats_t stats;
    assert(jerry_heap_stats(&stats));
    return stats.allocated_bytes;
}

static bool eval_source(const char *source) {
    jerry_value_t result = jerry_eval((const jerry_char_t *)source,
                                      strlen(source), JERRY_PARSE_NO_OPTS);
    if (jerry_value_is_exception(result)) {
        print_exception(result);
        return false;
    }
    jerry_value_free(result);
    return true;
}

static void print_exception(jerry_value_t exception) {
    jerry_value_t value = jerry_exception_value(exception, true);
    jerry_value_t text = jerry_value_to_string(value);
    jerry_size_t size = jerry_string_size(text, JERRY_ENCODING_UTF8);
    char *buffer = malloc(size + 1);
    if (buffer != NULL) {
        jerry_string_to_buffer(text, JERRY_ENCODING_UTF8,
                               (jerry_char_t *)buffer, size);
        buffer[size] = '\0';
        fprintf(stderr, "binding test exception: %s\n", buffer);
        free(buffer);
    }
    jerry_value_free(text);
    jerry_value_free(value);
}

static const char s_test_source[] =
    "(function () {\n"
    "  function assert(condition, message) {\n"
    "    if (!condition) throw new Error(message);\n"
    "  }\n"
    "  function assertFrozenTree(value, path) {\n"
    "    if (value === null || typeof value !== 'object') return;\n"
    "    assert(Object.isFrozen(value), path + ' is not frozen');\n"
    "    Object.keys(value).forEach(function (key) {\n"
    "      assertFrozenTree(value[key], path + '.' + key);\n"
    "    });\n"
    "  }\n"
    "  function attackFrozenTree(value, path) {\n"
    "    if (value === null || typeof value !== 'object') return;\n"
    "    var before = JSON.stringify(value);\n"
    "    try { value.__mcujsMutation = true; } catch (error) {}\n"
    "    var keys = Object.keys(value);\n"
    "    if (keys.length > 0) {\n"
    "      var key = keys[0];\n"
    "      var child = value[key];\n"
    "      try { value[key] = '__mcujsReplacement'; } catch (error) {}\n"
    "      try { delete value[key]; } catch (error) {}\n"
    "      assert(value[key] === child, path + '.' + key + ' changed');\n"
    "      attackFrozenTree(child, path + '.' + key);\n"
    "    }\n"
    "    if (Array.isArray(value)) {\n"
    "      try { value.push('__mcujsAppend'); } catch (error) {}\n"
    "    }\n"
    "    assert(JSON.stringify(value) === before, path + ' mutation persisted');\n"
    "  }\n"
    "  function assertImmutableProperty(object, name, path) {\n"
    "    var descriptor = Object.getOwnPropertyDescriptor(object, name);\n"
    "    assert(descriptor !== undefined, path + ' descriptor missing');\n"
    "    assert(descriptor.writable === false, path + ' is writable');\n"
    "    assert(descriptor.configurable === false, path + ' is configurable');\n"
    "    assert(descriptor.enumerable === true, path + ' is not enumerable');\n"
    "    var original = object[name];\n"
    "    try { object[name] = '__mcujsReplacement'; } catch (error) {}\n"
    "    try { delete object[name]; } catch (error) {}\n"
    "    assert(object[name] === original, path + ' replacement/deletion persisted');\n"
    "  }\n"
    "  function assertThrowsType(call, constructor, message) {\n"
    "    var thrown;\n"
    "    try { call(); } catch (error) { thrown = error; }\n"
    "    assert(thrown instanceof constructor, message);\n"
    "  }\n"
    "\n"
    "  var modules = require('mcujs:module');\n"
    "  assert(modules === require('node:module'), 'module alias diverged');\n"
    "  assertImmutableProperty(modules, 'builtinModules', 'modules.builtinModules');\n"
    "  assertFrozenTree(modules.builtinModules, 'modules.builtinModules');\n"
    "  attackFrozenTree(modules.builtinModules, 'modules.builtinModules');\n"
    "  assert(modules.has('board') === true, 'board missing from has()');\n"
    "  assert(modules.has('__missing_module__') === false, 'unknown module present');\n"
    "  assert(modules.has('image') === (__mcujsHasImage === true), 'feature lane mismatch');\n"
    "  assert(modules.has('keyboard') === (__mcujsHasKeyboard === true), 'keyboard feature lane mismatch');\n"
    "  assert(modules.has('mouse') === (__mcujsHasMouse === true), 'mouse feature lane mismatch');\n"
    "  assertThrowsType(function () { modules.has(); }, TypeError, 'missing has() argument did not throw TypeError');\n"
    "  assertThrowsType(function () { modules.has(1); }, TypeError, 'numeric has() argument did not throw TypeError');\n"
    "  assertThrowsType(function () { modules.has(null); }, TypeError, 'null has() argument did not throw TypeError');\n"
    "  assertThrowsType(function () { modules.has(''); }, RangeError, 'empty has() name did not throw RangeError');\n"
    "  assertThrowsType(function () { modules.has(Array(129).join('x')); }, RangeError, 'oversized has() name did not throw RangeError');\n"
    "  assertThrowsType(function () { modules.has('fs\\x00shadow'); }, RangeError, 'null-byte has() name did not throw RangeError');\n"
    "  modules.builtinModules.forEach(function (name) {\n"
    "    assert(modules.has(name) === true, name + ' missing from has()');\n"
    "    assert(require(name) !== undefined, name + ' cannot be required');\n"
    "  });\n"
    "  var knownModules = ['board', 'fs', 'process', 'gpio', 'pwm', 'i2c', 'spi', 'adc', 'neopixel', 'image', 'keyboard', 'mouse', 'graphics', 'screen', 'dvi', 'display', 'mcujs:module', 'node:module'];\n"
    "  knownModules.forEach(function (name) {\n"
    "    var listed = modules.builtinModules.indexOf(name) !== -1;\n"
    "    assert(modules.has(name) === listed, name + ' has()/list mismatch');\n"
    "    if (!listed) assertThrowsType(function () { require(name); }, Error, name + ' absent module was require-able');\n"
    "  });\n"
    "  var productionExports = {\n"
    "    gpio: ['OUTPUT', 'INPUT', 'INPUT_PULLUP', 'INPUT_PULLDOWN', 'init', 'set', 'get', 'toggle'],\n"
    "    pwm: ['init', 'setDuty', 'stop'],\n"
    "    i2c: ['init', 'write', 'read'],\n"
    "    neopixel: ['init', 'setPixel', 'show', 'clear'],\n"
    "    image: ['info', 'decodeJPEG', 'decodeBMP', 'drawJPEG', 'drawBMP'],\n"
    "    graphics: ['createBuffer', 'freeBuffer', 'getBufferInfo', 'getPointer', 'fill', 'setPixel', 'fillRect', 'color565'],\n"
    "    screen: ['init', 'fill', 'setPixel', 'fillRect', 'drawLine', 'drawCircle', 'fillCircle', 'drawText', 'rgb', 'color', 'show', 'getWidth', 'getHeight', 'getBufferHandle', 'getByteOrder', 'BLACK', 'WHITE', 'RED', 'GREEN', 'BLUE', 'CYAN', 'MAGENTA', 'YELLOW', 'ORANGE', 'GRAY']\n"
    "  };\n"
    "  Object.keys(productionExports).forEach(function (name) {\n"
    "    if (!modules.has(name)) return;\n"
    "    var actual = Object.keys(require(name)).sort();\n"
    "    var expected = productionExports[name].slice().sort();\n"
    "    assert(JSON.stringify(actual) === JSON.stringify(expected), name + ' production factory exports mismatch');\n"
    "  });\n"
    "  assert(modules.builtinModules.indexOf('image') !== -1 === (__mcujsHasImage === true), 'image list mismatch');\n"
    "  if (!__mcujsHasImage) {\n"
    "    assertThrowsType(function () { require('image'); }, Error, 'absent image module was require-able');\n"
    "  }\n"
    "\n"
    "  var board = require('board');\n"
    "  assert(board === globalThis.board, 'canonical board module is not the global compatibility alias');\n"
    "  assert(board === require('board'), 'canonical board module identity is unstable');\n"
    "  assert(board.name === __mcujsExpectedBoard.name, 'board name diverged from registry');\n"
    "  assert(board.chip === __mcujsExpectedBoard.chip, 'board chip diverged from registry');\n"
    "  assert(board.version === __mcujsExpectedBoard.firmwareVersion, 'board firmware version diverged from registry');\n"
    "  assert(board.apiVersion === '0.2', 'board API version mismatch');\n"
    "  assert(('safeMode' in board) === __mcujsExpectedSafeMode, 'safeMode availability mismatch');\n"
    "  assert(('storageReady' in board) === __mcujsExpectedStorageReady, 'storageReady availability mismatch');\n"
    "  ['name', 'chip', 'version', 'apiVersion', 'exposedPins', 'pins', 'devices'].forEach(function (name) {\n"
    "    assertImmutableProperty(board, name, 'board.' + name);\n"
    "  });\n"
    "  assertFrozenTree(board.exposedPins, 'board.exposedPins');\n"
    "  assertFrozenTree(board.pins, 'board.pins');\n"
    "  assertFrozenTree(board.devices, 'board.devices');\n"
    "  attackFrozenTree(board.exposedPins, 'board.exposedPins');\n"
    "  attackFrozenTree(board.pins, 'board.pins');\n"
    "  attackFrozenTree(board.devices, 'board.devices');\n"
    "  assert(JSON.stringify(board.pins) === JSON.stringify(__mcujsExpectedBoard.pins), 'semantic pin aliases diverged from registry');\n"
    "  assert(JSON.stringify(board.devices) === JSON.stringify(__mcujsExpectedBoard.devices), 'physical device inventory diverged from registry');\n"
    "  var gpioCapability = board.capability('gpio');\n"
    "  assertFrozenTree(gpioCapability, 'board.capability(\\'gpio\\')');\n"
    "  attackFrozenTree(gpioCapability, 'board.capability(\\'gpio\\')');\n"
    "  var capabilities = board.capabilities();\n"
    "  assertFrozenTree(capabilities, 'board.capabilities()');\n"
    "  attackFrozenTree(capabilities, 'board.capabilities()');\n"
    "  assert(JSON.stringify(capabilities.gpio) === JSON.stringify(gpioCapability), 'singular and snapshot capabilities diverged');\n"
    "  var moduleCapabilities = {fs: 'fs', gpio: 'gpio', pwm: 'pwm', i2c: 'i2c', spi: 'spi', adc: 'adc', neopixel: 'neopixel', image: 'image', graphics: 'graphics', screen: 'screen', dvi: 'dvi'};\n"
    "  Object.keys(moduleCapabilities).forEach(function (name) {\n"
    "    var capabilityName = moduleCapabilities[name];\n"
    "    assert(modules.has(name) === (board.capability(capabilityName) !== undefined), name + ' module/capability mismatch');\n"
    "    assert((capabilityName in capabilities) === modules.has(name), name + ' snapshot capability mismatch');\n"
    "  });\n"
    "  assert(board.capability('__missing_capability__') === undefined, 'unknown capability is present');\n"
    "  assertThrowsType(function () { board.capability(); }, TypeError, 'missing capability argument did not throw TypeError');\n"
    "  assertThrowsType(function () { board.capability(1); }, TypeError, 'numeric capability argument did not throw TypeError');\n"
    "  assertThrowsType(function () { board.capability(''); }, RangeError, 'empty capability name did not throw RangeError');\n"
    "  assertThrowsType(function () { board.capability(Array(33).join('x')); }, RangeError, 'oversized capability name did not throw RangeError');\n"
    "  assertThrowsType(function () { board.capability('gpio\\x00shadow'); }, RangeError, 'null-byte capability name did not throw RangeError');\n"
    "})();\n";

static const char s_busy_test_source[] =
    "(function () {\n"
    "  function assert(condition, message) { if (!condition) throw new Error(message); }\n"
    "  function expectBusy(call, label) {\n"
    "    var error; try { call(); } catch (caught) { error = caught; }\n"
    "    assert(error && error.name === 'ResourceBusyError', label + ' name');\n"
    "    assert(error.code === 'EBUSY', label + ' code');\n"
    "    assert(error.resource === 'filesystem', label + ' resource');\n"
    "    assert(error.owner === 'usb-host', label + ' owner');\n"
    "  }\n"
    "  var filesystem = require('fs');\n"
    "  expectBusy(function () { filesystem.readFileSync('/busy.js'); }, 'readFileSync');\n"
    "  expectBusy(function () { filesystem.writeFileSync('/busy.js', 'x'); }, 'writeFileSync');\n"
    "  expectBusy(function () { filesystem.appendFileSync('/busy.js', 'x'); }, 'appendFileSync');\n"
    "  expectBusy(function () { filesystem.existsSync('/busy.js'); }, 'existsSync');\n"
    "  expectBusy(function () { filesystem.unlinkSync('/busy.js'); }, 'unlinkSync');\n"
    "  expectBusy(function () { filesystem.readdirSync('/'); }, 'readdirSync');\n"
    "  expectBusy(function () { filesystem.statSync('/'); }, 'statSync');\n"
    "  expectBusy(function () { filesystem.renameSync('/busy.js', '/still-busy.js'); }, 'renameSync');\n"
    "  expectBusy(function () { filesystem.mkdirSync('/busy'); }, 'mkdirSync');\n"
    "  expectBusy(function () { require('./busy-module'); }, 'file-backed require');\n"
    "})();\n";

int main(void) {
    jerry_init(JERRY_INIT_EMPTY);

    const mcujs_runtime_registry_t *registry = mcujs_runtime_registry();
    jerry_value_t board = jerry_object();
    jerry_external_handler_t safe_mode = registry->safe_mode ? stub_handler : NULL;
    jerry_external_handler_t storage_ready = registry->storage_ready ? stub_handler : NULL;
    assert(js_board_apply_registry(board, safe_mode, storage_ready));
    js_register_global("board", board);
    jerry_value_free(board);

    jerry_value_t global = jerry_current_realm();
    jerry_value_t has_image = jerry_boolean(MCUJS_FEATURE_IMAGE != 0);
    js_set_property(global, "__mcujsHasImage", has_image);
    jerry_value_free(has_image);
    jerry_value_t has_keyboard = jerry_boolean(MCUJS_FEATURE_KEYBOARD != 0);
    js_set_property(global, "__mcujsHasKeyboard", has_keyboard);
    jerry_value_free(has_keyboard);
    jerry_value_t has_mouse = jerry_boolean(MCUJS_FEATURE_MOUSE != 0);
    js_set_property(global, "__mcujsHasMouse", has_mouse);
    jerry_value_free(has_mouse);
    jerry_value_t expected_board = jerry_json_parse(
        (const jerry_char_t *)registry->board_json, strlen(registry->board_json));
    assert(!jerry_value_is_exception(expected_board));
    js_set_property(global, "__mcujsExpectedBoard", expected_board);
    jerry_value_free(expected_board);
    jerry_value_t expected_safe_mode = jerry_boolean(registry->safe_mode);
    js_set_property(global, "__mcujsExpectedSafeMode", expected_safe_mode);
    jerry_value_free(expected_safe_mode);
    jerry_value_t expected_storage_ready = jerry_boolean(registry->storage_ready);
    js_set_property(global, "__mcujsExpectedStorageReady", expected_storage_ready);
    jerry_value_free(expected_storage_ready);
    jerry_value_t process = jerry_object();
    js_set_property(global, "process", process);
    jerry_value_free(process);
    jerry_value_free(global);

    js_bind_require();
    js_bind_console();
    assert(eval_source("if (!require('mcujs:module').has('events')) throw new Error('events module missing');"));
    assert(eval_source("if (!require('mcujs:module').has('devices') || require('devices') !== require('devices') || !Object.isFrozen(require('devices')) || require('devices').display !== undefined) throw new Error('device discovery contract');"));
    assert(eval_source("if (board.devices.button) { if (!require('devices').button || require('mcujs:button').getState() !== 'idle') throw Error('button discovery'); } else { if (require('devices').button !== undefined) throw Error('unsupported button'); var missing=false; try { require('mcujs:button'); } catch(e) { missing=true; } if (!missing) throw Error('private button leaked'); }"));

    size_t baseline = heap_used();
    assert(eval_source("globalThis.__oneCapability = board.capability('usb');"));
    size_t singular = heap_used();
    assert(eval_source("globalThis.__allCapabilities = board.capabilities();"));
    size_t complete = heap_used();
    assert(singular > baseline);
    assert(complete > singular);
    printf("runtime Jerry binding heap for %s: base=%zu one=%zu all=%zu\n",
           registry->board_id, baseline, singular, complete);

    assert(eval_source("require('events') === require('events') || (() => { throw new Error('events identity'); })();"));
    size_t event_heap = heap_used();
    assert(eval_source(events_test_source));
    size_t after_events = heap_used();
    assert(eval_source(events_test_source));
    size_t repeated_events = heap_used();
    printf("events shared contract: PASS (12 tests twice); heap=%zu/%zu/%zu\n", event_heap, after_events, repeated_events);
    fflush(stdout);
    assert(repeated_events <= after_events + 256);
    for (unsigned repeat = 0; repeat < 8; repeat++) {
        assert(eval_source(events_test_source));
        assert(heap_used() <= after_events + 256);
    }
    puts("events repeated workload: PASS (10 runs, bounded retained heap)");
    assert(eval_source("var cancelled; (function(){var E=require('events'), c=new E.AbortController(); new Promise(function(resolve,reject){c.signal.addEventListener('abort',function(){reject(c.signal.reason);},{once:true});}).catch(function(reason){cancelled=reason;}); c.abort('cancelled');})();"));
    jerry_value_t jobs = jerry_run_jobs();
    assert(!jerry_value_is_exception(jobs));
    jerry_value_free(jobs);
    assert(eval_source("if(cancelled !== 'cancelled') throw new Error('abort rejection not completed');"));
    assert(eval_source(s_test_source));
    s_fs_operation_result = FS_OK;
    assert(eval_source("(function(){var f=require('fs');var fd=f.openSync('/app/settings.json','r');if(typeof fd!=='number')throw Error('numeric file handle');f.closeSync(fd);})()"));
    puts("binary handles open/close: PASS");
    int temporary = mkstemp(s_binary_path);
    assert(temporary >= 0 && close(temporary) == 0);
    assert(eval_source(fs_binary_test_source));
    assert(s_open_files == 0);
    s_short_write=true;
    assert(eval_source("(function(){var f=require('fs'),h=f.openSync('/app/binary.bin','w');try{f.writeSync(h,new Uint8Array([0,128,255,1]),0,4);throw Error('short write accepted');}catch(e){if(e.code!=='ENOSPC'||e.bytesWritten!==3)throw e;}finally{f.closeSync(h);}})()"));
    s_short_write=false;
    assert(eval_source("var abandoned=require('fs').openSync('/app/binary.bin','r');"));
    assert(s_open_files==1);js_fs_cleanup();assert(s_open_files==0);
    assert(eval_source("try{require('fs').readSync(abandoned,new Uint8Array(1),0,1);throw Error('stale cleanup handle');}catch(e){if(e.code!=='EBADF')throw e;}"));
    assert(unlink(s_binary_path) == 0);
    puts("binary handles roundtrip: PASS");
    assert(eval_source("(function(){var f=require('fs'),fd=f.openSync('/app/settings.json','r'),b=new Uint8Array(4);if(f.readSync(fd,b,1,2)!==2||b[0]!==0||b[1]!==123||b[2]!==34||b[3]!==0)throw Error('binary read count and slice');f.closeSync(fd);})()"));
    assert(eval_source("var appMain=require('./nested/main');if(appMain.filename!=='/app/nested/main.js'||appMain.dirname!=='/app/nested'||appMain.later()!=='white')throw Error('module-relative app paths');"));
    assert(eval_source("if(require('/app/nested/./main.js')!==appMain||require('/app/nested/../nested/main')!==appMain||!require('helper').ok)throw Error('canonical app cache and library');"));
    assert(eval_source("['/index.js','/sd/settings.json','../settings.json','/app/../app/settings.json'].forEach(function(p){var denied=false;try{require(p);}catch(e){denied=true;}if(!denied)throw Error('namespace escape accepted');});"));
    jerry_value_t entry=js_require_exec_file("/app/index.js");
    assert(!jerry_value_is_exception(entry));jerry_value_free(entry);
    entry=js_require_exec_file("index.js");
    assert(!jerry_value_is_exception(entry));jerry_value_free(entry);
    assert(eval_source("if(appRuns!==2||appEntry.later()!=='white'||appEntry.filename!=='/app/nested/main.js')throw Error('entry rerun and callback import');"));
    js_require_clear_cache();
    const struct { fs_result_t result; const char *code; } media_errors[] = {
        {FS_ERROR_NO_MEDIA,"ENOMEDIUM"}, {FS_ERROR_UNSUPPORTED,"ENOTSUP"},
        {FS_ERROR_READ_ONLY,"EROFS"}, {FS_ERROR_CROSS_DEVICE,"EXDEV"},
        {FS_ERROR_NO_SPACE,"ENOSPC"}, {FS_ERROR_INVALID,"EINVAL"},
    };
    for (unsigned i=0;i<sizeof(media_errors)/sizeof(media_errors[0]);i++) {
        s_fs_operation_result=media_errors[i].result;
        char check[256];
        snprintf(check,sizeof(check),"(function(){try{require('fs').readFileSync('/sd/a');}catch(e){if(e.code==='%s')return;throw e;}throw Error('missing SD error');})()",media_errors[i].code);
        assert(eval_source(check));
    }
    s_fs_operation_result = FS_ERROR_BUSY;
    assert(eval_source(s_busy_test_source));
    assert(eval_source("globalThis.eventsBeforeClear = require('events');"));
    js_require_clear_cache();
    assert(eval_source("if (eventsBeforeClear !== require('events')) throw new Error('built-in identity changed during file cache clear');"));
    assert(eval_source(events_test_source));
    js_require_cleanup();
    jerry_cleanup();
    // A new VM must not inherit branded objects, budgets or cancellation state.
    jerry_init(JERRY_INIT_EMPTY);
    board = jerry_object();
    assert(js_board_apply_registry(board, safe_mode, storage_ready));
    js_register_global("board", board);
    jerry_value_free(board);
    js_bind_require();
    js_bind_console();
    assert(eval_source("if (!Object.isFrozen(require('devices')) || require('devices').display !== undefined) throw new Error('devices recreate');"));
    assert(eval_source(events_test_source));
    js_require_cleanup();
    jerry_cleanup();

    printf("runtime Jerry binding test passed for %s\n", registry->board_id);
    return 0;
}
