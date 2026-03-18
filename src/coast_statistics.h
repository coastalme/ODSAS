#ifndef COAST_STATISTICS_H
#define COAST_STATISTICS_H

#include <unordered_map>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <string>
#include <vector>


// Define the data structures

struct Normal {
    int32_t ID_Profile;
    double NSM;
    double EPR;
    double EPRunc;
    double SCE;
    double LRR;
    double LR2;
    double WLR;
    double WR2;
};

// Data structure for baseline filter
 struct Data {
    int32_t ID_Profile;
    int32_t ID_Coast;
    double Distance;
    double X;  // X coordinate
    double Y;  // Y coordinate
    int32_t ID_nProf_Coast; //new ID composition coast and profile
    std::string Date;
    std::string Hour;
    double Uncertainty;
 };

struct DataCoast {
    int32_t ID_Coast; 
     std::string Date;
     std::string Hour;
     double Uncertainty;
 };

// Function prototypes
std::vector<Data> readShapefile(const std::string& filepath);
void writeShapefile(const std::vector<Data>& data, const std::string& filepath, const std::string& fileformat, OGRSpatialReference* poSRS);

std::vector<Data> filterData(const std::vector<Data>& data, const std::string& position);
std::vector<Data> baseline_filter(const std::string& shp, const std::string& position, const std::string& out_points, const std::string& fileformat);

void coast_rates(const std::vector<Data>& inter_dist, const std::string& shp, const std::string& table_path, const std::string& out_name);
std::vector<DataCoast> readCSVFile(const std::string& filepath);
bool isValidDate(const std::string& date_str);
std::vector<Normal> calculateRates(const std::vector<Data>& inter_dist, const std::vector<DataCoast>& table);

int coast_statistics(int argc, char* argv[]);

#endif // COAST_STATISTICS_H
