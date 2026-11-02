#define LOG_MODULE "patch-piuio"

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "capnhook/hooklib/usb-emu.h"

#include "io/piuio/defs.h"

#include "ptapi/io/piuio.h"
#include "ptapi/io/piuio/util/lib.h"

#include "util/log.h"

// Enable this to get a detailed "call trace" of reads/writes and updates
// for debugging purpose
// #define PATCH_PIUIO_CALL_TRACE

static bool _patch_piuio_enumerate(bool real_exists);
static enum cnh_result _patch_piuio_open(void);
static enum cnh_result _patch_piuio_reset(void);
static enum cnh_result _patch_piuio_control_msg(
    int request_type,
    int request,
    int value,
    int index,
    struct cnh_iobuf *buffer,
    int timeout);
static void _patch_piuio_close(void);

static void _patch_piuio_async_start(void);
static void _patch_piuio_async_stop(void);
static void *_patch_piuio_async_thread(void *arg);
static void _patch_piuio_async_cycle(void);
static uint32_t _patch_piuio_get_async_cycle_delay_us(void);
static void _patch_piuio_read_inputs_to_buffer(struct cnh_iobuf *buffer);
static void _patch_piuio_read_outputs_from_buffer(const uint8_t *bytes);
static enum ptapi_io_piuio_sensor_group
_patch_piuio_get_sensor_group_from_buffer(const uint8_t *bytes);

static const struct cnh_usb_emu_virtdev_ep _patch_piuio_virtdev = {
    .pid = PIUIO_DRV_PID,
    .vid = PIUIO_DRV_VID,
    .enumerate = _patch_piuio_enumerate,
    .open = _patch_piuio_open,
    .reset = _patch_piuio_reset,
    .control_msg = _patch_piuio_control_msg,
    .close = _patch_piuio_close,
};

static uint32_t _patch_piuio_poll_delay_ms;
static struct ptapi_io_piuio_api _patch_piuio_api;
static enum ptapi_io_piuio_sensor_group _patch_piuio_sensor_group;

/* The 1st/2nd generation engines are effectively frame-driven and target about
   60 FPS. Polling the cabinet much faster than a frame makes the emulated IO
   appear to advance too quickly, which speeds up gameplay and pad light timing.
   Keep the async refresh near a single-frame cadence while still honoring any
   longer, intentionally configured hardware poll delay. */
#define PATCH_PIUIO_ASYNC_CYCLE_DELAY_US 16667

/* Async mode: the API implementation is exclusively used by the polling thread.
   The game facing side only exchanges latched state with it under the lock */
static bool _patch_piuio_async;
static bool _patch_piuio_async_running;
static pthread_t _patch_piuio_async_tid;
static pthread_mutex_t _patch_piuio_async_lock = PTHREAD_MUTEX_INITIALIZER;
static struct ptapi_io_piuio_pad_inputs
    _patch_piuio_async_pad_in[2][PTAPI_IO_PIUIO_SENSOR_GROUP_NUM];
static struct ptapi_io_piuio_sys_inputs _patch_piuio_async_sys_in;
static struct ptapi_io_piuio_pad_outputs _patch_piuio_async_pad_out[2];
static struct ptapi_io_piuio_cab_outputs _patch_piuio_async_cab_out;

void patch_piuio_set_async_poll(bool enable)
{
  _patch_piuio_async = enable;

  if (enable) {
    log_info("Async polling enabled");
  }
}

void patch_piuio_init(const char *piuio_lib_path, uint32_t poll_delay_ms)
{
  _patch_piuio_poll_delay_ms = poll_delay_ms;

  if (_patch_piuio_poll_delay_ms > 0) {
    log_debug("Enabled poll delay ms: %d", poll_delay_ms);
  }

  if (!piuio_lib_path) {
    log_die("No piuio emulation library path specified");
  }

  /* Load piuio library funcs */
  log_info("Loading piuio api implementation %s", piuio_lib_path);

  if (!ptapi_io_piuio_util_lib_load(piuio_lib_path, &_patch_piuio_api)) {
    log_error("Loading PIUIO API from %s failed", piuio_lib_path);
    return;
  }

  cnh_usb_emu_add_virtdevep(&_patch_piuio_virtdev);

  log_info("Initialized");
}

void patch_piuio_shutdown(void)
{
}

static bool _patch_piuio_enumerate(bool real_exists)
{
  if (real_exists) {
    log_info("Real PIUIO exists but emulation enabled, blocking real");
  } else {
    log_info("Enumerating PIUIO emulation");
  }

  return true;
}

static enum cnh_result _patch_piuio_open(void)
{
  log_info("Opening PIUIO: %s", _patch_piuio_api.ident());

  if (!_patch_piuio_api.open()) {
    log_error("Opening api piuio %s failed", _patch_piuio_api.ident());
    return CNH_RESULT_OTHER_ERROR;
  }

  _patch_piuio_async_start();

  return CNH_RESULT_SUCCESS;
}

static enum cnh_result _patch_piuio_reset(void)
{
  log_info("Resetting PIUIO: %s", _patch_piuio_api.ident());

  _patch_piuio_async_stop();
  _patch_piuio_api.close();

  if (!_patch_piuio_api.open()) {
    log_error("Resetting api piuio %s failed", _patch_piuio_api.ident());
    return CNH_RESULT_OTHER_ERROR;
  }

  _patch_piuio_async_start();

  return CNH_RESULT_SUCCESS;
}

static enum cnh_result _patch_piuio_control_msg(
    int request_type,
    int request,
    int value,
    int index,
    struct cnh_iobuf *buffer,
    int timeout)
{
  if (_patch_piuio_poll_delay_ms > 0) {
    usleep(_patch_piuio_poll_delay_ms * 1000);
  }

  /**
   * Expected call pattern for a full game state update on a single frame (when
   * done synchronously)
   *
   * CTRL_OUT: light data + sensor 0
   * CTRL_IN: input data of sensor 0
   * CTRL_OUT: light data + sensor 1
   * CTRL_IN: input data of sensor 1
   * CTRL_OUT: light data + sensor 2
   * CTRL_IN: input data of sensor 2
   * CTRL_OUT: light data + sensor 3
   * CTRL_IN: input data of sensor 3
   *
   * Reduce call overhead to pumptools's piuio API for recv and send polling any
   * potential implementation
   */
  if (request_type == PIUIO_DRV_USB_CTRL_TYPE_IN &&
      request == PIUIO_DRV_USB_CTRL_REQUEST) {
    if (buffer->nbytes != PIUIO_DRV_BUFFER_SIZE) {
      log_error("Invalid buffer size for ctrl in: %d", buffer->nbytes);
      return CNH_RESULT_INVALID_PARAMETER;
    }

#ifdef PATCH_PIUIO_CALL_TRACE
    log_debug("Read");
#endif

    // Only read buffered inputs, no need to trigger another update since
    // this is taken of by the cycle start

    _patch_piuio_read_inputs_to_buffer(buffer);

    return CNH_RESULT_SUCCESS;
  } else if (
      request_type == PIUIO_DRV_USB_CTRL_TYPE_OUT &&
      request == PIUIO_DRV_USB_CTRL_REQUEST) {
    uint8_t output_buffer[PIUIO_DRV_BUFFER_SIZE];

    if (buffer->nbytes != PIUIO_DRV_BUFFER_SIZE) {
      log_error("Invalid buffer size for ctrl out: %d", buffer->nbytes);
      return CNH_RESULT_INVALID_PARAMETER;
    }

    /* The game owns this buffer and may access it from another thread. Take a
       single snapshot so one request cannot combine bytes from different
       states. */
    memcpy(output_buffer, buffer->bytes, sizeof(output_buffer));

    _patch_piuio_read_outputs_from_buffer(output_buffer);

    // Sync properly with sensor cycling by application
    // Note: Naturally, the update code below will break if sensors are not
    // cycled as expected
    _patch_piuio_sensor_group =
        _patch_piuio_get_sensor_group_from_buffer(output_buffer);

#ifdef PATCH_PIUIO_CALL_TRACE
    log_debug("Write: %d", _patch_piuio_sensor_group);
#endif

    // Trigger exactly ONE full update cycle on the API implementation on
    // every first call of the whole update cycle

    if (_patch_piuio_sensor_group == 0 && !_patch_piuio_async) {
#ifdef PATCH_PIUIO_CALL_TRACE
      log_debug("Update API");
#endif

      if (!_patch_piuio_api.send()) {
        log_error(
            "Sending outputs on api piuio %s failed", _patch_piuio_api.ident());
        return CNH_RESULT_OTHER_ERROR;
      }

      if (!_patch_piuio_api.recv()) {
        log_error(
            "Receiving inputs on api piuio %s failed",
            _patch_piuio_api.ident());
        return CNH_RESULT_OTHER_ERROR;
      }
    }

    return CNH_RESULT_SUCCESS;
  } else if (request_type == 0 && request == 0) {
    // ITG 2/PIU Pro kernel hack, can be handled by piuio-khack module
    // Safety net for visibility if the module is missing
    log_error("Unhandled PIUIO kernel hack request detected, cannot dispatch");

    return CNH_RESULT_INVALID_PARAMETER;
  } else {
    log_error("Invalid usb control request to PIUIO: 0x%02X", request);
    return CNH_RESULT_INVALID_PARAMETER;
  }
}

static void _patch_piuio_close(void)
{
  log_info("Closing PIUIO: %s", _patch_piuio_api.ident());

  _patch_piuio_async_stop();
  _patch_piuio_api.close();
}

static void _patch_piuio_async_start(void)
{
  if (!_patch_piuio_async || _patch_piuio_async_running) {
    return;
  }

  memset(_patch_piuio_async_pad_in, 0, sizeof(_patch_piuio_async_pad_in));
  memset(&_patch_piuio_async_sys_in, 0, sizeof(_patch_piuio_async_sys_in));

  _patch_piuio_async_running = true;

  if (pthread_create(
          &_patch_piuio_async_tid, NULL, _patch_piuio_async_thread, NULL) != 0) {
    log_error("Starting async polling thread failed");
    _patch_piuio_async_running = false;
  }
}

static void _patch_piuio_async_stop(void)
{
  if (!_patch_piuio_async_running) {
    return;
  }

  pthread_mutex_lock(&_patch_piuio_async_lock);
  _patch_piuio_async_running = false;
  pthread_mutex_unlock(&_patch_piuio_async_lock);

  pthread_join(_patch_piuio_async_tid, NULL);
}

static void *_patch_piuio_async_thread(void *arg)
{
  bool running;

  (void) arg;

  do {
    _patch_piuio_async_cycle();

    usleep(_patch_piuio_get_async_cycle_delay_us());

    pthread_mutex_lock(&_patch_piuio_async_lock);
    running = _patch_piuio_async_running;
    pthread_mutex_unlock(&_patch_piuio_async_lock);
  } while (running);

  return NULL;
}

static uint32_t _patch_piuio_get_async_cycle_delay_us(void)
{
  uint32_t configured_delay_us = _patch_piuio_poll_delay_ms * 1000U;

  if (configured_delay_us > PATCH_PIUIO_ASYNC_CYCLE_DELAY_US) {
    return configured_delay_us;
  }

  return PATCH_PIUIO_ASYNC_CYCLE_DELAY_US;
}

static void _patch_piuio_async_cycle(void)
{
  static bool failed;

  struct ptapi_io_piuio_pad_outputs pad_out[2];
  struct ptapi_io_piuio_cab_outputs cab_out;
  struct ptapi_io_piuio_pad_inputs pad_in[2][PTAPI_IO_PIUIO_SENSOR_GROUP_NUM];
  struct ptapi_io_piuio_sys_inputs sys_in;

  pthread_mutex_lock(&_patch_piuio_async_lock);
  memcpy(pad_out, _patch_piuio_async_pad_out, sizeof(pad_out));
  memcpy(&cab_out, &_patch_piuio_async_cab_out, sizeof(cab_out));
  pthread_mutex_unlock(&_patch_piuio_async_lock);

  _patch_piuio_api.set_output_pad(0, &pad_out[0]);
  _patch_piuio_api.set_output_pad(1, &pad_out[1]);
  _patch_piuio_api.set_output_cab(&cab_out);

  if (!_patch_piuio_api.send() || !_patch_piuio_api.recv()) {
    /* Don't flood the log if the device stays broken */
    if (!failed) {
      log_error("Updating api piuio %s failed", _patch_piuio_api.ident());
      failed = true;
    }

    return;
  }

  failed = false;

  memset(pad_in, 0, sizeof(pad_in));
  memset(&sys_in, 0, sizeof(sys_in));

  for (uint8_t i = 0; i < PTAPI_IO_PIUIO_SENSOR_GROUP_NUM; i++) {
    _patch_piuio_api.get_input_pad(0, i, &pad_in[0][i]);
    _patch_piuio_api.get_input_pad(1, i, &pad_in[1][i]);
  }

  _patch_piuio_api.get_input_sys(&sys_in);

  pthread_mutex_lock(&_patch_piuio_async_lock);
  memcpy(_patch_piuio_async_pad_in, pad_in, sizeof(pad_in));
  memcpy(&_patch_piuio_async_sys_in, &sys_in, sizeof(sys_in));
  pthread_mutex_unlock(&_patch_piuio_async_lock);
}

static void _patch_piuio_read_inputs_to_buffer(struct cnh_iobuf *buffer)
{
  uint8_t input_buffer[PIUIO_DRV_BUFFER_SIZE];
  struct ptapi_io_piuio_pad_inputs p1_pad_in;
  struct ptapi_io_piuio_pad_inputs p2_pad_in;
  struct ptapi_io_piuio_sys_inputs sys_in;

  memset(&p1_pad_in, 0, sizeof(struct ptapi_io_piuio_pad_inputs));
  memset(&p2_pad_in, 0, sizeof(struct ptapi_io_piuio_pad_inputs));
  memset(&sys_in, 0, sizeof(struct ptapi_io_piuio_sys_inputs));

  if (_patch_piuio_async) {
    pthread_mutex_lock(&_patch_piuio_async_lock);
    p1_pad_in = _patch_piuio_async_pad_in[0][_patch_piuio_sensor_group];
    p2_pad_in = _patch_piuio_async_pad_in[1][_patch_piuio_sensor_group];
    sys_in = _patch_piuio_async_sys_in;
    pthread_mutex_unlock(&_patch_piuio_async_lock);
  } else {
    _patch_piuio_api.get_input_pad(0, _patch_piuio_sensor_group, &p1_pad_in);
    _patch_piuio_api.get_input_pad(1, _patch_piuio_sensor_group, &p2_pad_in);

    _patch_piuio_api.get_input_sys(&sys_in);
  }

  /* PIUIO inputs are active-low. Build the complete response locally before
     publishing it to the game-owned transfer buffer in one copy. */
  memset(input_buffer, 0xFF, sizeof(input_buffer));

  /*
     byte 0:
   bit 0: sensor p1: LU
   bit 1: sensor p1: RU
   bit 2: sensor p1: CN
   bit 3: sensor p1: LD
   bit 4: sensor p1: RD
   bit 5:
   bit 6:
   bit 7:

   byte 1:
   bit 0:
   bit 1: test
   bit 2: coin 1
   bit 3:
   bit 4:
   bit 5:
   bit 6: service
   bit 7: clear

   byte 2:
   bit 0: sensor p2: LU
   bit 1: sensor p2: RU
   bit 2: sensor p2: CN
   bit 3: sensor p2: LD
   bit 4: sensor p2: RD
   bit 5:
   bit 6:
   bit 7:

   byte 3:
   bit 0:
   bit 1:
   bit 2: coin 2
   bit 3:
   bit 4:
   bit 5:
   bit 6:
   bit 7:

   bytes 4 - 7 dummy
  */

  /* Player 1 */
  input_buffer[0] &= ~((p1_pad_in.lu ? 1 : 0) << 0);
  input_buffer[0] &= ~((p1_pad_in.ru ? 1 : 0) << 1);
  input_buffer[0] &= ~((p1_pad_in.cn ? 1 : 0) << 2);
  input_buffer[0] &= ~((p1_pad_in.ld ? 1 : 0) << 3);
  input_buffer[0] &= ~((p1_pad_in.rd ? 1 : 0) << 4);

  /* Player 2 */
  input_buffer[2] &= ~((p2_pad_in.lu ? 1 : 0) << 0);
  input_buffer[2] &= ~((p2_pad_in.ru ? 1 : 0) << 1);
  input_buffer[2] &= ~((p2_pad_in.cn ? 1 : 0) << 2);
  input_buffer[2] &= ~((p2_pad_in.ld ? 1 : 0) << 3);
  input_buffer[2] &= ~((p2_pad_in.rd ? 1 : 0) << 4);

  /* Sys */
  input_buffer[1] &= ~((sys_in.test ? 1 : 0) << 1);
  input_buffer[1] &= ~((sys_in.service ? 1 : 0) << 6);
  input_buffer[1] &= ~((sys_in.clear ? 1 : 0) << 7);
  input_buffer[1] &= ~((sys_in.coin ? 1 : 0) << 2);

  input_buffer[3] &= ~((sys_in.coin2 ? 1 : 0) << 2);

  memcpy(buffer->bytes, input_buffer, sizeof(input_buffer));
  buffer->pos = PIUIO_DRV_BUFFER_SIZE;
}

static void _patch_piuio_read_outputs_from_buffer(const uint8_t *bytes)
{
  struct ptapi_io_piuio_pad_outputs p1_pad_out;
  struct ptapi_io_piuio_pad_outputs p2_pad_out;
  struct ptapi_io_piuio_cab_outputs cab_out;

  /*
   byte 0:
   bit 0: sensor id bit 0	select sensor to be read for next input request:
   bit 1: sensor id bit 1   00 = sens1, 01 = sens2, 10 = sens3, 11 = sens4
   bit 2: pad light p1: LU
   bit 3: pad light p1: RU
   bit 4: pad light p1: CN
   bit 5: pad light p1: LD
   bit 6: pad light p1: RD
   bit 7:

   byte 1:
   bit 0:
   bit 1:
   bit 2: light neons
   bit 3: coin counter 2 (front usb ports enable)
   bit 4:
   bit 5:
   bit 6:
   bit 7:

   byte 2:
   bit 0:
   bit 1:
   bit 2: pad light p2: LU
   bit 3: pad light p2: RU
   bit 4: pad light p2: CN
   bit 5: pad light p2: LD
   bit 6: pad light p2: RD
   bit 7: halogen R2

   byte 3:
   bit 0: halogen R1
   bit 1: halogen L2
   bit 2: halogen L1
   bit 3:
   bit 4: coin counter 1
   bit 5:
   bit 6:
   bit 7:

   bytes 4 - 7 dummy
  */

  p1_pad_out.lu = (bytes[0] & (1 << 2)) > 0;
  p1_pad_out.ru = (bytes[0] & (1 << 3)) > 0;
  p1_pad_out.cn = (bytes[0] & (1 << 4)) > 0;
  p1_pad_out.ld = (bytes[0] & (1 << 5)) > 0;
  p1_pad_out.rd = (bytes[0] & (1 << 6)) > 0;

  p2_pad_out.lu = (bytes[2] & (1 << 2)) > 0;
  p2_pad_out.ru = (bytes[2] & (1 << 3)) > 0;
  p2_pad_out.cn = (bytes[2] & (1 << 4)) > 0;
  p2_pad_out.ld = (bytes[2] & (1 << 5)) > 0;
  p2_pad_out.rd = (bytes[2] & (1 << 6)) > 0;

  cab_out.bass = (bytes[1] & (1 << 2)) > 0;
  cab_out.halo_r2 = (bytes[2] & (1 << 7)) > 0;
  cab_out.halo_r1 = (bytes[3] & (1 << 0)) > 0;
  cab_out.halo_l2 = (bytes[3] & (1 << 1)) > 0;
  cab_out.halo_l1 = (bytes[3] & (1 << 2)) > 0;

  if (_patch_piuio_async) {
    pthread_mutex_lock(&_patch_piuio_async_lock);
    _patch_piuio_async_pad_out[0] = p1_pad_out;
    _patch_piuio_async_pad_out[1] = p2_pad_out;
    _patch_piuio_async_cab_out = cab_out;
    pthread_mutex_unlock(&_patch_piuio_async_lock);
  } else {
    _patch_piuio_api.set_output_pad(0, &p1_pad_out);
    _patch_piuio_api.set_output_pad(1, &p2_pad_out);
    _patch_piuio_api.set_output_cab(&cab_out);
  }
}

static enum ptapi_io_piuio_sensor_group
_patch_piuio_get_sensor_group_from_buffer(const uint8_t *bytes)
{
  return (enum ptapi_io_piuio_sensor_group)(bytes[0] & 0x03);
}
