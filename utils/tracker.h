// Simple tracking header for object tracking
// Based on BoT-SORT algorithm concepts

#ifndef _TRACKER_H_
#define _TRACKER_H_

#include <stdint.h>
#include "common.h"
#include "yolov8.h"  // Includes postprocess.h which defines object_detect_result

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_TRACKED_OBJECTS 128
#define MAX_LOST_FRAMES 30      // Frames before considering object lost temporarily
#define MAX_DISAPPEARED_FRAMES 90  // Frames before considering object completely lost
#define MIN_HITS_TO_CONFIRM 5   // Minimum hits before confirming as NEW track
#define CONFIRMATION_TIME_WINDOW 30  // Time window (frames) to collect MIN_HITS_TO_CONFIRM

typedef enum {
    TRACK_STATE_TENTATIVE = 0,     // Tentative track (not yet confirmed)
    TRACK_STATE_NEW,               // New object confirmed (just reached min hits)
    TRACK_STATE_TRACKED,           // Object being tracked
    TRACK_STATE_LOST_TEMP,         // Temporarily lost (still searching)
    TRACK_STATE_LOST_PERMANENT     // Permanently lost
} TrackState;

typedef struct {
    int track_id;                  // Unique tracking ID
    TrackState state;              // Current state
    image_rect_t box;              // Current bounding box
    int cls_id;                    // Class ID
    float confidence;              // Detection confidence
    int age;                       // How many frames tracked
    int time_since_update;         // Frames since last update
    int hits;                      // Number of detection hits in time window
    int first_seen_frame;          // Frame number when first detected
    
    // For velocity estimation
    float vx, vy;                  // Velocity in x, y
    float prev_cx, prev_cy;        // Previous center
} TrackedObject;

typedef struct {
    TrackedObject objects[MAX_TRACKED_OBJECTS];
    int count;
    int next_id;
    int enabled;
    int current_frame;             // Current frame number
} Tracker;

// Callback function type for tracking events
typedef void (*TrackEventCallback)(int track_id, TrackState old_state, TrackState new_state);

// Initialize tracker
void tracker_init(Tracker* tracker);

// Update tracker with new detections
void tracker_update(Tracker* tracker, object_detect_result* detections, int num_detections,
                   TrackEventCallback callback);

// Get all active tracks
int tracker_get_active_tracks(Tracker* tracker, TrackedObject* out_tracks, int max_tracks);

// Enable/disable tracking
void tracker_set_enabled(Tracker* tracker, int enabled);

// Check if tracking is enabled
int tracker_is_enabled(Tracker* tracker);

#ifdef __cplusplus
}
#endif

#endif // _TRACKER_H_
