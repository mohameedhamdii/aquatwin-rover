/**
 * @file waypoint_nav.c
 * @brief Waypoint navigation implementation
 *
 * GPS-guided patrol using bearing-based differential steering.
 * Uses Haversine for distance and forward azimuth for bearing.
 */

#include "nav/waypoint_nav.h"
#include <math.h>
#include <string.h>

#ifdef BUILD_MODE_ROVER

#define DEG_TO_RAD  (3.14159265358979f / 180.0f)
#define RAD_TO_DEG  (180.0f / 3.14159265358979f)
#define EARTH_R_M   6371000.0f

/* Default arrival radius */
#define DEFAULT_ARRIVAL_RADIUS_M  5.0f

/* Navigation proportional gain */
#define NAV_KP          4.0f
#define NAV_BASE_SPEED  500     /* Base motor speed (out of 1000) */

/**
 * @brief Haversine distance between two points (meters)
 */
static float haversine_distance(int32_t lat1_e7, int32_t lon1_e7,
                                 int32_t lat2_e7, int32_t lon2_e7)
{
    float la1 = (float)lat1_e7 / 1e7f * DEG_TO_RAD;
    float lo1 = (float)lon1_e7 / 1e7f * DEG_TO_RAD;
    float la2 = (float)lat2_e7 / 1e7f * DEG_TO_RAD;
    float lo2 = (float)lon2_e7 / 1e7f * DEG_TO_RAD;

    float dlat = la2 - la1;
    float dlon = lo2 - lo1;

    float a = sinf(dlat / 2) * sinf(dlat / 2) +
              cosf(la1) * cosf(la2) * sinf(dlon / 2) * sinf(dlon / 2);

    float c = 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));

    return EARTH_R_M * c;
}

/**
 * @brief Bearing from point 1 to point 2 (degrees, 0 = North, CW)
 */
static float bearing_to(int32_t lat1_e7, int32_t lon1_e7,
                         int32_t lat2_e7, int32_t lon2_e7)
{
    float la1 = (float)lat1_e7 / 1e7f * DEG_TO_RAD;
    float lo1 = (float)lon1_e7 / 1e7f * DEG_TO_RAD;
    float la2 = (float)lat2_e7 / 1e7f * DEG_TO_RAD;
    float lo2 = (float)lon2_e7 / 1e7f * DEG_TO_RAD;

    float dlon = lo2 - lo1;
    float x = sinf(dlon) * cosf(la2);
    float y = cosf(la1) * sinf(la2) - sinf(la1) * cosf(la2) * cosf(dlon);

    float bearing = atan2f(x, y) * RAD_TO_DEG;
    if (bearing < 0) bearing += 360.0f;

    return bearing;
}

/**
 * @brief Compute shortest angular difference (degrees, signed)
 */
static float angle_diff(float target, float current)
{
    float diff = target - current;
    while (diff > 180.0f)  diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;
    return diff;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

int waypoint_nav_init(waypoint_nav_t *nav, gps_driver_t *gps, motor_driver_t *motors)
{
    if (!nav || !gps || !motors) return -1;

    memset(nav, 0, sizeof(waypoint_nav_t));
    nav->gps    = gps;
    nav->motors = motors;
    nav->arrival_radius_m = DEFAULT_ARRIVAL_RADIUS_M;
    nav->loop_patrol = true;
    nav->state  = NAV_STATE_IDLE;
    nav->initialised = true;

    return 0;
}

int waypoint_nav_add_waypoint(waypoint_nav_t *nav, int32_t lat, int32_t lon)
{
    if (!nav || nav->num_waypoints >= NAV_MAX_WAYPOINTS) return -1;

    nav->waypoints[nav->num_waypoints].latitude  = lat;
    nav->waypoints[nav->num_waypoints].longitude = lon;
    nav->num_waypoints++;

    return 0;
}

void waypoint_nav_start(waypoint_nav_t *nav)
{
    if (!nav || nav->num_waypoints == 0) return;
    nav->current_wp = 0;
    nav->state = NAV_STATE_NAVIGATING;
}

void waypoint_nav_stop(waypoint_nav_t *nav)
{
    if (!nav) return;
    motor_stop(nav->motors);
    nav->state = NAV_STATE_IDLE;
}

void waypoint_nav_update(waypoint_nav_t *nav)
{
    if (!nav || !nav->initialised || nav->state != NAV_STATE_NAVIGATING) return;

    gps_fix_t fix = gps_get_fix(nav->gps);
    if (!fix.fix_valid) {
        motor_stop(nav->motors);
        return;
    }

    waypoint_t *target = &nav->waypoints[nav->current_wp];

    /* Distance to target */
    float dist = haversine_distance(fix.latitude, fix.longitude,
                                     target->latitude, target->longitude);

    /* Check arrival */
    if (dist < nav->arrival_radius_m) {
        nav->current_wp++;
        if (nav->current_wp >= nav->num_waypoints) {
            if (nav->loop_patrol) {
                nav->current_wp = 0;
            } else {
                motor_stop(nav->motors);
                nav->state = NAV_STATE_PATROL_COMPLETE;
                return;
            }
        }
        nav->state = NAV_STATE_ARRIVED;
        return;
    }

    /* Compute desired bearing */
    float desired_bearing = bearing_to(fix.latitude, fix.longitude,
                                        target->latitude, target->longitude);

    /* Current heading from GPS */
    float current_heading = gps_get_heading(nav->gps);

    /* Heading error */
    float error = angle_diff(desired_bearing, current_heading);

    /* Proportional steering */
    float steer = error * NAV_KP;

    /* Compute differential motor speeds */
    int16_t left_speed  = (int16_t)(NAV_BASE_SPEED + steer);
    int16_t right_speed = (int16_t)(NAV_BASE_SPEED - steer);

    /* Clamp */
    if (left_speed > 1000)  left_speed = 1000;
    if (left_speed < -1000) left_speed = -1000;
    if (right_speed > 1000)  right_speed = 1000;
    if (right_speed < -1000) right_speed = -1000;

    motor_set_speeds(nav->motors, left_speed, right_speed);
}

nav_state_t waypoint_nav_get_state(const waypoint_nav_t *nav)
{
    if (!nav) return NAV_STATE_IDLE;
    return nav->state;
}

#endif /* BUILD_MODE_ROVER */
