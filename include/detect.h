/*
 * detect.h
 * ---------------------------------------------------------------
 * Automatic location detection.
 *
 * Detection order:
 *   1. Environment variables WFS_LAT / WFS_LON (e.g. set by a GPS tool)
 *   2. data/device_location.txt - GPS reading of this device
 *      (simulated sensor file: latitude|longitude|source)
 *   3. The computer's time zone (UTC offset) -> a city in that zone
 *   4. The user's home location (fallback)
 * The nearest known city is found with the Haversine formula.
 */
#ifndef DETECT_H
#define DETECT_H

#include "common.h"

typedef enum {
    DETECT_NONE = 0,
    DETECT_ENVIRONMENT,
    DETECT_GPS_FILE,
    DETECT_TIMEZONE,
    DETECT_HOME
} DetectMethod;

typedef struct {
    int          location_id;
    DetectMethod method;
    double       latitude;       /* device coordinates (if known) */
    double       longitude;
    double       distance_km;    /* device -> city, -1 if unknown */
} DetectResult;

int         detect_location(int home_location, DetectResult *out);
const char *detect_method_name(DetectMethod m);
int         detect_save_device_coords(double lat, double lon);

#endif /* DETECT_H */
