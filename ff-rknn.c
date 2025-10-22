/*
 * ff-rknn - Decode H264 / HEVC, AI Inference, Render it on screen
 * 2023 Alexander Finger <alex.mobigo@gmail.com>
 *
 * This code has been tested on Rockchip RK3588 platform
 *      kernel v5.10.110 BSP
 *      ffmpeg 4.4.2 / ffmpeg 5.1 / ffmpeg 6 + SDL3 + RKNN support
 *
 * FFMPEG DRM/KMS example application
 * Jorge Ramirez-Ortiz <jramirez@baylibre.com>
 *
 * Main file of the application
 *      Based on code from:
 *              2001 Fabrice Bellard (FFMPEG/doc/examples/decode_video.codec_ctx
 *              2018 Stanimir Varbanov (v4l2-decode/src/drm.codec_ctx)
 *
 * This code has been tested on Linaro's Dragonboard 820c
 *      kernel v4.14.15, venus decoder
 *      ffmpeg 4.0 + lrusacks ffmpeg/DRM support + review
 *              https://github.com/ldts/ffmpeg  branch lrusak/v4l2-drmprime
 *
 *
 * Copyright (codec_ctx) 2018 Baylibre
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "SDL3/SDL.h"
/* #include "SDL_syswm.h" */  // Commented out for SDL3 compatibility

#include <drm_fourcc.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext_drm.h>
#include <libavutil/pixfmt.h>

#include <rga/RgaApi.h>
#include <rga/rga.h>

#ifdef __cplusplus
} // closing brace for extern "C"
#endif

#include "yolov8.h"
#include "image_drawing.h"
#include "tracker.h"

#define ALIGN(x, a)           ((x) + (a - 1)) & (~(a - 1))
#define DRM_ALIGN(val, align) ((val + (align - 1)) & ~(align - 1))

#ifndef DRM_FORMAT_NV12_10
#define DRM_FORMAT_NV12_10 fourcc_code('N', 'A', '1', '2')
#endif

#ifndef DRM_FORMAT_NV15
#define DRM_FORMAT_NV15 fourcc_code('N', 'A', '1', '5')
#endif

#define MODEL_WIDTH  640
#define MODEL_HEIGHT 640

#define arg_a 36430 // -a
#define arg_b 36431 // -b
#define arg_i 36438 // -i
#define arg_x 36453 // -x
#define arg_y 36454 // -y
#define arg_l 36441 // -l
#define arg_m 36442 // -m
#define arg_o 36444 // -o
#define arg_t 36449 // -t
#define arg_f 36435 // -f
#define arg_r 36447 // -r
#define arg_d 36433 // -d
#define arg_p 36445 // -p
#define arg_s 36448 // -s
// New flags
#define arg_F 36403   // -F  (Processing FPS cap)
#define arg_fps 39681526 // -fps (Processing FPS cap alias)
#define arg_Fd 1201399   // -Fd (Drop input packets to enforce FPS cap)
#define arg_track 280167394 // -track (Enable tracking)
#define arg_roi 39694551 // -roi (Region of Interest zones)
#define arg_roi_overlap 1950882749 // -roi-overlap (ROI overlap percentage 0-100)

static unsigned int hash_me(char *str);

/* --- RKNN --- */
int channel = 3;
int width = MODEL_WIDTH;
int height = MODEL_HEIGHT;
unsigned char *model_data;
int model_data_size = 0;
char *model_name = NULL;
float scale_w = 1.0f; // (float)width / img_width;
float scale_h = 1.0f; // (float)height / img_height;
rknn_app_context_t rknn_app_ctx;
image_buffer_t src_image;
std::vector<float> out_scales;
std::vector<int32_t> out_zps;
rknn_context ctx;
rknn_input_output_num io_num;
rknn_input inputs[2];
rknn_tensor_attr output_attrs[256];
size_t actual_size = 0;
const float nms_threshold = NMS_THRESH;
const float box_conf_threshold = BOX_THRESH;
/* --- SDL --- */
int alphablend;
int accur;
unsigned int obj2det;
int frameSize_texture;
int frameSize_rknn;
void *resize_buf;
void *texture_dst_buf;
Uint32 format;
SDL_Texture *texture;
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;

int screen_width = 960;
int screen_height = 540;
int screen_left = 0;
int screen_top = 0;
unsigned int frame_width = 960;
unsigned int frame_height = 540;
int source_video_width = 0;   // Actual video source width (will be detected)
int source_video_height = 0;  // Actual video source height (will be detected)
int v4l2;  // v4l2 h264
int rtsp;  // rtsp h264
int rtmp;  // flv h264
int http;  // flv h264
int delay; // ms
char *pixel_format;
char *sensor_frame_size;
char *sensor_frame_rate;

float frmrate = 0.0;      // Measured frame rate
float avg_frmrate = 0.0;  // avg frame rate
float prev_frmrate = 0.0; // avg frame rate
Uint32 currtime;
Uint32 lasttime;
int loop_counter = 0;
const int frmrate_update = 25;

// Memory monitoring
static int frame_counter = 0;
static Uint32 last_health_check = 0;

// FPS limiting
float target_fps = 0.0f;        // desired processing FPS (0 = unlimited)
Uint32 last_process_tick = 0;   // last time we processed a frame (ms)
int fps_drop_mode = 0;          // 0: only throttle processing; 1: also drop decode packets

// Tracking
Tracker global_tracker;
int enable_tracking = 0;

// Tracking event callback
void on_track_event(int track_id, TrackState old_state, TrackState new_state) {
    // Su kien da duoc in trong tracker.c
    // Ham goi lai nay co the dung de xu ly them neu can
    
    const char* state_names[] = {"TENTATIVE", "NEW", "TRACKED", "LOST_TEMP", "LOST_PERMANENT"};
    const char* state_names_vi[] = {"Tam", "Moi", "Dang theo doi", "Mat tam thoi", "Mat vinh vien"};
    
    if (old_state != new_state) {
        fprintf(stderr, "[CALLBACK] Track ID %d: %s -> %s  (VI: %s -> %s)\n",
                track_id,
                state_names[old_state], state_names[new_state],
                state_names_vi[old_state], state_names_vi[new_state]);
    }
}


enum AVPixelFormat get_format(AVCodecContext *Context,
                              const enum AVPixelFormat *PixFmt)
{
    while (*PixFmt != AV_PIX_FMT_NONE) {
        if (*PixFmt == AV_PIX_FMT_DRM_PRIME)
            return AV_PIX_FMT_DRM_PRIME;
        PixFmt++;
    }
    return AV_PIX_FMT_NONE;
}

static int drm_rga_buf(int src_Width, int src_Height, int wStride, int hStride, int src_fd,
                       int src_format, int dst_Width, int dst_Height,
                       int dst_format, int frameSize, char *buf)
{
    rga_info_t src;
    rga_info_t dst;
    int ret;
    // int hStride = (src_Height + 15) & (~15);
    // int wStride = (src_Width + 15) & (~15);
    // int dhStride = (dst_Height + 15) & (~15);
    // int dwStride = (dst_Width + 15) & (~15);

    memset(&src, 0, sizeof(rga_info_t));
    src.fd = src_fd;
    src.mmuFlag = 1;

    memset(&dst, 0, sizeof(rga_info_t));
    dst.fd = -1;
    dst.virAddr = buf;
    dst.mmuFlag = 1;

    rga_set_rect(&src.rect, 0, 0, src_Width, src_Height, wStride, hStride,
                 src_format);
    rga_set_rect(&dst.rect, 0, 0, dst_Width, dst_Height, dst_Width, dst_Height,
                 dst_format);

    ret = c_RkRgaBlit(&src, &dst, NULL);
    return ret;
}

#if 0
static char *drm_get_rgaformat_str(uint32_t drm_fmt) 
{
  switch (drm_fmt) {
  case DRM_FORMAT_NV12:
    return "RK_FORMAT_YCbCr_420_SP";
  case DRM_FORMAT_NV12_10:
    return "RK_FORMAT_YCbCr_420_SP_10B";
  case DRM_FORMAT_NV15:
    return "RK_FORMAT_YCbCr_420_SP_10B";
  case DRM_FORMAT_NV16:
    return "RK_FORMAT_YCbCr_422_SP";
  case DRM_FORMAT_YUYV:
    return "RK_FORMAT_YUYV_422";
  case DRM_FORMAT_UYVY:
    return "RK_FORMAT_UYVY_422";
  default:
    return "0";
  }
}
#endif

static uint32_t drm_get_rgaformat(uint32_t drm_fmt)
{
    switch (drm_fmt) {
    case DRM_FORMAT_NV12:
        return RK_FORMAT_YCbCr_420_SP;
    case DRM_FORMAT_NV12_10:
        return RK_FORMAT_YCbCr_420_SP_10B;
    case DRM_FORMAT_NV15:
        return RK_FORMAT_YCbCr_420_SP_10B;
    case DRM_FORMAT_NV16:
        return RK_FORMAT_YCbCr_422_SP;
    case DRM_FORMAT_YUYV:
        return RK_FORMAT_YUYV_422;
    case DRM_FORMAT_UYVY:
        return RK_FORMAT_UYVY_422;
    default:
        return 0;
    }
}


static int decode_and_display(AVCodecContext *dec_ctx, AVFrame *frame,
                              AVPacket *pkt)
{
    AVDRMFrameDescriptor *desc;
    AVDRMLayerDescriptor *layer;
    unsigned int drm_format;
    RgaSURF_FORMAT src_format;
    RgaSURF_FORMAT dst_format;
    int hStride, wStride;
    SDL_Rect rect;
    int ret;

    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error sending a packet for decoding\n");
        return ret;
    }
    ret = 0;
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            fprintf(stderr, "Error during decoding!\n");
            return ret;
        }
        
        // CRITICAL: Unref frame at the end of each iteration to prevent memory leak
        // This will be done at the end of the processing loop

        // FPS limiter: drop frames if we already processed one within the interval
        if (target_fps > 0.0f) {
            Uint32 now = SDL_GetTicks();
            Uint32 min_interval = (Uint32)(1000.0f / target_fps);
            if (last_process_tick == 0) {
                last_process_tick = now; // initialize on first frame
            }
            if ((now - last_process_tick) < min_interval) {
                // Skip processing this frame (decode ok, no inference/render)
                continue;
            }
            last_process_tick = now;
        }

        desc = (AVDRMFrameDescriptor *)frame->data[0];
        layer = &desc->layers[0];
        if (desc && layer) {
            // Detect source video dimensions on first frame
            if (source_video_width == 0 && source_video_height == 0) {
                source_video_width = frame->width;
                source_video_height = frame->height;
                fprintf(stderr, "📹 Source video: %dx%d, Display: %dx%d\n", 
                       source_video_width, source_video_height, screen_width, screen_height);
                
                // Scale polygon ROI zones if needed
                if ((source_video_width != screen_width || source_video_height != screen_height) && 
                    g_roi_zones_count > 0) {
                    float scale_x = (float)screen_width / source_video_width;
                    float scale_y = (float)screen_height / source_video_height;
                    
                    fprintf(stderr, "🔄 Scaling ROI polygons: x=%.3f, y=%.3f\n", scale_x, scale_y);
                    
                    for (int i = 0; i < g_roi_zones_count; i++) {
                        if (g_roi_zones[i].type == ROI_TYPE_POLYGON) {
                            for (int p = 0; p < g_roi_zones[i].num_points; p++) {
                                int orig_x = g_roi_zones[i].points[p].x;
                                int orig_y = g_roi_zones[i].points[p].y;
                                g_roi_zones[i].points[p].x = (int)(orig_x * scale_x);
                                g_roi_zones[i].points[p].y = (int)(orig_y * scale_y);
                                fprintf(stderr, "  Zone %d Point %d: (%d,%d) -> (%d,%d)\n",
                                       i+1, p+1, orig_x, orig_y, 
                                       g_roi_zones[i].points[p].x, g_roi_zones[i].points[p].y);
                            }
                        }
                    }
                }
            }
            
            wStride = layer->planes[0].pitch;
            hStride = (layer->planes[1].offset / layer->planes[0].pitch);

            drm_format = layer->format;
            src_format = (RgaSURF_FORMAT)drm_get_rgaformat(drm_format);

            /* ------------ RKNN ----------- */
            // First convert to display size (for rendering)
            drm_rga_buf(frame->width, frame->height, wStride, hStride, desc->objects[0].fd, src_format,
                        screen_width, screen_height, RK_FORMAT_RGB_888,
                        frameSize_texture, (char *)texture_dst_buf);
            
            // Convert to a temporary buffer that maintains aspect ratio for model input
            // We'll use screen buffer as source since it's already in RGB888
            image_buffer_t temp_src_image;
            temp_src_image.width = screen_width;
            temp_src_image.height = screen_height;
            temp_src_image.width_stride = screen_width;
            temp_src_image.height_stride = screen_height;
            temp_src_image.format = IMAGE_FORMAT_RGB888;
            temp_src_image.virt_addr = (unsigned char*)texture_dst_buf;
            temp_src_image.size = frameSize_texture;
            temp_src_image.fd = -1;

            // Setup display image buffer
            src_image.width = screen_width;
            src_image.height = screen_height;
            src_image.width_stride = screen_width;
            src_image.height_stride = screen_height;
            src_image.format = IMAGE_FORMAT_RGB888;
            src_image.virt_addr = (unsigned char*)texture_dst_buf;
            src_image.size = frameSize_texture;
            src_image.fd = -1;

            // Run YOLOv8 inference with proper letterbox preprocessing
            // inference_yolov8_model will handle letterbox conversion internally
            object_detect_result_list od_results;
            ret = inference_yolov8_model(&rknn_app_ctx, &temp_src_image, &od_results);

            // Update tracker with detections
            if (tracker_is_enabled(&global_tracker)) {
                tracker_update(&global_tracker, od_results.results, od_results.count, on_track_event);
            }

            // Note: Detection results are already in source image coordinates (screen_width x screen_height)
            // because letterbox transformation is handled inside inference_yolov8_model
            // No additional scaling needed if source is already screen size

            char text[256];
            
            // If tracking is enabled, use tracked objects; otherwise use detections
            if (tracker_is_enabled(&global_tracker)) {
                TrackedObject tracks[MAX_TRACKED_OBJECTS];
                int num_tracks = tracker_get_active_tracks(&global_tracker, tracks, MAX_TRACKED_OBJECTS);
                
                for (int i = 0; i < num_tracks; i++)
                {
                    TrackedObject *track = &tracks[i];
                    
                    // Skip permanently lost tracks and tentative tracks
                    if (track->state == TRACK_STATE_LOST_PERMANENT || 
                        track->state == TRACK_STATE_TENTATIVE) continue;
                    
                    int x1 = track->box.left;
                    int y1 = track->box.top;
                    int x2 = track->box.right;
                    int y2 = track->box.bottom;
                    
                    // Ensure coordinates are within bounds
                    x1 = x1 < 0 ? 0 : (x1 >= screen_width ? screen_width - 1 : x1);
                    y1 = y1 < 0 ? 0 : (y1 >= screen_height ? screen_height - 1 : y1);
                    x2 = x2 < 0 ? 0 : (x2 >= screen_width ? screen_width - 1 : x2);
                    y2 = y2 < 0 ? 0 : (y2 >= screen_height ? screen_height - 1 : y2);

                    // Choose color based on track state
                    uint32_t box_color = COLOR_BLUE;
                    if (track->state == TRACK_STATE_NEW) {
                        box_color = COLOR_GREEN;  // Green for new tracks (just confirmed)
                    } else if (track->state == TRACK_STATE_LOST_TEMP) {
                        box_color = 0xFF8800FF;   // Orange for lost temporarily
                    }

                    draw_rectangle(&src_image, x1, y1, x2 - x1, y2 - y1, box_color, 3);

                    sprintf(text, "ID:%d %s %.1f%%", track->track_id, 
                           coco_cls_to_name(track->cls_id), track->confidence * 100);
                    draw_text(&src_image, text, x1, y1 - 20, COLOR_RED, 10);
                }
            } else {
                // Original detection display (without tracking)
                for (int i = 0; i < od_results.count; i++)
                {
                    object_detect_result *det_result = &(od_results.results[i]);
                    
                    int x1 = det_result->box.left;
                    int y1 = det_result->box.top;
                    int x2 = det_result->box.right;
                    int y2 = det_result->box.bottom;
                    
                    // Ensure coordinates are within bounds
                    x1 = x1 < 0 ? 0 : (x1 >= screen_width ? screen_width - 1 : x1);
                    y1 = y1 < 0 ? 0 : (y1 >= screen_height ? screen_height - 1 : y1);
                    x2 = x2 < 0 ? 0 : (x2 >= screen_width ? screen_width - 1 : x2);
                    y2 = y2 < 0 ? 0 : (y2 >= screen_height ? screen_height - 1 : y2);

                    // printf("%s @ (%d %d %d %d) %.3f\n", 
                    //     coco_cls_to_name(det_result->cls_id),
                    //     x1, y1, x2, y2,
                    //     det_result->prop);

                    draw_rectangle(&src_image, x1, y1, x2 - x1, y2 - y1, COLOR_BLUE, 3);

                    sprintf(text, "%s %.1f%%", coco_cls_to_name(det_result->cls_id), det_result->prop * 100);
                    draw_text(&src_image, text, x1, y1 - 20, COLOR_RED, 10);
                }
            }

            // Draw ROI zones on screen (yellow borders)
            if (g_roi_zones_count > 0) {
                for (int roi_i = 0; roi_i < g_roi_zones_count; roi_i++) {
                    roi_zone_t *zone = &g_roi_zones[roi_i];
                    
                    if (zone->type == ROI_TYPE_RECT) {
                        // Draw rectangle ROI
                        int roi_x = (int)(zone->x * screen_width);
                        int roi_y = (int)(zone->y * screen_height);
                        int roi_w = (int)(zone->width * screen_width);
                        int roi_h = (int)(zone->height * screen_height);
                        
                        draw_rectangle(&src_image, roi_x, roi_y, roi_w, roi_h, 0x00FFFFFF, 2);
                        
                        sprintf(text, "ROI %d (Rect)", roi_i + 1);
                        draw_text(&src_image, text, roi_x + 5, roi_y + 5, 0x00FFFFFF, 10);
                        
                    } else if (zone->type == ROI_TYPE_POLYGON) {
                        // Draw polygon ROI - connect points with lines
                        for (int p = 0; p < zone->num_points; p++) {
                            int x1 = zone->points[p].x;
                            int y1 = zone->points[p].y;
                            int x2 = zone->points[(p + 1) % zone->num_points].x;
                            int y2 = zone->points[(p + 1) % zone->num_points].y;
                            
                            // Draw line between consecutive points
                            // Simple line drawing (can be improved with Bresenham if needed)
                            int dx = abs(x2 - x1);
                            int dy = abs(y2 - y1);
                            int steps = dx > dy ? dx : dy;
                            
                            if (steps > 0) {
                                float x_inc = (float)(x2 - x1) / steps;
                                float y_inc = (float)(y2 - y1) / steps;
                                float x = x1;
                                float y = y1;
                                
                                for (int s = 0; s <= steps; s++) {
                                    int px = (int)x;
                                    int py = (int)y;
                                    if (px >= 0 && px < screen_width && py >= 0 && py < screen_height) {
                                        // Draw thick line (3 pixels)
                                        for (int dy = -1; dy <= 1; dy++) {
                                            for (int dx = -1; dx <= 1; dx++) {
                                                int npx = px + dx;
                                                int npy = py + dy;
                                                if (npx >= 0 && npx < screen_width && npy >= 0 && npy < screen_height) {
                                                    unsigned char *pixel = src_image.virt_addr + (npy * screen_width + npx) * 3;
                                                    pixel[0] = 255; // R
                                                    pixel[1] = 255; // G
                                                    pixel[2] = 0;   // B (Yellow)
                                                }
                                            }
                                        }
                                    }
                                    x += x_inc;
                                    y += y_inc;
                                }
                            }
                        }
                        
                        // Draw label at first point
                        sprintf(text, "ROI %d (Poly)", roi_i + 1);
                        draw_text(&src_image, text, zone->points[0].x + 5, zone->points[0].y + 5, 0x00FFFFFF, 10);
                    }
                }
            }

            // Display the processed image with bounding boxes
            SDL_UpdateTexture(texture, NULL, texture_dst_buf, screen_width * 3);
            SDL_RenderClear(renderer);
            SDL_RenderTexture(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);
            
            // Health monitoring - print stats every 300 frames (~10 sec at 30fps)
            frame_counter++;
            if (frame_counter % 300 == 0) {
                Uint32 now = SDL_GetTicks();
                if (last_health_check > 0) {
                    float actual_fps = 300000.0f / (now - last_health_check);
                    fprintf(stderr, "📊 Health: %d frames | FPS: %.1f | Tracks: %d\n",
                           frame_counter, actual_fps, 
                           tracker_is_enabled(&global_tracker) ? global_tracker.count : 0);
                }
                last_health_check = now;
            }

            // rknn_inputs_set(ctx, io_num.n_input, inputs);

            // rknn_output outputs[io_num.n_output];
            // memset(outputs, 0, sizeof(outputs));
            // for (int i = 0; i < io_num.n_output; i++) {
            //     outputs[i].want_float = 0;
            // }

            // ret = rknn_run(ctx, NULL);
            // ret = rknn_outputs_get(ctx, io_num.n_output, outputs, NULL);

            // // post process
            // scale_w = (float)width / screen_width;
            // scale_h = (float)height / screen_height;

            // for (int i = 0; i < io_num.n_output; ++i) {
            //     out_scales.push_back(output_attrs[i].scale);
            //     out_zps.push_back(output_attrs[i].zp);
            // }
            // post_process((int8_t *)outputs[0].buf, (int8_t *)outputs[1].buf, (int8_t *)outputs[2].buf,
            //              height, width, box_conf_threshold, nms_threshold,
            //              scale_w, scale_h, out_zps, out_scales, &detect_result_group);

            // displayTexture(texture_dst_buf);
            // ret = rknn_outputs_release(ctx, io_num.n_output, outputs);
        }
        
        // CRITICAL FIX: Unref frame after processing to prevent memory leak
        av_frame_unref(frame);
    }
    return 0;
}

static unsigned int hash_me(char *str)
{
    unsigned int hash = 32;
    while (*str) {
        hash = ((hash << 5) + hash) + (*str++);
    }
    return hash;
}

void print_help(void)
{
    fprintf(stderr, "ff-rknn parameters:\n"
                    "-x displayed width\n"
                    "-y displayed height\n"
                    "-m rknn model\n"
                    "-f protocol (v4l2, rtsp, rtmp, http)\n"
                    "-p pixel format (h264) - camera\n"
                    "-s video frame size (WxH) - camera\n"
                    "-r video frame rate - camera\n"
                    "-F processing FPS cap (e.g. -F 2 => process 2 frames/sec)\n"
                    "-fps processing FPS cap (alias of -F)\n"
                    "-Fd drop input packets to also limit decoding (0/1)\n"
                    "-track enable object tracking (true/false)\n"
                    "-roi detection zones - 2 formats:\n"
                    "     Rectangle: -roi \"0.0,0.5,1.0,0.5\" (normalized x,y,w,h)\n"
                    "     Polygon: -roi \"[[263,167],[832,197],[835,497],[313,530]]\" (pixel coords)\n"
                    "     Multiple: -roi \"[[x1,y1],[x2,y2],...];[[x1,y1],[x2,y2],...]\"\n"
                    "-roi-overlap overlap percentage (0-100):\n"
                    "     0 = center point must be in ROI (fast, default)\n"
                    "     50 = 50%% of box must overlap ROI (balanced)\n"
                    "     80 = 80%% of box must overlap ROI (strict)\n"
                    "-o unique object to detect\n"
                    "-b use alpha blend on detected objects (1 ~ 255)\n"
                    "-a accuracy perc (1 ~ 100)\n");
}

/*-------------------------------------------
  Functions
  -------------------------------------------*/
static unsigned char *load_data(FILE *fp, size_t ofst, size_t sz)
{
    unsigned char *data;
    int ret;

    data = NULL;

    if (NULL == fp) {
        return NULL;
    }

    ret = fseek(fp, ofst, SEEK_SET);
    if (ret != 0) {
        fprintf(stderr, "blob seek failure.\n");
        return NULL;
    }

    data = (unsigned char *)malloc(sz);
    if (data == NULL) {
        fprintf(stderr, "buffer malloc failure.\n");
        return NULL;
    }
    ret = fread(data, 1, sz, fp);
    return data;
}

static unsigned char *load_model(char *filename, int *model_size)
{

    FILE *fp;
    unsigned char *data;

    if (!filename)
        return NULL;

    fp = fopen(filename, "rb");
    if (NULL == fp) {
        fprintf(stderr, "Open file %s failed.\n", filename);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);

    data = load_data(fp, 0, size);

    fclose(fp);

    *model_size = size;
    return data;
}

static int saveFloat(const char *file_name, float *output, int element_size)
{
    FILE *fp;
    fp = fopen(file_name, "w");
    for (int i = 0; i < element_size; i++) {
        fprintf(fp, "%.6f\n", output[i]);
    }
    fclose(fp);
    return 0;
}

int main(int argc, char *argv[])
{
    SDL_Event event;
    /* SDL_SysWMinfo info; */  // Commented out for SDL3 compatibility
    /* SDL_version sdl_compiled; */  // Commented out for SDL3 compatibility
    /* SDL_version sdl_linked; */   // Commented out for SDL3 compatibility
    Uint32 wflags = 0 | SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS |
                    SDL_WINDOW_ALWAYS_ON_TOP;
    AVFormatContext *input_ctx = NULL;
    AVStream *video = NULL;
    int video_stream, ret, v4l2 = 0, kmsgrab = 0;
    AVCodecContext *codec_ctx = NULL;
    AVCodec *codec;
    AVFrame *frame;
    AVPacket pkt;
    int lindex, opt;
    char *codec_name = NULL;
    char *video_name = NULL;
    // char *video_name = "/home/rock/weston/apps/videos_rknn/vid-1.mp4";
    // char *video_name = "/home/rock/Videos/jellyfish-5-mbps-hd-hevc.mkv";
    char *pixel_format = NULL, *size_window = NULL;
    AVDictionary *opts = NULL;
    AVDictionaryEntry *dict = NULL;
    AVCodecParameters *codecpar;
    AVInputFormat *ifmt = NULL;
    int nframe = 1;
    int finished = 0;
    int i = 1;
    unsigned int a;

    a = 0;

    while (i < argc) {
        a = hash_me(argv[i++]);
        switch (a) {
        case arg_i:
            video_name = argv[i];
            break;
        case arg_x:
            screen_width = atoi(argv[i]);
            break;
        case arg_y:
            screen_height = atoi(argv[i]);
            break;
        case arg_l:
            screen_left = atoi(argv[i]);
            break;
        case arg_t:
            screen_top = atoi(argv[i]);
            break;
        case arg_f:
            // v4l2 = atoi(argv[i]);
            v4l2 = !strncasecmp(argv[i], "v4l2", 4);
            rtsp = !strncasecmp(argv[i], "rtsp", 4);
            rtmp = !strncasecmp(argv[i], "rtmp", 4);
            http = !strncasecmp(argv[i], "http", 4);
            break;
        case arg_r:
            sensor_frame_rate = argv[i];
            break;
        case arg_F:
        case arg_fps:
            target_fps = atof(argv[i]);
            break;
        case arg_Fd:
            fps_drop_mode = atoi(argv[i]);
            break;
        case arg_track:
            enable_tracking = !strncasecmp(argv[i], "true", 4) || !strncasecmp(argv[i], "1", 1);
            break;
        case arg_roi:
            // Parse ROI zones - support 2 formats:
            // 1. Rectangle: "x,y,w,h" (normalized 0.0-1.0)
            // 2. Polygon: "[[x1,y1],[x2,y2],[x3,y3],[x4,y4]]" (pixel coords)
            {
                fprintf(stderr, "🔍 Parsing ROI parameter: %s\n", argv[i]);
                char *roi_str = strdup(argv[i]);
                g_roi_zones_count = 0;
                
                // Check if it's polygon format (starts with '[[')
                if (roi_str[0] == '[' && roi_str[1] == '[') {
                    // Polygon format: [[x1,y1],[x2,y2],...];[[x1,y1],[x2,y2],...]
                    // Format structure: [[ [x1,y1], [x2,y2], [x3,y3] ]]
                    // But actually: [[x1,y1],[x2,y2],[x3,y3]]
                    char *zone_start = roi_str;
                    
                    while (zone_start && *zone_start && g_roi_zones_count < MAX_ROI_ZONES) {
                        // Skip whitespace and zone delimiters
                        while (*zone_start && (*zone_start == ' ' || *zone_start == ';')) {
                            zone_start++;
                        }
                        
                        // Check for polygon start [[
                        if (*zone_start != '[') break;
                        zone_start++; // Skip first '['
                        if (*zone_start != '[') break;
                        zone_start++; // Skip second '['
                        
                        // Now we're at: x1,y1],[x2,y2],... 
                        // Parse polygon points
                        roi_zone_t *zone = &g_roi_zones[g_roi_zones_count];
                        zone->type = ROI_TYPE_POLYGON;
                        zone->num_points = 0;
                        
                        // Parse first point directly (no leading '[')
                        int x, y;
                        if (sscanf(zone_start, "%d,%d", &x, &y) == 2) {
                            zone->points[zone->num_points].x = x;
                            zone->points[zone->num_points].y = y;
                            zone->num_points++;
                            // Skip to closing ']'
                            while (*zone_start && *zone_start != ']') zone_start++;
                            if (*zone_start == ']') zone_start++;
                        }
                        
                        // Parse remaining points: ,[x2,y2],[x3,y3],...
                        while (*zone_start && zone->num_points < MAX_ROI_POINTS) {
                            // Skip comma and whitespace
                            if (*zone_start == ',') zone_start++;
                            while (*zone_start && *zone_start == ' ') zone_start++;
                            
                            // Check for end of polygon ']'
                            if (*zone_start == ']') break;
                            
                            // Expect '['
                            if (*zone_start != '[') break;
                            zone_start++; // Skip '['
                            
                            if (sscanf(zone_start, "%d,%d", &x, &y) == 2) {
                                zone->points[zone->num_points].x = x;
                                zone->points[zone->num_points].y = y;
                                zone->num_points++;
                                // Skip to closing ']'
                                while (*zone_start && *zone_start != ']') zone_start++;
                                if (*zone_start == ']') zone_start++;
                            } else {
                                break;
                            }
                        }
                        
                        // Skip closing ']' of polygon
                        if (*zone_start == ']') zone_start++;
                        
                        if (zone->num_points >= 3) {
                            g_roi_zones_count++;
                            fprintf(stderr, "✓ Polygon ROI zone %d: %d points\n", 
                                   g_roi_zones_count, zone->num_points);
                            for (int p = 0; p < zone->num_points; p++) {
                                fprintf(stderr, "  Point %d: (%d, %d)\n", 
                                       p + 1, zone->points[p].x, zone->points[p].y);
                            }
                        } else {
                            fprintf(stderr, "Warning: Polygon needs at least 3 points (got %d)\n", zone->num_points);
                        }
                    }
                } else {
                    // Rectangle format: "x,y,w,h;x,y,w,h;..."
                    char *zone_token = strtok(roi_str, ";");
                    
                    while (zone_token != NULL && g_roi_zones_count < MAX_ROI_ZONES) {
                        float x, y, w, h;
                        if (sscanf(zone_token, "%f,%f,%f,%f", &x, &y, &w, &h) == 4) {
                            // Validate values are in [0.0, 1.0] range
                            if (x >= 0.0f && x <= 1.0f && y >= 0.0f && y <= 1.0f &&
                                w >= 0.0f && w <= 1.0f && h >= 0.0f && h <= 1.0f &&
                                (x + w) <= 1.0f && (y + h) <= 1.0f) {
                                g_roi_zones[g_roi_zones_count].type = ROI_TYPE_RECT;
                                g_roi_zones[g_roi_zones_count].x = x;
                                g_roi_zones[g_roi_zones_count].y = y;
                                g_roi_zones[g_roi_zones_count].width = w;
                                g_roi_zones[g_roi_zones_count].height = h;
                                g_roi_zones_count++;
                            } else {
                                fprintf(stderr, "Warning: Invalid ROI zone values (must be 0.0-1.0): %s\n", zone_token);
                            }
                        } else {
                            fprintf(stderr, "Warning: Invalid ROI format (expected x,y,w,h): %s\n", zone_token);
                        }
                        zone_token = strtok(NULL, ";");
                    }
                    
                    if (g_roi_zones_count > 0) {
                        fprintf(stderr, "Rectangle ROI zones configured: %d zone(s)\n", g_roi_zones_count);
                        for (int roi_i = 0; roi_i < g_roi_zones_count; roi_i++) {
                            fprintf(stderr, "  Zone %d: x=%.2f, y=%.2f, w=%.2f, h=%.2f\n", 
                                   roi_i + 1,
                                   g_roi_zones[roi_i].x, g_roi_zones[roi_i].y,
                                   g_roi_zones[roi_i].width, g_roi_zones[roi_i].height);
                        }
                    }
                }
                
                free(roi_str);
                
                if (g_roi_zones_count == 0) {
                    fprintf(stderr, "No valid ROI zones configured, using full frame\n");
                }
            }
            break;
        case arg_roi_overlap:
            g_roi_overlap_percent = atoi(argv[i]);
            if (g_roi_overlap_percent < 0) g_roi_overlap_percent = 0;
            if (g_roi_overlap_percent > 100) g_roi_overlap_percent = 100;
            fprintf(stderr, "✓ ROI overlap mode: %d%% %s\n", 
                    g_roi_overlap_percent,
                    g_roi_overlap_percent == 0 ? "(center point)" : "(area overlap)");
            break;
        case arg_d:
            delay = atoi(argv[i]);
            break;
        case arg_p:
            pixel_format = argv[i];
            break;
        case arg_s:
            sensor_frame_size = argv[i];
            break;
        case arg_m:
            model_name = argv[i];
            break;
        case arg_o:
            obj2det = hash_me(argv[i]);
            break;
        case arg_b:
            alphablend = atoi(argv[i]);
            break;
        case arg_a:
            accur = atoi(argv[i]);
            break;
        default:
            break;
        }
        i++;
    }
    // fprintf(stderr,"%s: %u\n", "-p", hash_me("-p"));
    // fprintf(stderr,"%s: %u\n", "-s", hash_me("-s"));

    if (!video_name) {
        fprintf(stderr, "No stream to play! Please pass an input.\n");
        print_help();
        return -1;
    }
    if (!model_name) {
        fprintf(stderr, "No model to load! Please pass a model.\n");
        print_help();
        return -1;
    }
    if (screen_width <= 0)
        screen_width = 960;
    if (screen_height <= 0)
        screen_height = 540;
    if (screen_left <= 0)
        screen_left = 0;
    if (screen_top <= 0)
        screen_top = 0;

    // If user provided -F/-fps, target_fps is already set. Otherwise, if -r was provided,
    // allow using it as a fallback cap (useful for simple cases like files/rtsp)
    if (target_fps <= 0.0f && sensor_frame_rate) {
        // Accept formats like "30" or "30/1"; atof will read up to non-numeric char
        target_fps = atof(sensor_frame_rate);
    }
    if (target_fps < 0.0f) target_fps = 0.0f;
    if (target_fps > 0.0f)
        fprintf(stderr, "Processing FPS cap: %.2f fps\n", target_fps);

    /* Initialize YOLOv8 model */
    fprintf(stderr, "Loading model: %s\n", model_name);
    memset(&rknn_app_ctx, 0, sizeof(rknn_app_context_t));
    
    ret = init_yolov8_model(model_name, &rknn_app_ctx);
    if (ret != 0) {
        fprintf(stderr, "init_yolov8_model failed! ret=%d\n", ret);
        return -1;
    }
    
    ret = init_post_process();
    if (ret != 0) {
        fprintf(stderr, "init_post_process failed! ret=%d\n", ret);
        return -1;
    }

    // Use model dimensions from context
    width = rknn_app_ctx.model_width;
    height = rknn_app_ctx.model_height;
    channel = rknn_app_ctx.model_channel;
    
    fprintf(stderr, "YOLOv8 model initialized: %dx%dx%d\n", width, height, channel);

    // Initialize tracker
    tracker_init(&global_tracker);
    if (enable_tracking) {
        tracker_set_enabled(&global_tracker, 1);
        fprintf(stderr, "🎯 Object tracking ENABLED\n");
    } else {
        fprintf(stderr, "ℹ️  Object tracking DISABLED (use -track true to enable)\n");
    }

    input_ctx = avformat_alloc_context();
    if (!input_ctx) {
        av_log(0, AV_LOG_ERROR, "Cannot allocate input format (Out of memory?)\n");
        return -1;
    }

    av_dict_set(&opts, "num_capture_buffers", "128", 0);
    if (rtsp) {
        // av_dict_set(&opts, "rtsp_transport", "tcp", 0);
        av_dict_set(&opts, "rtsp_flags", "prefer_tcp", 0);
    }
    if (v4l2) {
        avdevice_register_all();
        ifmt = av_find_input_format("video4linux2");
        if (!ifmt) {
            av_log(0, AV_LOG_ERROR, "Cannot find input format: v4l2\n");
            return -1;
        }
        input_ctx->flags |= AVFMT_FLAG_NONBLOCK;
        if (pixel_format) {
            av_dict_set(&opts, "input_format", pixel_format, 0);
        }
        if (sensor_frame_size)
            av_dict_set(&opts, "video_size", sensor_frame_size, 0);
        if (sensor_frame_rate)
            av_dict_set(&opts, "framerate", sensor_frame_rate, 0);
    }
    if (rtmp) {
        ifmt = av_find_input_format("flv");
        if (!ifmt) {
            av_log(0, AV_LOG_ERROR, "Cannot find input format: flv\n");
            return -1;
        }
        av_dict_set(&opts, "fflags", "nobuffer", 0);
    }

    if (http) {
        av_dict_set(&opts, "fflags", "nobuffer", 0);
    }

    if (avformat_open_input(&input_ctx, video_name, ifmt, &opts) != 0) {
        av_log(0, AV_LOG_ERROR, "Cannot open input file '%s'\n", video_name);
        avformat_close_input(&input_ctx);
        return -1;
    }

    if (avformat_find_stream_info(input_ctx, NULL) < 0) {
        av_log(0, AV_LOG_ERROR, "Cannot find input stream information.\n");
        avformat_close_input(&input_ctx);
        return -1;
    }

    /* find the video stream information */
    ret = av_find_best_stream(input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    if (ret < 0) {
        av_log(0, AV_LOG_ERROR, "Cannot find a video stream in the input file\n");
        avformat_close_input(&input_ctx);
        return -1;
    }
    video_stream = ret;

    /* find the video decoder: ie: h264_rkmpp / h264_rkmpp_decoder */
    codecpar = input_ctx->streams[video_stream]->codecpar;
    if (!codecpar) {
        av_log(0, AV_LOG_ERROR, "Unable to find stream!\n");
        avformat_close_input(&input_ctx);
        return -1;
    }

#if 0
    if (codecpar->codec_id != AV_CODEC_ID_H264) {
        av_log(0, AV_LOG_ERROR, "H264 support only!\n");
        avformat_close_input(&input_ctx);
        return -1;
    }
#endif

    codec = avcodec_find_decoder_by_name("h264_rkmpp");
    if (!codec) {
        av_log(0, AV_LOG_WARNING, "h264_rkmpp decoder not found, trying default decoder!\n");
        codec = avcodec_find_decoder(codecpar->codec_id);
        if (!codec) {
            av_log(0, AV_LOG_ERROR, "Codec not found!\n");
            avformat_close_input(&input_ctx);
            return -1;
        }
    }

    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        av_log(0, AV_LOG_ERROR, "Could not allocate video codec context!\n");
        avformat_close_input(&input_ctx);
        return -1;
    }

    video = input_ctx->streams[video_stream];
    if (avcodec_parameters_to_context(codec_ctx, video->codecpar) < 0) {
        av_log(0, AV_LOG_ERROR, "Error with the codec!\n");
        avformat_close_input(&input_ctx);
        avcodec_free_context(&codec_ctx);
        return -1;
    }

    codec_ctx->pix_fmt = AV_PIX_FMT_DRM_PRIME;
    codec_ctx->coded_height = frame_height;
    codec_ctx->coded_width = frame_width;
    codec_ctx->get_format = get_format;

#if 0
    while (dict = av_dict_get(opts, "", dict, AV_DICT_IGNORE_SUFFIX)) {
        fprintf(stderr, "dict: %s -> %s\n", dict->key, dict->value);
    }
#endif

    /* open it */
    if (avcodec_open2(codec_ctx, codec, &opts) < 0) {
        av_log(0, AV_LOG_ERROR, "Could not open codec!\n");
        avformat_close_input(&input_ctx);
        avcodec_free_context(&codec_ctx);
        return -1;
    }

    av_dict_free(&opts);

    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        avformat_close_input(&input_ctx);
        avcodec_free_context(&codec_ctx);
        return -1;
    }

    /* SDL_VERSION(&sdl_compiled);
    SDL_GetVersion(&sdl_linked);
    SDL_Log("SDL: compiled with=%d.%d.%d linked against=%d.%d.%d",
            sdl_compiled.major, sdl_compiled.minor, sdl_compiled.patch,
            sdl_linked.major, sdl_linked.minor, sdl_linked.patch); */

    // SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles2");
    // SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_ALLOW_LIBDECOR, "0");
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_Log("SDL_Init failed (%s)", SDL_GetError());
        avformat_close_input(&input_ctx);
        avcodec_free_context(&codec_ctx);
        return -1;
    }

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);

    if (SDL_CreateWindowAndRenderer("FF-RKNN", screen_width, screen_height,
                                    wflags,
                                    &window, &renderer) < 0) {
        SDL_Log("SDL_CreateWindowAndRenderer failed (%s)", SDL_GetError());
        goto error_exit;
    }
    if (alphablend) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    }
    // SDL_RenderFillRect(renderer, &rect);
    SDL_SetWindowTitle(window, "rknn yolov5 object detection");
    SDL_SetWindowPosition(window, screen_left, screen_top);

    format = SDL_PIXELFORMAT_RGB24;
    texture = SDL_CreateTexture(renderer, (SDL_PixelFormat)format, SDL_TEXTUREACCESS_STREAMING,
                                screen_width, screen_height);
    if (!texture) {
        av_log(NULL, AV_LOG_FATAL, "Failed to create texturer: %s", SDL_GetError());
        goto error_exit;
    }

    frameSize_rknn = width * height * channel;
    resize_buf = calloc(1, frameSize_rknn);

    frameSize_texture = screen_width * screen_height * channel;
    texture_dst_buf = calloc(1, frameSize_texture);

    if (!resize_buf || !texture_dst_buf) {
        av_log(NULL, AV_LOG_FATAL, "Failed to create texture buf: %dx%d",
               screen_width, screen_height);
        goto error_exit;
    }

    ret = 0;
    while (ret >= 0) {
        if ((ret = av_read_frame(input_ctx, &pkt)) < 0) {
            if (ret == AVERROR(EAGAIN)) {
                ret = 0;
                continue;
            }
            break;
        }
        if (video_stream == pkt.stream_index && pkt.size > 0) {
            // Optional: drop packets to also limit decoding when FPS cap is active
            if (target_fps > 0.0f && fps_drop_mode) {
                Uint32 now = SDL_GetTicks();
                Uint32 min_interval = (Uint32)(1000.0f / target_fps);
                if (last_process_tick != 0 && (now - last_process_tick) < min_interval) {
                    // Drop this packet to reduce decoder load/latency
                    av_packet_unref(&pkt);
                    continue;
                }
            }
            ret = decode_and_display(codec_ctx, frame, &pkt);
            if (delay > 0)
                usleep(delay * 1000);
        }
        av_packet_unref(&pkt);

        // CRITICAL FIX: Process ALL pending SDL events to prevent queue overflow
        int event_count = 0;
        while (SDL_PollEvent(&event)) {
            event_count++;
            switch (event.type) {
            case SDL_EVENT_QUIT:
            {
                finished = 1;
                SDL_Log("Program quit after %ld ticks", event.quit.timestamp);
                break;
            }
            case SDL_EVENT_KEY_DOWN:
            {
                bool withControl = !!(event.key.mod & SDL_KMOD_CTRL);
                bool withShift = !!(event.key.mod & SDL_KMOD_SHIFT);
                bool withAlt = !!(event.key.mod & SDL_KMOD_ALT);

                switch (event.key.key) {
                /* Add hotkeys here */
                case SDLK_ESCAPE:
                    finished = 1;
                    break;
                case SDLK_X:
                    finished = 1;
                    break;
                }
            }
            }
        }
        
        // Warn if event queue is backing up
        if (event_count > 10) {
            fprintf(stderr, "⚠️  SDL event queue backlog: %d events processed\n", event_count);
        }
        
        if (finished) {
            break;
        }
    }
    /* flush the codec */
    decode_and_display(codec_ctx, frame, NULL);

error_exit:

    if (input_ctx)
        avformat_close_input(&input_ctx);
    if (codec_ctx)
        avcodec_free_context(&codec_ctx);
    if (frame) {
        av_frame_free(&frame);
    }
    if (texture_dst_buf) {
        free(texture_dst_buf);
    }
    if (resize_buf) {
        free(resize_buf);
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();

    // Release YOLOv8 model
    ret = release_yolov8_model(&rknn_app_ctx);
    if (ret != 0) {
        fprintf(stderr, "release_yolov8_model failed\n");
    }

    deinit_post_process();

    fprintf(stderr, "Avg FPS: %.1f\n", avg_frmrate);
}
