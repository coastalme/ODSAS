// profile_crossing.h
#ifndef PROFILE_CROSSING_H
#define PROFILE_CROSSING_H

#include <string>

void CreatePointInProfile(const std::string &profileShapefile, const std::string &multiCoastShapefile, const std::string &outputShapefile, double &distancecoast, const std::string& fileformat, const std::string &multicoastDatesFile, bool saveNonIntersectingPairs = false);

#endif // PROFILE_CROSSING_H