#ifndef _RKNN_YOLOV8_DEMO_POSTPROCESS_H_
#define _RKNN_YOLOV8_DEMO_POSTPROCESS_H_

#include <stdint.h>
#include <vector>
#include "rknn_api.h"
#include "common.h"
#include "image_utils.h"

#define OBJ_NAME_MAX_SIZE 64
#define OBJ_NUMB_MAX_SIZE 128
#define OBJ_CLASS_NUM 80
#define NMS_THRESH 0.3
#define BOX_THRESH 0.5          // Ngưỡng confidence cho detection mới (cao = ít false positive)
#define BOX_THRESH_TRACKING 0.1 // Ngưỡng confidence thấp hơn cho tracking (giữ track tốt hơn)

// Class filter using array - List classes you want to detect
// Empty array {} = detect all classes
// Example: {0} = person only
// Example: {0, 2, 7} = person, car, truck
// Example: {0, 2, 3, 5, 7} = person, car, motorcycle, bus, truck
static const int DETECT_CLASS_FILTER[] ={0};  // Empty = detect all
static const int DETECT_CLASS_FILTER_COUNT = sizeof(DETECT_CLASS_FILTER) / sizeof(DETECT_CLASS_FILTER[0]);

// ROI (Region of Interest) - Vùng nhận diện
// Support 2 formats:
// 1. Rectangle (normalized): {x, y, width, height} [0.0-1.0]
// 2. Polygon (pixel coordinates): [[x1,y1],[x2,y2],[x3,y3],[x4,y4]]

#define MAX_ROI_POINTS 16  // Max points per polygon

typedef enum {
    ROI_TYPE_RECT,     // Rectangle with normalized coords
    ROI_TYPE_POLYGON   // Polygon with pixel coords
} roi_type_e;

typedef struct {
    int x;
    int y;
} roi_point_t;

typedef struct {
    roi_type_e type;
    
    // For rectangle type
    float x;      // Tọa độ X chuẩn hóa (0.0 = trái, 1.0 = phải)
    float y;      // Tọa độ Y chuẩn hóa (0.0 = trên, 1.0 = dưới)  
    float width;  // Chiều rộng chuẩn hóa
    float height; // Chiều cao chuẩn hóa
    
    // For polygon type
    roi_point_t points[MAX_ROI_POINTS];
    int num_points;
} roi_zone_t;

// Global ROI zones - will be set from command line
#define MAX_ROI_ZONES 16
extern roi_zone_t g_roi_zones[MAX_ROI_ZONES];
extern int g_roi_zones_count;

// ROI overlap mode:
// 0 = center point (default) - object center must be inside ROI
// 1-100 = percentage overlap - X% of bounding box area must be inside ROI
extern int g_roi_overlap_percent;

// Helper function to check if a box is inside any ROI zone
bool is_box_in_roi(const image_rect_t* box, int img_width, int img_height);

/*(

    Giải thích các tham số:
    1. NMS_THRESH (Non-Maximum Suppression Threshold) = 0.45

    Mục đích: Loại bỏ các bounding box CHỒNG LÊN NHAU của cùng một đối tượng
    Cách hoạt động:
    Tính IoU (Intersection over Union) giữa các box
    Nếu IoU > NMS_THRESH → box có confidence thấp hơn sẽ bị loại bỏ
    Giá trị:
    Cao (0.6-0.7): Cho phép nhiều box chồng lên nhau hơn → phát hiện nhiều đối tượng gần nhau
    Thấp (0.3-0.4): Loại bỏ nhiều box chồng lên nhau → ít false positives
    Mặc định: 0.45 - cân bằng tốt
    2. BOX_THRESH (Confidence Threshold) = 0.15

    Mục đích: Lọc các detection có confidence QUÁ THẤP
    Không liên quan đến việc loại bỏ box chồng lên nhau

)*/

typedef struct {
    image_rect_t box;
    float prop;
    int cls_id;
} object_detect_result;

typedef struct {
    int id;
    int count;
    object_detect_result results[OBJ_NUMB_MAX_SIZE];
} object_detect_result_list;

int init_post_process();
void deinit_post_process();
char *coco_cls_to_name(int cls_id);
int post_process(rknn_app_context_t *app_ctx, void *outputs, letterbox_t *letter_box, float conf_threshold, float nms_threshold, object_detect_result_list *od_results, int src_img_width, int src_img_height);

void deinitPostProcess();
#endif //_RKNN_YOLOV8_DEMO_POSTPROCESS_H_
