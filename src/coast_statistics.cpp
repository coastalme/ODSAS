/*!
 *
 * \file coast_statistics.cpp
 * \brief Coastal change rate statistics calculation from profile-coastline intersections
 * \details This module implements comprehensive statistical analysis of coastal change by processing
 *          intersection points between shore-normal profiles and multiple temporal coastlines.
 *          It calculates standard coastal change metrics including EPR, LRR, NSM, and uncertainty measures.
 * 
 * \author Cristina Torrecillas with ChatGPT support
 * \date 2024
 * \copyright GNU General Public License
 *
 * \section OVERVIEW Overview
 * 
 * This module processes temporal coastline data to calculate coastal change statistics for each
 * shore-normal profile. The analysis workflow consists of:
 * 
 * 1. **Data Input**: Reads profile-coastline intersection points and temporal metadata
 * 2. **Data Filtering**: Selects optimal measurement points based on position criteria
 * 3. **Statistical Analysis**: Performs regression analysis to calculate change rates
 * 4. **Results Output**: Exports statistics to both vector files and CSV tables
 * 
 * \section METRICS Calculated Metrics
 * 
 * - **NSM (Net Shoreline Movement)**: Total distance change between oldest and newest positions
 * - **EPR (End Point Rate)**: Linear rate calculated from endpoints (m/year)
 * - **EPRunc (EPR Uncertainty)**: Uncertainty in EPR based on measurement errors
 * - **SCE (Shoreline Change Envelope)**: Maximum distance between any two positions
 * - **LRR (Linear Regression Rate)**: Rate from ordinary least squares regression
 * - **LR2 (Linear R-squared)**: Coefficient of determination for OLS regression
 * - **WLR (Weighted Linear Rate)**: Rate from weighted least squares regression
 * - **WR2 (Weighted R-squared)**: Coefficient of determination for WLS regression
 * 
 * \section DATAFLOW Data Flow
 * 
 * ```
 * Input Shapefile (intersections) → Filter Data → Time Series Analysis → Statistics → Output
 *        ↓                              ↓              ↓                    ↓         ↓
 * Temporal Metadata CSV ────────────────┴──────────────┴────────────────────┴─────────┴
 * ```
 *
 */
/*===============================================================================================================================

 This file is part of ODSAS.

 ODSAS is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 3 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

===============================================================================================================================*/
#define BOOST_ALLOW_DEPRECATED_HEADERS
#define BOOST_BIND_GLOBAL_PLACEHOLDERS
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "ogrsf_frmts.h"
#include "gdal.h"
#include "ogr_api.h"
#include "ogr_geometry.h"
#include <gdal_priv.h>
#include <ogr_api.h>
#include <ogr_spatialref.h>

#include <unordered_map>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <limits>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <Eigen/Dense>

#include "odsas.h"
#include "delineation.h"
#include "coast_statistics.h"

/**
 * \brief Writes filtered intersection data to a vector shapefile
 * 
 * \details Creates a point shapefile containing the filtered profile-coastline intersection data.
 *          Each point represents an optimal measurement location for coastal change analysis.
 * 
 * \param data Vector of filtered Data structures containing intersection points
 * \param filepath Output shapefile path
 * \param format OGR driver format (e.g., "ESRI Shapefile")
 * \param poSRS Spatial reference system for the output
 * 
 * \section FIELDS Output Fields
 * - **ID_nP_nC**: Combined profile and coast identifier (string)
 * - **ID_Profile**: Profile identifier (integer)
 * - **ID_Coast**: Coast/timeline identifier (integer) 
 * - **Distance**: Distance from baseline/reference (real)
 * 
 * \note The function creates point geometries with X,Y coordinates from the Data structure
 */
void writeShapefile(const std::vector<Data>& data, const std::string& filepath, const std::string& format, OGRSpatialReference* poSRS) {
    GDALAllRegister();
    GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(format.c_str());
    if (poDriver == NULL) {
        std::cerr << format.c_str() << " driver not available" << std::endl;
        exit(1);
    }

    GDALDataset* poDS = poDriver->Create(filepath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
    if (poDS == NULL) {
        std::cerr << "Failed to create vectorfile" << filepath.c_str() << std::endl;
        exit(1);
    }

    OGRLayer* poLayer = poDS->CreateLayer("out_points", poSRS, wkbPoint, NULL); //NAME SHP FILTERED
    OGRFieldDefn oFieldID_Prof_Id_coast("ID_nP_nC", OFTString);
    OGRFieldDefn oFieldID_Profile("ID_Profile", OFTInteger);
    OGRFieldDefn oFieldID_Coast("ID_Coast", OFTInteger);
    OGRFieldDefn oFieldDistance("Distance", OFTReal);

    poLayer->CreateField(&oFieldID_Prof_Id_coast);
    poLayer->CreateField(&oFieldID_Profile);
    poLayer->CreateField(&oFieldID_Coast);
    poLayer->CreateField(&oFieldDistance);

    for (const auto& d : data) {
        OGRFeature* poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
        poFeature->SetField("ID_nP_nC", (std::to_string(d.ID_Profile) + "_" + std::to_string(d.ID_Coast)).c_str());
        poFeature->SetField("ID_Profile", d.ID_Profile);
        poFeature->SetField("ID_Coast", d.ID_Coast);
        poFeature->SetField("Distance", d.Distance);

        // Create point geometry
        OGRPoint pt;
        pt.setX(d.X);  // Assuming d.X exists in the Data structure
        pt.setY(d.Y);  // Assuming d.Y exists in the Data structure
        poFeature->SetGeometry(&pt);

        if (poLayer->CreateFeature(poFeature) != OGRERR_NONE) {
            std::cerr << "Failed to create feature in shapefile" << std::endl;
            OGRFeature::DestroyFeature(poFeature);
            GDALClose(poDS);
            exit(1);
        }

        OGRFeature::DestroyFeature(poFeature);
    }

    GDALClose(poDS);
    std::cout << DISTANCEPOINTFNAMENOTICE << filepath << std::endl;
}

/**
 * \brief Reads profile-coastline intersection data from a vector shapefile
 * 
 * \details Extracts intersection point data from a shapefile containing profile-coastline 
 *          intersections. Each feature represents where a shore-normal profile intersects
 *          with a temporal coastline.
 * 
 * \param filepath Path to input shapefile containing intersection data
 * 
 * \return Vector of Data structures containing intersection information
 * 
 * \section REQUIREMENTS Required Fields
 * The input shapefile must contain:
 * - **nProf**: Profile identifier (integer)
 * - **ID**: Coast/timeline identifier (integer)
 * - **Distance**: Distance measurement from baseline (real)
 * - **Point geometry**: X,Y coordinates of intersection
 * 
 * \section PROCESSING Processing Logic
 * 1. Opens and validates the input shapefile
 * 2. Extracts field indices for required attributes
 * 3. Iterates through features, extracting data and geometry
 * 4. Skips features with null values or invalid geometry
 * 5. Reports number of successfully processed points
 * 
 * \note Features with missing fields or invalid geometry are skipped with warnings
 */
//std::vector<Data> readShapefile(const std::string& filepath) {
//OGRSpatialReference* poSRS;
std::vector<Data> readShapefile(const std::string& filepath) {
    
    std::vector<Data> data;

    GDALAllRegister();
    GDALDataset* poDS = static_cast<GDALDataset*>(GDALOpenEx(filepath.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL));
    if (!poDS) {
        std::cerr << "Failed to open vector file: " << filepath.c_str() << std::endl;
        exit(1);
    }

    OGRLayer* poLayer = poDS->GetLayer(0);
    //std::vector<Data> data;
    //OGRSpatialReference* pOGRSpatialRef = poLayer->GetSpatialRef();  // get the CRS of transects = Historical Shorelines
    //poSRS = poLayer->GetSpatialRef();  // get the CRS of transects = Historical Shorelines
    
    OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
    int indexProf = poFDefn->GetFieldIndex("nProf");
    int indexID = poFDefn->GetFieldIndex("ID");
    int indexDistance = poFDefn->GetFieldIndex("Distance");

    if (indexProf == -1 || indexID == -1 || indexDistance == -1) {
        std::cerr << "One or more required fields not found." << std::endl;
        GDALClose(poDS);
        return data;
    }

    OGRFeature* poFeature;
    poLayer->ResetReading();
    int featureCount = 0;
    while ((poFeature = poLayer->GetNextFeature()) != NULL) 
    {
        featureCount++;
        if (!poFeature->IsFieldNull(indexProf) && 
            !poFeature->IsFieldNull(indexID) && 
            !poFeature->IsFieldNull(indexDistance)) 
        {
            Data d;
            d.ID_Profile = poFeature->GetFieldAsInteger(indexProf);
            d.ID_Coast = poFeature->GetFieldAsInteger(indexID);
            d.Distance = poFeature->GetFieldAsDouble(indexDistance);
            
            // Extract geometry
            OGRGeometry *poGeometry = poFeature->GetGeometryRef();
            if (poGeometry != nullptr && wkbFlatten(poGeometry->getGeometryType()) == wkbPoint) 
            {
                OGRPoint *poPoint = dynamic_cast<OGRPoint *>(poGeometry);
                d.X = poPoint->getX();
                d.Y = poPoint->getY();
            } 
            else 
            {
                std::cerr << "Feature " << featureCount << " - Invalid or missing geometry" << std::endl;
                OGRFeature::DestroyFeature(poFeature);
                continue;
            }
            
            data.push_back(d);
        }
        else
        {
            std::cout << "Feature " << featureCount << " - Skipped due to null fields" << std::endl;
        }
        OGRFeature::DestroyFeature(poFeature);
    }

    std::cout << DISTANCEPOINTSNUMNOTICE << data.size() << std::endl;

    GDALClose(poDS);
    return data;
}

/**
 * \brief Filters intersection data to select optimal measurement points per profile-coast combination
 * 
 * \details For each profile-coast pair, selects the best measurement point based on position criteria.
 *          This removes duplicate/multiple intersections and ensures one measurement per profile per timeline.
 * 
 * \param data Vector of raw intersection data
 * \param position Position preference criterion:
 *                 - "1" (SEA): Select seaward-most point (maximum distance)
 *                 - "0" (LAND): Select landward-most point (minimum distance) 
 *                 - "2" (BOTH): Select point closest to baseline (minimum absolute distance)
 * 
 * \return Vector of filtered Data with one point per profile-coast combination
 * 
 * \section ALGORITHM Filtering Algorithm
 * 1. Groups data by profile ID and coast ID using nested hash maps
 * 2. For each profile-coast combination, applies selection criteria:
 *    - SEA mode: Retains point with maximum distance value
 *    - LAND mode: Retains point with minimum distance value  
 *    - BOTH mode: Retains point with minimum absolute distance
 * 3. Converts filtered hash map back to vector format
 * 
 * \section PURPOSE Purpose
 * Handles cases where profiles intersect coastlines multiple times or where
 * measurement positions need standardization across different coastline datasets.
 */
std::vector<Data> filterData(const std::vector<Data>& data, const std::string& position) {
    
    std::unordered_map<int32_t, std::unordered_map<int64_t, Data>> best_data;

    for (const auto& d : data) {
        int32_t profile_key = d.ID_Profile;
        int32_t coast_key = d.ID_Coast;

        if (position == "1") { // SEA
            if (best_data[profile_key].find(coast_key) == best_data[profile_key].end() || d.Distance > best_data[profile_key][coast_key].Distance) {
                best_data[profile_key][coast_key] = d;
            }
        } else if (position == "0") { // LAND
            if (best_data[profile_key].find(coast_key) == best_data[profile_key].end() || d.Distance < best_data[profile_key][coast_key].Distance) {
                best_data[profile_key][coast_key] = d;
            }
        } else if (position == "2") { // BOTH
            if (best_data[profile_key].find(coast_key) == best_data[profile_key].end() || abs(d.Distance) < abs(best_data[profile_key][coast_key].Distance)) {
                best_data[profile_key][coast_key] = d;
            }
        }
    }

    std::vector<Data> filtered_data;
    for (const auto& profile_pair : best_data) {
        for (const auto& coast_pair : profile_pair.second) {
            filtered_data.push_back(coast_pair.second);
        }
    }
    std::cout << CLEANDISTANCEPOINTSNUMNOTICE << filtered_data.size() << std::endl;
    return filtered_data;
}

/**
 * \brief Main data filtering workflow that combines reading, filtering, and writing operations
 * 
 * \details Orchestrates the complete data filtering pipeline from raw intersection data 
 *          to cleaned measurement points ready for statistical analysis.
 * 
 * \param shp Path to input shapefile with raw intersection data
 * \param position Position filtering criterion ("0"=LAND, "1"=SEA, "2"=BOTH)
 * \param out_points Path for output shapefile with filtered points
 * \param outformat OGR output format specification
 * 
 * \return Vector of filtered Data structures
 * 
 * \section WORKFLOW Processing Workflow
 * 1. **Read**: Load raw intersection data from input shapefile
 * 2. **Filter**: Apply position-based filtering to select optimal points
 * 3. **Write**: Export filtered data to new shapefile with inherited CRS
 * 4. **Return**: Provide filtered data for subsequent statistical analysis
 * 
 * \note Spatial reference system is inherited from the input shapefile to maintain
 *       coordinate system consistency across the analysis workflow.
 */
std::vector<Data> baseline_filter(const std::string& shp, const std::string& position, const std::string& out_points, const std::string& outformat) {
    std::vector<Data> data;
    
    data = readShapefile(shp);
    auto filtered_data = filterData(data, position);

    GDALAllRegister();
    GDALDataset* poDS = static_cast<GDALDataset*>(GDALOpenEx(shp.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL));
    if (!poDS) {
        std::cerr << "Failed to open vector file: " << shp.c_str() << std::endl;
        exit(1);
    }

    OGRLayer* poLayer = poDS->GetLayer(0);
    OGRSpatialReference* pOGRSpatialRef = poLayer->GetSpatialRef();  // get the CRS of transects = Historical Shorelines
    writeShapefile(filtered_data, out_points, outformat, pOGRSpatialRef);
    GDALClose(poDS);
    
    return filtered_data;
}


/**
 * \brief Reads temporal metadata from CSV file containing coastline dates and uncertainties
 * 
 * \details Parses a CSV file that provides temporal information for each coastline dataset,
 *          including survey dates, times, and measurement uncertainties.
 * 
 * \param filepath Path to CSV file containing temporal metadata
 * 
 * \return Vector of DataCoast structures with temporal information
 * 
 * \section FORMAT Expected CSV Format
 * The CSV file must have a header row followed by data rows:
 * ```
 * ID,Day (yyyy-mm-dd),Hour (HH:MM:SS),Uncertainty
 * 0,1984-04-12,10:19:10,10
 * 1,1995-08-15,14:30:00,5
 * ```
 * 
 * \section FIELDS Field Descriptions
 * - **ID**: Coast/timeline identifier (integer) - must match ID_Coast in intersection data
 * - **Day**: Survey date in ISO format (yyyy-mm-dd)
 * - **Hour**: Survey time in 24-hour format (HH:MM:SS)
 * - **Uncertainty**: Measurement uncertainty in same units as distance (real)
 * 
 * \section PURPOSE Purpose
 * Provides temporal context for distance measurements, enabling calculation of 
 * change rates and uncertainty propagation in statistical analysis.
 */
// Read the file with the historical shorelines IDs, Date (yyyy-mm-dd), time (HH:MM:SS), Uncertainty 
std::vector<DataCoast> readCSVFile(const std::string& filepath) {
    std::vector<DataCoast> data;
    std::ifstream file(filepath);

    if (!file.is_open()) {
        std::cerr << "Failed to open CSV file" << std::endl;
        return data;
    }

    std::string line;
    std::getline(file, line); // Skip header line = ID,Day (yyyy-mm-dd), Hour (HH:MM:SS), Uncertainty

    // Read line with data e.g. = 0,1984-04-12, 10:19:10,10
    while (std::getline(file, line)) {      
        std::istringstream ss(line);
        std::string token;
        DataCoast d;

        std::getline(ss, token, ',');
        d.ID_Coast = static_cast<int32_t>(std::stoll(token));

        std::getline(ss, token, ',');
        d.Date = token;

        std::getline(ss, token, ',');
        d.Hour = token;

        std::getline(ss, token, ',');
        d.Uncertainty = std::stod(token);
        data.push_back(d);
    }

    file.close();
    std::cout << READDATEFNAMEDNOTICE << filepath << std::endl;
    return data;
}

/**
 * \brief Validates date strings using Boost date parsing
 * 
 * \details Utility function to verify that date strings can be successfully parsed
 *          by the Boost Gregorian date library.
 * 
 * \param date_str Date string to validate (expected format: yyyy-mm-dd)
 * 
 * \return true if date string is valid and parseable, false otherwise
 * 
 * \note Used for error checking during temporal data processing to identify
 *       and handle malformed date entries gracefully.
 */
bool isValidDate(const std::string& date_str) {
    try {
        boost::gregorian::from_string(date_str);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

/**
 * \brief Calculates comprehensive coastal change rate statistics for each profile
 * 
 * \details Core statistical analysis function that processes temporal distance data
 *          to calculate multiple coastal change metrics using both simple and 
 *          regression-based methods.
 * 
 * \param inter_dist Vector of filtered intersection data with distance measurements
 * \param table Vector of temporal metadata for each coastline
 * 
 * \return Vector of Normal structures containing calculated statistics for each profile
 * 
 * \section ANALYSIS Statistical Analysis Methods
 * 
 * ### 1. Endpoint Analysis
 * - **NSM**: Net Shoreline Movement = distance_newest - distance_oldest
 * - **EPR**: End Point Rate = NSM / time_span (m/year)
 * - **EPRunc**: EPR uncertainty = sqrt(unc_newest² + unc_oldest²) / time_span
 * 
 * ### 2. Envelope Analysis  
 * - **SCE**: Shoreline Change Envelope = maximum distance difference between any two measurements
 * 
 * ### 3. Regression Analysis
 * - **Ordinary Least Squares (OLS)**:
 *   - LRR: Linear Regression Rate (slope coefficient)
 *   - LR2: R-squared coefficient of determination
 * - **Weighted Least Squares (WLS)**:
 *   - WLR: Weighted Linear Rate (slope with uncertainty weighting)
 *   - WR2: Weighted R-squared
 * 
 * \section ALGORITHM Processing Algorithm
 * 
 * For each profile:
 * 1. **Data Preparation**:
 *    - Match intersection data with temporal metadata
 *    - Convert dates to years since 1900 for regression
 *    - Calculate uncertainty weights (1/σ²)
 *    - Sort data chronologically by coast ID
 * 
 * 2. **Time Series Construction**:
 *    - Create x-vector: time in decimal years
 *    - Create y-vector: distance measurements  
 *    - Create weight vector: inverse variance weights
 * 
 * 3. **Statistical Calculations**:
 *    - Endpoint statistics: NSM, EPR, EPRunc
 *    - Envelope statistic: SCE
 *    - OLS regression: LRR, LR2
 *    - WLS regression: WLR, WR2
 * 
 * \section WEIGHTING Uncertainty Weighting
 * Weighted regression uses inverse variance weighting:
 * ```
 * weight_i = 1 / uncertainty_i²
 * ```
 * This gives higher influence to more precise measurements.
 * 
 * \section TEMPORAL Time Handling
 * - Dates converted to decimal years since 1900-01-01
 * - Times included for sub-daily precision
 * - Coast IDs assumed to be chronologically ordered
 * 
 * \note Profiles with insufficient valid data points may produce limited statistics
 *       but the function attempts to calculate what metrics are possible.
 */
std::vector<Normal> calculateRates(const std::vector<Data>& inter_dist, const std::vector<DataCoast>& table) {
// Create a map for table data, grouping by ID_Coast
    std::unordered_map<int32_t, DataCoast> table_map;
    for (const auto& t : table) {
        table_map[t.ID_Coast] = t;
    }

    // Create a map for inter_dist data, grouping by ID_Profile
    std::unordered_map<int32_t, std::vector<Data>> data_map;
    for (const auto& d : inter_dist) {
        data_map[d.ID_Profile].push_back(d);
    }

    std::vector<Normal> results;

    // Iterate over each ID_Profile in data_map
    for (auto& entry : data_map) {
        int32_t ID_Profile = entry.first;
        auto& points = entry.second;

        //if (points.size() < 2) {
            //std::cerr << "Not enough points for ID_Profile " << ID_Profile << std::endl;
            //LogStream << "Not enough points for ID_Profile " << ID_Profile << std::endl;
          //  continue;
        //}

        std::vector<int> ID_Coast(points.size());
        std::vector<double> x(points.size());
        std::vector<double> y(points.size());
        std::vector<double> weights(points.size());
        std::vector<double> Uncertainty(points.size());

        size_t valid_points = 0;
        //double min_distance = std::numeric_limits<double>::max();
       // double max_distance = std::numeric_limits<double>::lowest();

        //boost::gregorian::date min_date(boost::gregorian::date(3000, 1, 1)); // Arbitrary far future date
        //boost::gregorian::date max_date(boost::gregorian::date(1900, 1, 1)); // Arbitrary far past date
        //double uncertainty_min_date = 0.0;
        //double uncertainty_max_date = 0.0;

        //double distance_min_date = 0.0;
        //double distance_max_date = 0.0;

        // Collect data for regression
        for (size_t i = 0; i < points.size(); ++i) {
            try {
                auto it = table_map.find(points[i].ID_Coast);
                if (it == table_map.end()) {
                    std::cerr << "No matching DataCoast entry for ID_Coast " << points[i].ID_Coast << std::endl;
                    continue;
                }

                const DataCoast& table_entry = it->second;

                // Store DataCoast information in inter_dist entry
                points[i].ID_Coast = table_entry.ID_Coast;   // assume ID_Coast are sorted by date
                points[i].Date = table_entry.Date;
                points[i].Hour = table_entry.Hour;
                points[i].Uncertainty = table_entry.Uncertainty;

                // Perform date and time parsing
                std::string date_str = table_entry.Date;
                std::string time_str = table_entry.Hour.empty() ? "00:00:00" : table_entry.Hour;

                boost::gregorian::date date = boost::gregorian::from_string(date_str);
                boost::posix_time::time_duration time = boost::posix_time::duration_from_string(time_str);

                boost::posix_time::ptime pt(date, time);

               /*/ if (date < min_date) {
                    min_date = date;
                    uncertainty_min_date = table_entry.Uncertainty;
                    distance_min_date = points[i].Distance;
                }
                if (date > max_date) {
                    max_date = date;
                    uncertainty_max_date = table_entry.Uncertainty;
                    distance_max_date = points[i].Distance;
                }*/

                // Calculate the time difference in years
                double years_since_1900 = (date - boost::gregorian::date(1900, 1, 1)).days() / 365.25; //365.25

                ID_Coast[valid_points] = points[i].ID_Coast;
                x[valid_points] = years_since_1900;
                y[valid_points] = points[i].Distance;
                Uncertainty[valid_points] = table_entry.Uncertainty;
                weights[valid_points] = 1.0 / std::pow(Uncertainty[valid_points],2);

                valid_points++;
            } catch (const std::exception& e) {
                std::cerr << "Error parsing date for point " << i << ": " << e.what()
                          << ". Date: " << table_map[points[i].ID_Coast].Date
                          << ", Hour: " << table_map[points[i].ID_Coast].Hour << std::endl;
                // Skip the invalid entry
                continue;
            }
        }

        //if (valid_points < 2) {
        //    std::cerr << "Not enough valid points for ID_Profile " << ID_Profile << std::endl;
        //    continue;
        //}
        double max_abs_distance = 0.0;

        for (size_t i = 0; i < points.size(); ++i) {
            for (size_t j = i + 1; j < points.size(); ++j) {
                double abs_diff = std::abs(points[i].Distance - points[j].Distance);
                if (abs_diff > max_abs_distance) {
                    max_abs_distance = abs_diff;
                }
            }
        }

        // Ensure we are only using valid points
        ID_Coast.resize(valid_points);
        x.resize(valid_points);
        y.resize(valid_points);
        weights.resize(valid_points);
        Uncertainty.resize(valid_points);

        // Sort x, y, weights based on x (time)  << problems when two dates are too close in time
        // Sort x, y, weights based on ID_Coast instead of time (x): ID_Coast is a unique integer sorted by date already
        std::vector<size_t> sort_indices(valid_points);
        std::iota(sort_indices.begin(), sort_indices.end(), 0);
        std::sort(sort_indices.begin(), sort_indices.end(), [&](size_t i, size_t j) {
            //return x[i] < x[j];
            return ID_Coast[i] < ID_Coast[j];
        });

        std::vector<double> x_sorted(valid_points);
        std::vector<double> y_sorted(valid_points);
        std::vector<double> weights_sorted(valid_points);
        std::vector<double> Uncertainty_sorted(valid_points);

        for (size_t i = 0; i < valid_points; ++i) {
            x_sorted[i] = x[sort_indices[i]];
            y_sorted[i] = y[sort_indices[i]];
            weights_sorted[i] = weights[sort_indices[i]];
            Uncertainty_sorted[i] = Uncertainty[sort_indices[i]];
        }

        // Convert vectors to Eigen vectors
        Eigen::VectorXd x_eigen = Eigen::Map<Eigen::VectorXd>(x_sorted.data(), valid_points);
        Eigen::VectorXd y_eigen = Eigen::Map<Eigen::VectorXd>(y_sorted.data(), valid_points);
        Eigen::VectorXd weights_eigen = Eigen::Map<Eigen::VectorXd>(weights_sorted.data(), valid_points);

        // Print data used in regression for this ID_Profile
        //std::cout << "Data used for ID_Profile " << ID_Profile << ":\n";
        //std::cout << "x (time):\n" << x_eigen.transpose() << "\n";
        //std::cout << "y (distance):\n" << y_eigen.transpose() << "\n";
        //std::cout << "weights:\n" << weights_eigen.transpose() << "\n";

        // Perform Ordinary Least Squares (OLS) regression
        Eigen::MatrixXd A(valid_points, 2);
        A.col(0) = x_eigen;
        A.col(1) = Eigen::VectorXd::Ones(valid_points);

        Eigen::VectorXd solution = A.bdcSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(y_eigen);
        Eigen::VectorXd residuals = y_eigen - A * solution;
        double residual_sum_of_squares = residuals.squaredNorm();
        double total_sum_of_squares = (y_eigen.array() - y_eigen.mean()).square().sum();
        double r_squared = 1 - (residual_sum_of_squares / total_sum_of_squares);

        // Perform Weighted Least Squares (WLS) regression
        Eigen::MatrixXd W = weights_eigen.asDiagonal();
        Eigen::MatrixXd Aw = W * A;
        Eigen::VectorXd yw = W * y_eigen;

        Eigen::VectorXd solution_wls = Aw.bdcSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(yw);
        Eigen::VectorXd residuals_wls = yw - Aw * solution_wls;
        double residual_sum_of_squares_wls = residuals_wls.squaredNorm();
        double total_sum_of_squares_wls = (yw.array() - yw.mean()).square().sum();
        double r_squared_wls = 1 - (residual_sum_of_squares_wls / total_sum_of_squares_wls);

        // Get main distances and dates
        double max_date = x_sorted.back();
        double min_date = x_sorted.front();
        double distance_max_date = y_sorted.back();
        double distance_min_date = y_sorted.front();
        double uncertainty_max_date = Uncertainty_sorted.back();
        double uncertainty_min_date = Uncertainty_sorted.front();

        // Calculate EPR
        //double years_between = (max_date - min_date).days() / 365.25;
        double years_between = max_date - min_date;
        double NSM = distance_max_date - distance_min_date;
        double EPR = NSM / years_between;

        // Calculate EPRunc
        double EPRunc = std::sqrt(std::pow(uncertainty_max_date, 2) + std::pow(uncertainty_min_date, 2)) / years_between;

        // Populate Normal structure with calculated statistics
        Normal n;
        n.ID_Profile = ID_Profile;
        n.NSM = NSM;
        n.EPR = EPR;
        n.EPRunc = EPRunc;
        n.SCE = max_abs_distance;    // SCE calculation
        n.LRR = solution(0);         // Use solution from OLS for LRR
        n.LR2 = r_squared;           // R-squared from OLS
        n.WLR = solution_wls(0);     // Use solution from WLS for WLR
        n.WR2 = r_squared_wls;       // R-squared from WLS

        results.push_back(n);
    }

    return results;
}

/**
 * \brief Main function for coastal change rate analysis and results output
 * 
 * \details Orchestrates the complete coastal statistics workflow by combining
 *          filtered intersection data with temporal metadata to calculate
 *          comprehensive change statistics and export results.
 * 
 * \param inter_dist Vector of filtered intersection data
 * \param shp Path to profiles shapefile to be updated with statistics
 * \param table_path Path to CSV file with temporal metadata
 * \param out_name Path for output CSV statistics file
 * 
 * \section WORKFLOW Processing Workflow
 * 
 * 1. **Input Processing**:
 *    - Read temporal metadata from CSV
 *    - Calculate statistics using filtered intersection data
 * 
 * 2. **Vector File Update**:
 *    - Open existing profiles shapefile for modification
 *    - Add statistical fields if they don't exist:
 *      - NSM, EPR, EPRunc, SCE, LRR, LR2, WLR, WR2
 *    - Join statistics to profiles based on profile ID (nProf field)
 *    - Update feature attributes with calculated values
 * 
 * 3. **CSV Export**:
 *    - Sort results by profile ID for consistent output
 *    - Write statistics to CSV with appropriate precision:
 *      - Distance/rate fields: 1 decimal place
 *      - R-squared values: 4 decimal places
 *    - Include headers for data interpretation
 * 
 * \section INTEGRATION GIS Integration
 * The function updates the existing profiles shapefile in-place, adding
 * statistical attributes that can be used for:
 * - Cartographic visualization of change rates
 * - Spatial analysis of coastal change patterns
 * - Integration with other GIS datasets
 * 
 * \section OUTPUT Output Products
 * 1. **Updated Shapefile**: Original profiles with added statistical attributes
 * 2. **CSV Table**: Tabular statistics suitable for further analysis
 * 
 * \note The profiles shapefile must contain a 'nProf' field that matches
 *       the ID_Profile values in the intersection data.
 */
void coast_rates(const std::vector<Data>& inter_dist, const std::string& shp, const std::string& table_path, const std::string& out_name) {
    
    auto table = readCSVFile(table_path);
    auto rates = calculateRates(inter_dist, table);
 
 // Join to normals shape
 // Register GDAL/OGR drivers
    GDALAllRegister();

    // Open the existing shapefile
    GDALDataset *poDS = (GDALDataset*)GDALOpenEx(shp.c_str(), GDAL_OF_UPDATE, NULL, NULL, NULL);
    if (poDS == NULL) {
        std::cerr << "Failed to open shapefile with profiles." << std::endl;
        return;
    }

    // Access the layer (assuming there is only one layer)
    OGRLayer *poLayer = poDS->GetLayer(0);
    if (poLayer == NULL) {
        std::cerr << "Failed to fetch layer from shapefile." << std::endl;
        GDALClose(poDS);
        return;
    }
// Define a list of field names and their corresponding types (assuming all are double)
    struct {
        const char *name;
        OGRFieldType type;
    } fields[] = {
        {"NSM", OFTReal},
        {"EPR", OFTReal},
        {"EPRunc", OFTReal},
        {"SCE", OFTReal},
        {"LRR", OFTReal},
        {"LR2", OFTReal},
        {"WLR", OFTReal},
        {"WR2", OFTReal}
    };

    // Check if fields exist, create if they don't
    OGRFeatureDefn *poFDefn = poLayer->GetLayerDefn();
    for (auto& field : fields) {
        if (poFDefn->GetFieldIndex(field.name) == -1) {
            OGRFieldDefn oField(field.name, field.type);
            
            // Set precision and width for specific fields
            if (strcmp(field.name, "LR2") == 0 || strcmp(field.name, "WR2") == 0) {
                oField.SetPrecision(4); // Set precision
                oField.SetWidth(10);      // Set width
            } else {
                oField.SetPrecision(1); // Set precision
                oField.SetWidth(10);      // Set width
            }

            if (poLayer->CreateField(&oField) != OGRERR_NONE) {
                std::cerr << "Failed to create field " << field.name << "." << std::endl;
                GDALClose(poDS);
                return;
            }
        }
    }
    // Find the field index for the 'nProf' field (ID_Profile in the shapefile)
    int nFieldIndex = poFDefn->GetFieldIndex("nProf");
    if (nFieldIndex == -1) {
        std::cerr << "Field 'nProf' not found in shapefile." << std::endl;
        GDALClose(poDS);
        return;
    }

    // Iterate over features and update attributes
    poLayer->ResetReading();
    OGRFeature *poFeature;
    while ((poFeature = poLayer->GetNextFeature()) != NULL) {
        // Get ID_Profile (nProf value in shapefile)
        int nProfValue = poFeature->GetFieldAsInteger(nFieldIndex);

        // Find corresponding Normal object in rates
        for (const auto& rate : rates) {
            if (rate.ID_Profile == nProfValue) {
                // Update fields with statistics
                poFeature->SetField("NSM", rate.NSM); 
                poFeature->SetField("EPR", rate.EPR);
                poFeature->SetField("EPRunc", rate.EPRunc);
                poFeature->SetField("SCE", rate.SCE);
                poFeature->SetField("LRR", rate.LRR);
                poFeature->SetField("LR2", rate.LR2);
                poFeature->SetField("WLR", rate.WLR);
                poFeature->SetField("WR2", rate.WR2);

                // Save feature modifications
                if (poLayer->SetFeature(poFeature) != OGRERR_NONE) {
                    std::cerr << "Failed to update feature." << std::endl;
                }

                break; // Exit the loop once found and updated
            }
        }

        OGRFeature::DestroyFeature(poFeature);
    }

    // Close shapefile
    GDALClose(poDS);

    std::cout << PROFILESTATSINVECTORFNAME << shp << std::endl;

    //Statistic file
    std::ofstream out_file(out_name);
    if (!out_file.is_open()) {
        std::cerr << "Failed to open output file" << std::endl;
        return;
    }

    // Write headers to the file
    out_file << "ID_Profile,NSM,EPR,EPRunc,SCE,LRR,LR2,WLR,WR2\n";

        // Sort rates vector based on ID_Profile
    std::vector<Normal> sorted_rates = rates;
    std::sort(sorted_rates.begin(), sorted_rates.end(), 
              [](const Normal& a, const Normal& b) {
                  return a.ID_Profile < b.ID_Profile;
              });

    // Write sorted statistics to the file
    for (const auto& r : sorted_rates) {
        out_file << r.ID_Profile << ",";
        out_file << std::fixed << std::setprecision(1); // Set precision for floating point output
        out_file << r.NSM << ",";
        out_file << r.EPR << ",";
        out_file << r.EPRunc << ",";
        out_file << r.SCE << ",";
        out_file << r.LRR << ",";
        out_file << std::fixed << std::setprecision(4); // Set precision for floating point output
        out_file << r.LR2 << ",";
        out_file << std::fixed << std::setprecision(1); // Set precision for floating point output
        out_file << r.WLR << ",";
        out_file << std::fixed << std::setprecision(4); // Set precision for floating point output
        out_file << r.WR2 << "\n";
    }
    out_file.close();

    std::cout << PROFILESSTATSFNAMENOTICE << out_name << std::endl;
}

/**
 * \brief Main entry point for coastal statistics analysis module
 * 
 * \details Command-line interface for the coastal change rate analysis system.
 *          Processes profile-coastline intersection data to calculate comprehensive
 *          coastal change statistics.
 * 
 * \param argc Number of command line arguments (expected: 7)
 * \param argv Array of command line arguments
 * 
 * \return 0 on success, 1 on error
 * 
 * \section USAGE Command Line Usage
 * ```
 * coast_statistics <intersection_points> <position> <filtered_points> <profiles> <dates_table> <statistics_output> <format>
 * ```
 * 
 * \section ARGUMENTS Arguments Description
 * 
 * **Position 0**: `intersection_points`
 * - Path to shapefile containing raw profile-coastline intersection points
 * - Must contain fields: nProf, ID, Distance, plus point geometry
 * 
 * **Position 1**: `position` 
 * - Filtering criterion for selecting measurement points:
 *   - "0" = LAND (select landward-most intersections)
 *   - "1" = SEA (select seaward-most intersections)  
 *   - "2" = BOTH (select intersections closest to baseline)
 * 
 * **Position 2**: `filtered_points`
 * - Output path for shapefile containing filtered intersection points
 * - Will contain one point per profile-coast combination
 * 
 * **Position 3**: `profiles`
 * - Path to profiles shapefile to be updated with statistics
 * - Must contain 'nProf' field matching intersection data
 * - Will be modified in-place to add statistical fields
 * 
 * **Position 4**: `dates_table`
 * - Path to CSV file containing temporal metadata
 * - Format: ID,Day (yyyy-mm-dd),Hour (HH:MM:SS),Uncertainty
 * 
 * **Position 5**: `statistics_output`
 * - Output path for CSV file containing calculated statistics
 * - Will include all profiles with calculated metrics
 * 
 * **Position 6**: `format`
 * - OGR vector format for output (e.g., "ESRI Shapefile")
 * 
 * \section WORKFLOW Processing Workflow
 * 1. **Data Filtering**: Apply position-based filtering to intersection data
 * 2. **Statistical Analysis**: Calculate comprehensive change rate metrics
 * 3. **Results Export**: Update vector files and create CSV summary
 * 
 * \section OUTPUTS Output Products
 * - **Filtered Points Shapefile**: Cleaned intersection points
 * - **Updated Profiles Shapefile**: Original profiles with added statistics
 * - **Statistics CSV**: Tabular summary of all calculated metrics
 * 
 * \note This function serves as the main interface between ODSAS and the
 *       coastal statistics analysis capabilities.
 */
//*==============================================================================================================================
//
// Main function COAST STATISTICS
//
//==============================================================================================================================*/
int coast_statistics(int argc, char* argv[]) {
    if (argc != 7){
        //std::cerr << "Usage: " << " <pinter_profiles> <position> <pinter_profiles_filtered> <profiles> <table_dates_QC> <table_statistics>" << std::endl;
        return 1;
    }

    std::string pointer_profiles = argv[0]; // output cross_profile
    std::string position = argv[1]; // LAND, from Input file
    std::string pointer_profiles_filtered = argv[2]; // Vector point with non duplicates points
    std::string vector_gis_out_format = argv[6]; // from CDelineation::m_strVectorGISOutFormat << User defined GIS vector output format 

    std::string profiles = argv[3]; // output cliffmetric >> transects with the correct CRS inherited from Historical Shorelines database
    std::string table_dates_QC = argv[4]; // from Input file m_strMulticoastDatesFile
    std::string table_statistics = argv[5]; // OUTPUT
    
    auto filtered_data= baseline_filter(pointer_profiles, position, pointer_profiles_filtered, vector_gis_out_format);

    coast_rates(filtered_data, profiles, table_dates_QC, table_statistics);

    return 0;
}

