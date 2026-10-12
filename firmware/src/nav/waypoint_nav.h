/**
 * @file waypoint_nav.h
 * @brief GPS waypoint navigation for rover mode
 */

#ifndef WAYPOINT_NAV_H
#define WAYPOINT_NAV_H

#include <stdint.h>
#include <stdbool.h>
#include "nav/gps_driver.h"
#include "nav/motor_driver.h"

#define NAV_MAX_WAYPOINTS   16

typedef struct {
    int32_t latitude;   /**< Degrees × 1e7 */
    int32_t longitude;  /**< Degrees × 1e7 */
} waypoint_t;

typedef enum {
    NAV_STATE_IDLE,
    NAV_STATE_NAVIGATING,
    NAV_STATE_ARRIVED,
    NAV_STATE_PATROL_COMPLETE,
} nav_state_t;

typedef struct {
    waypoint_t      waypoints[NAV_MAX_WAYPOINTS];
    uint8_t         num_waypoints;
    uint8_t         current_wp;
    nav_state_t     state;
    float           arrival_radius_m;   /**< Radius to consider "arrived" */
    motor_driver_t  *motors;
    gps_driver_t    *gps;
    bool            loop_patrol;        /**< Restart from first WP on completion */
    bool            initialised;
} waypoint_nav_t;

int         waypoint_nav_init(waypoint_nav_t *nav, gps_driver_t *gps, motor_driver_t *motors);
int         waypoint_nav_add_waypoint(waypoint_nav_t *nav, int32_t lat, int32_t lon);
void        waypoint_nav_start(waypoint_nav_t *nav);
void        waypoint_nav_stop(waypoint_nav_t *nav);
void        waypoint_nav_update(waypoint_nav_t *nav);
nav_state_t waypoint_nav_get_state(const waypoint_nav_t *nav);

#endif /* WAYPOINT_NAV_H */
