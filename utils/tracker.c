// Simple tracking implementation
// Based on BoT-SORT algorithm concepts

#include "tracker.h"
#include "../postprocess.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Calculate IoU (Intersection over Union) between two boxes
static float calculate_iou(image_rect_t* box1, image_rect_t* box2) {
    int x1_min = box1->left;
    int y1_min = box1->top;
    int x1_max = box1->right;
    int y1_max = box1->bottom;
    
    int x2_min = box2->left;
    int y2_min = box2->top;
    int x2_max = box2->right;
    int y2_max = box2->bottom;
    
    // Calculate intersection
    int inter_x_min = (x1_min > x2_min) ? x1_min : x2_min;
    int inter_y_min = (y1_min > y2_min) ? y1_min : y2_min;
    int inter_x_max = (x1_max < x2_max) ? x1_max : x2_max;
    int inter_y_max = (y1_max < y2_max) ? y1_max : y2_max;
    
    if (inter_x_max <= inter_x_min || inter_y_max <= inter_y_min) {
        return 0.0f;
    }
    
    int inter_area = (inter_x_max - inter_x_min) * (inter_y_max - inter_y_min);
    
    // Calculate union
    int box1_area = (x1_max - x1_min) * (y1_max - y1_min);
    int box2_area = (x2_max - x2_min) * (y2_max - y2_min);
    int union_area = box1_area + box2_area - inter_area;
    
    if (union_area == 0) {
        return 0.0f;
    }
    
    return (float)inter_area / (float)union_area;
}

// Calculate center of box
static void get_box_center(image_rect_t* box, float* cx, float* cy) {
    *cx = (box->left + box->right) / 2.0f;
    *cy = (box->top + box->bottom) / 2.0f;
}

// Initialize tracker
void tracker_init(Tracker* tracker) {
    if (!tracker) return;
    
    memset(tracker, 0, sizeof(Tracker));
    tracker->next_id = 1;
    tracker->enabled = 0;  // Disabled by default
}

// Enable/disable tracking
void tracker_set_enabled(Tracker* tracker, int enabled) {
    if (!tracker) return;
    tracker->enabled = enabled;
    if (enabled) {
        printf("🎯 Tracking ENABLED\n");
    } else {
        printf("🎯 Tracking DISABLED\n");
    }
}

// Check if tracking is enabled
int tracker_is_enabled(Tracker* tracker) {
    if (!tracker) return 0;
    return tracker->enabled;
}

// Update tracker with new detections
void tracker_update(Tracker* tracker, object_detect_result* detections, int num_detections,
                   TrackEventCallback callback) {
    if (!tracker || !tracker->enabled) return;
    
    const float IOU_THRESHOLD = 0.3f;  // Minimum IoU for matching
    
    // Mark all existing tracks as not updated
    int matched[MAX_TRACKED_OBJECTS] = {0};
    int det_matched[OBJ_NUMB_MAX_SIZE] = {0};
    
    // Match detections to existing tracks using IoU
    for (int i = 0; i < tracker->count; i++) {
        TrackedObject* track = &tracker->objects[i];
        
        if (track->state == TRACK_STATE_LOST_PERMANENT) {
            continue;
        }
        
        float best_iou = 0.0f;
        int best_det_idx = -1;
        
        // Find best matching detection
        for (int j = 0; j < num_detections; j++) {
            if (det_matched[j]) continue;
            
            // Only match same class
            if (detections[j].cls_id != track->cls_id) continue;
            
            float iou = calculate_iou(&track->box, &detections[j].box);
            if (iou > best_iou && iou > IOU_THRESHOLD) {
                best_iou = iou;
                best_det_idx = j;
            }
        }
        
        if (best_det_idx >= 0) {
            // Match found - update track
            TrackState old_state = track->state;
            
            // Calculate velocity
            float cx, cy;
            get_box_center(&detections[best_det_idx].box, &cx, &cy);
            track->vx = cx - track->prev_cx;
            track->vy = cy - track->prev_cy;
            track->prev_cx = cx;
            track->prev_cy = cy;
            
            // Update track
            track->box = detections[best_det_idx].box;
            track->confidence = detections[best_det_idx].prop;
            track->time_since_update = 0;
            track->age++;
            
            if (track->state == TRACK_STATE_LOST_TEMP) {
                track->state = TRACK_STATE_TRACKED;
                printf("🔄 ID %d: RE-TRACKED (was lost temporarily)\n", track->track_id);
                if (callback) callback(track->track_id, old_state, track->state);
            } else if (track->state == TRACK_STATE_NEW) {
                track->state = TRACK_STATE_TRACKED;
            }
            
            matched[i] = 1;
            det_matched[best_det_idx] = 1;
        } else {
            // No match - track lost
            track->time_since_update++;
            
            TrackState old_state = track->state;
            
            if (track->time_since_update > MAX_DISAPPEARED_FRAMES) {
                track->state = TRACK_STATE_LOST_PERMANENT;
                printf("❌ ID %d: LOST PERMANENTLY (disappeared for %d frames)\n", 
                       track->track_id, track->time_since_update);
                if (callback) callback(track->track_id, old_state, track->state);
            } else if (track->time_since_update > MAX_LOST_FRAMES) {
                if (track->state != TRACK_STATE_LOST_TEMP) {
                    track->state = TRACK_STATE_LOST_TEMP;
                    printf("⚠️  ID %d: LOST TEMPORARILY (searching...)\n", track->track_id);
                    if (callback) callback(track->track_id, old_state, track->state);
                }
            }
        }
    }
    
    // Create new tracks for unmatched detections
    for (int j = 0; j < num_detections; j++) {
        if (det_matched[j]) continue;
        
        // Find empty slot or replace permanently lost track
        int slot = -1;
        for (int i = 0; i < MAX_TRACKED_OBJECTS; i++) {
            if (i >= tracker->count) {
                slot = i;
                tracker->count++;
                break;
            }
            if (tracker->objects[i].state == TRACK_STATE_LOST_PERMANENT) {
                slot = i;
                break;
            }
        }
        
        if (slot >= 0) {
            TrackedObject* new_track = &tracker->objects[slot];
            memset(new_track, 0, sizeof(TrackedObject));
            
            new_track->track_id = tracker->next_id++;
            new_track->state = TRACK_STATE_NEW;
            new_track->box = detections[j].box;
            new_track->cls_id = detections[j].cls_id;
            new_track->confidence = detections[j].prop;
            new_track->age = 1;
            new_track->time_since_update = 0;
            
            get_box_center(&new_track->box, &new_track->prev_cx, &new_track->prev_cy);
            
            printf("✨ ID %d: NEW OBJECT DETECTED (class: %d, conf: %.2f)\n", 
                   new_track->track_id, new_track->cls_id, new_track->confidence);
            
            if (callback) callback(new_track->track_id, TRACK_STATE_LOST_PERMANENT, TRACK_STATE_NEW);
        }
    }
    
    // CRITICAL FIX: Periodic cleanup of old lost tracks to prevent memory buildup
    // Compact the tracker array by removing permanently lost tracks
    static int cleanup_counter = 0;
    cleanup_counter++;
    if (cleanup_counter > 100) { // Cleanup every 100 frames
        int write_idx = 0;
        for (int read_idx = 0; read_idx < tracker->count; read_idx++) {
            if (tracker->objects[read_idx].state != TRACK_STATE_LOST_PERMANENT) {
                if (write_idx != read_idx) {
                    tracker->objects[write_idx] = tracker->objects[read_idx];
                }
                write_idx++;
            }
        }
        int removed = tracker->count - write_idx;
        tracker->count = write_idx;
        if (removed > 0) {
            printf("🧹 Tracker cleanup: removed %d permanently lost tracks (active: %d)\n", 
                   removed, tracker->count);
        }
        cleanup_counter = 0;
    }
}

// Get all active tracks
int tracker_get_active_tracks(Tracker* tracker, TrackedObject* out_tracks, int max_tracks) {
    if (!tracker || !out_tracks) return 0;
    
    int count = 0;
    for (int i = 0; i < tracker->count && count < max_tracks; i++) {
        if (tracker->objects[i].state != TRACK_STATE_LOST_PERMANENT) {
            out_tracks[count++] = tracker->objects[i];
        }
    }
    
    return count;
}
