/*
 * Copyright (c) 2019 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/kernel.h>

#include <zephyr/drivers/video.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL

#define VIDEO_DEV_SW "VIDEO_SW_GENERATOR"

int main(void) {
  struct video_buffer *buffers[CONFIG_VIDEO_BUFFER_POOL_NUM_MAX], *vbuf;
  struct video_format fmt;
  struct video_caps caps;
  struct video_frmival frmival;
  struct video_frmival_enum fie;
  unsigned int frame = 0;
  size_t bsize;
  int i = 0;
  int err;

#if DT_HAS_CHOSEN(zephyr_camera)
  const struct device *const video_dev =
      DEVICE_DT_GET(DT_CHOSEN(zephyr_camera));

  if (!device_is_ready(video_dev)) {
    LOG_ERR("%s: video device is not ready", video_dev->name);
    return 0;
  }
#else
  const struct device *const video_dev = device_get_binding(VIDEO_DEV_SW);

  if (video_dev == NULL) {
    LOG_ERR("%s: video device not found or failed to initialized",
            VIDEO_DEV_SW);
    return 0;
  }
#endif

  LOG_INF("Video device: %s", video_dev->name);

  /* Get capabilities */
  if (video_get_caps(video_dev, VIDEO_EP_OUT, &caps)) {
    LOG_ERR("Unable to retrieve video capabilities");
    return 0;
  }

  LOG_INF("- Capabilities:");
  while (caps.format_caps[i].pixelformat) {
    const struct video_format_cap *fcap = &caps.format_caps[i];
    /* fourcc to string */
    LOG_INF("  %c%c%c%c width [%u; %u; %u] height [%u; %u; %u]",
            (char)fcap->pixelformat, (char)(fcap->pixelformat >> 8),
            (char)(fcap->pixelformat >> 16), (char)(fcap->pixelformat >> 24),
            fcap->width_min, fcap->width_max, fcap->width_step,
            fcap->height_min, fcap->height_max, fcap->height_step);
    i++;
  }

  /* Get default/native format */
  if (video_get_format(video_dev, VIDEO_EP_OUT, &fmt)) {
    LOG_ERR("Unable to retrieve video format");
    return 0;
  }

#if CONFIG_VIDEO_FRAME_HEIGHT
  fmt.height = CONFIG_VIDEO_FRAME_HEIGHT;
#endif

#if CONFIG_VIDEO_FRAME_WIDTH
  fmt.width = CONFIG_VIDEO_FRAME_WIDTH;
  fmt.pitch = fmt.width * 2;
#endif

  if (strcmp(CONFIG_VIDEO_PIXEL_FORMAT, "")) {
    fmt.pixelformat = video_fourcc(
        CONFIG_VIDEO_PIXEL_FORMAT[0], CONFIG_VIDEO_PIXEL_FORMAT[1],
        CONFIG_VIDEO_PIXEL_FORMAT[2], CONFIG_VIDEO_PIXEL_FORMAT[3]);
  }

  LOG_INF("- Video format: %c%c%c%c %ux%u", (char)fmt.pixelformat,
          (char)(fmt.pixelformat >> 8), (char)(fmt.pixelformat >> 16),
          (char)(fmt.pixelformat >> 24), fmt.width, fmt.height);

  if (video_set_format(video_dev, VIDEO_EP_OUT, &fmt)) {
    LOG_ERR("Unable to set format");
    return 0;
  }

  if (!video_get_frmival(video_dev, VIDEO_EP_OUT, &frmival)) {
    LOG_INF("- Default frame rate : %f fps",
            1.0 * frmival.denominator / frmival.numerator);
  }

  LOG_INF("- Supported frame intervals for the default format:");
  memset(&fie, 0, sizeof(fie));
  fie.format = &fmt;
  while (video_enum_frmival(video_dev, VIDEO_EP_OUT, &fie) == 0) {
    if (fie.type == VIDEO_FRMIVAL_TYPE_DISCRETE) {
      LOG_INF("   %u/%u ", fie.discrete.numerator, fie.discrete.denominator);
    } else {
      LOG_INF("   [min = %u/%u; max = %u/%u; step = %u/%u]",
              fie.stepwise.min.numerator, fie.stepwise.min.denominator,
              fie.stepwise.max.numerator, fie.stepwise.max.denominator,
              fie.stepwise.step.numerator, fie.stepwise.step.denominator);
    }
    fie.index++;
  }

  /* Size to allocate for each buffer */
  if (caps.min_line_count == LINE_COUNT_HEIGHT) {
    bsize = fmt.pitch * fmt.height;
  } else {
    bsize = fmt.pitch * caps.min_line_count;
  }

  /* Alloc video buffers and enqueue for capture */
  for (i = 0; i < ARRAY_SIZE(buffers); i++) {
    /*
     * For some hardwares, such as the PxP used on i.MX RT1170 to do image
     * rotation, buffer alignment is needed in order to achieve the best
     * performance
     */
    buffers[i] =
        video_buffer_aligned_alloc(bsize, CONFIG_VIDEO_BUFFER_POOL_ALIGN);
    if (buffers[i] == NULL) {
      LOG_ERR("Unable to alloc video buffer");
      return 0;
    }

    video_enqueue(video_dev, VIDEO_EP_OUT, buffers[i]);
  }

  /* Start video capture */
  if (video_stream_start(video_dev)) {
    LOG_ERR("Unable to start capture (interface)");
    return 0;
  }

  LOG_INF("Capture started");

  /* Grab video frames */
  while (1) {
    err = video_dequeue(video_dev, VIDEO_EP_OUT, &vbuf, K_FOREVER);
    if (err) {
      LOG_ERR("Unable to dequeue video buf");
      return 0;
    }

    LOG_DBG("Got frame %u! size: %u; timestamp %u ms", frame++, vbuf->bytesused,
            vbuf->timestamp);

    err = video_enqueue(video_dev, VIDEO_EP_OUT, vbuf);
    if (err) {
      LOG_ERR("Unable to requeue video buf");
      return 0;
    }
  }
}
