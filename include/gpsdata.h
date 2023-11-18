#pragma once

class GPSData {
    public:
        static double lon,lat;
        static float min_speed,speed,max_speed;
        static float min_alt,alt,max_alt;
        static int satellites;
        static float HDOP;
        GPSData();
};
