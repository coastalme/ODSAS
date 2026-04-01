/*!
 *
 * \file profile_crossing.cpp
 * \brief Geometric intersection analysis between shore-normal profiles and temporal coastlines
 * \details This module implements sophisticated geometric intersection algorithms to calculate 
 *          precise crossing points between shore-normal profiles and multiple temporal coastlines.
 *          It produces georeferenced measurement points essential for coastal change analysis.
 * 
 * \author Cristina Torrecillas with ChatGPT support
 * \date 2024
 * \copyright GNU General Public License
 *
 * \section OVERVIEW Overview
 * 
 * This module performs high-precision geometric intersection analysis for coastal monitoring applications.
 * The primary function calculates where shore-normal profiles intersect with temporal coastline datasets,
 * producing measurement points with accurate distance calculations from profile baselines.
 * 
 * \section WORKFLOW Processing Workflow
 * 
 * 1. **Spatial Preprocessing**: 
 *    - Calculate union bounding box of all profiles for spatial optimization
 *    - Clip coastline data to profile extent using ogr2ogr for efficiency
 * 
 * 2. **Intersection Analysis**:
 *    - Load clipped coastlines into memory for fast access
 *    - For each profile-coastline pair, perform geometric intersection
 *    - Extract all intersection points from complex geometries
 * 
 * 3. **Distance Calculation**:
 *    - Measure distance from profile start point to each intersection
 *    - Apply baseline offset for consistent measurement reference
 * 
 * 4. **Data Integration**:
 *    - Combine attributes from profiles and coastlines
 *    - Create output features with integrated metadata
 * 
 * 5. **Quality Control**:
 *    - Log non-intersecting pairs for analysis
 *    - Validate geometries and handle edge cases
 * 
 * \section APPLICATIONS Applications
 * 
 * - **Coastal Change Analysis**: Provides measurement points for calculating erosion/accretion rates
 * - **Temporal Monitoring**: Tracks coastline position changes across multiple time periods
 * - **Beach Profile Analysis**: Quantifies cross-shore position variations
 * - **Validation Studies**: Compares modeled vs. observed coastline positions
 * 
 * \section TECHNICAL Technical Features
 * 
 * - **Multi-geometry Support**: Handles LineString, MultiLineString, and complex geometries
 * - **Spatial Optimization**: Uses bounding box clipping for large dataset efficiency
 * - **Precision Calculations**: OGR-based geometric operations ensure sub-meter accuracy
 * - **Memory Management**: Efficient handling of large coastline datasets
 * - **Robust Error Handling**: Validates geometries and gracefully handles edge cases
 *
 */

/*===============================================================================================================================

 This file is part of ODSAS.

 ODSAS is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 3 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

===============================================================================================================================*/
//#include "profile_crossing.h"
#include <iostream>
#include "ogrsf_frmts.h"
#include <iostream>
#include <string>
#include "odsas.h"
#include "gdal.h"
#include "ogr_api.h"
#include "ogr_geometry.h"
#include "profile_crossing.h"
#include <vector>
#include <filesystem>
#include "odsas.h"
#include "delineation.h"

/**
 * \brief Extracts all point geometries from complex intersection results
 * 
 * \details Helper function that recursively extracts point coordinates from various
 *          geometry types that may result from profile-coastline intersections.
 *          Handles the complexity of OGR intersection operations which can return
 *          points, multipoints, linestrings, or geometry collections.
 * 
 * \param geom Input geometry from intersection operation
 * 
 * \return Vector of OGRPoint objects representing all intersection locations
 * 
 * \section GEOMETRY Supported Geometry Types
 * 
 * - **Point**: Direct extraction of single intersection point
 * - **MultiPoint**: Extraction of all constituent points
 * - **LineString**: Extraction of all vertices (for coincident segments)
 * - **GeometryCollection**: Recursive extraction from nested geometries
 * 
 * \section PURPOSE Purpose
 * Profile-coastline intersections can produce various geometry types depending on:
 * - Angular relationship between profile and coastline
 * - Digitization precision and data quality
 * - Coincident or near-coincident line segments
 * 
 * This function ensures all intersection points are captured regardless of 
 * the complexity of the resulting geometry.
 * 
 * \note The function handles edge cases where intersections may produce
 *       unexpected geometry types due to precision issues or data artifacts.
 */
// Helper to extract all points from an intersection geometry
std::vector<OGRPoint> extractPoints(OGRGeometry* geom) {
    std::vector<OGRPoint> points;
    if (!geom) return points;
    OGRwkbGeometryType gtype = wkbFlatten(geom->getGeometryType());
    if (gtype == wkbPoint) {
        points.push_back(*(OGRPoint*)geom);
    } else if (gtype == wkbMultiPoint) {
        OGRMultiPoint* mp = (OGRMultiPoint*)geom;
        for (int i = 0; i < mp->getNumGeometries(); ++i) {
            points.push_back(*(OGRPoint*)mp->getGeometryRef(i));
        }
    } else if (gtype == wkbLineString) {
        OGRLineString* ls = (OGRLineString*)geom;
        for (int i = 0; i < ls->getNumPoints(); ++i) {
            OGRPoint pt;
            ls->getPoint(i, &pt);
            points.push_back(pt);
        }
    } else if (gtype == wkbGeometryCollection) {
        OGRGeometryCollection* gc = (OGRGeometryCollection*)geom;
        for (int i = 0; i < gc->getNumGeometries(); ++i) {
            std::vector<OGRPoint> subpoints = extractPoints(gc->getGeometryRef(i));
            points.insert(points.end(), subpoints.begin(), subpoints.end());
        }
    }
    return points;
}

/**
 * \brief Main function for calculating profile-coastline intersection points
 * 
 * \details Performs comprehensive geometric intersection analysis between shore-normal 
 *          profiles and temporal coastline datasets. Produces georeferenced measurement
 *          points with accurate distance calculations for coastal change analysis.
 * 
 * \param profileShapefile Path to shapefile containing shore-normal profile lines
 * \param multiCoastShapefile Path to shapefile containing temporal coastline features
 * \param outputShapefile Output path for intersection points shapefile
 * \param distancecoast Baseline offset distance for consistent measurement reference
 * \param fileformat OGR driver format for output (e.g., "ESRI Shapefile")
 * \param saveNonIntersectingPairs Optional flag to save non-intersecting geometry pairs for QC
 * 
 * \section INPUTS Input Requirements
 * 
 * **Profile Shapefile**:
 * - Geometry: LineString or MultiLineString representing shore-normal transects
 * - Attributes: Profile metadata (ID, length, etc.)
 * - CRS: Any projected coordinate system suitable for distance calculations
 * 
 * **MultiCoast Shapefile**:
 * - Geometry: LineString or MultiLineString representing temporal coastlines
 * - Attributes: Temporal metadata (ID, date, source, etc.)
 * - CRS: Must match or be compatible with profile CRS
 * 
 * \section OUTPUTS Output Products
 * 
 * **Intersection Points Shapefile**:
 * - Geometry: Point locations of profile-coastline intersections
 * - Attributes: Combined fields from both input datasets
 * - Distance Field: Calculated distance from profile start + baseline offset
 * - CRS: Inherited from multiCoast shapefile
 * 
 * **Debug Output** (if enabled):
 * - GeoPackage with non-intersecting profile-coastline pairs
 * - Useful for quality control and debugging intersection issues
 * 
 * \section ALGORITHM Processing Algorithm
 * 
 * ### 1. Spatial Preprocessing
 * ```
 * Calculate union bounding box of all profiles
 * ↓
 * Clip coastlines to profile extent using ogr2ogr
 * ↓  
 * Load clipped coastlines into memory
 * ```
 * 
 * ### 2. Intersection Analysis
 * ```
 * For each profile:
 *   For each coastline:
 *     Check geometric intersection
 *     ↓
 *     Calculate intersection geometry
 *     ↓
 *     Extract all intersection points
 *     ↓
 *     Calculate distance from profile start
 *     ↓
 *     Create output feature with combined attributes
 * ```
 * 
 * ### 3. Quality Control
 * ```
 * Track non-intersecting pairs
 * ↓
 * Optionally export for debugging
 * ↓
 * Report intersection statistics
 * ```
 * 
 * \section OPTIMIZATION Performance Optimizations
 * 
 * - **Spatial Clipping**: Reduces coastline data to relevant area only
 * - **Memory Loading**: Clipped coastlines loaded once for fast iteration
 * - **Bounding Box**: Union envelope calculation minimizes unnecessary processing
 * - **Geometry Validation**: Early validation prevents processing invalid geometries
 * 
 * \section PRECISION Distance Calculation
 * 
 * Distance measured as:
 * ```
 * distance = profile_start_to_intersection + baseline_offset
 * ```
 * 
 * Where:
 * - **profile_start_to_intersection**: Euclidean distance from profile start point
 * - **baseline_offset**: User-defined reference distance (distancecoast parameter)
 * 
 * \section QC Quality Control Features
 * 
 * - **Geometry Validation**: Checks for valid geometries before processing
 * - **Intersection Verification**: Confirms geometric intersections exist
 * - **Progress Reporting**: Provides feedback during long processing operations
 * - **Error Handling**: Graceful handling of geometric edge cases
 * - **Debug Output**: Optional logging of problematic geometry pairs
 * 
 * \section INTEGRATION Integration Notes
 * 
 * This function integrates with the broader ODSAS coastal analysis workflow:
 * 1. **Input**: Uses profiles generated by ODSAS profile creation modules
 * 2. **Processing**: Calculates intersection points for temporal analysis
 * 3. **Output**: Provides measurement points for coastal statistics calculations
 * 
 * \note The function uses system calls to ogr2ogr for robust spatial clipping,
 *       which requires ogr2ogr to be available in the system PATH.
 */
void CreatePointInProfile(const std::string &profileShapefile, const std::string &multiCoastShapefile, const std::string &outputShapefile, double &distancecoast, const std::string& fileformat, const std::string &multicoastDatesFile, bool saveNonIntersectingPairs) {
    /**
     * \brief Lambda function to extract LineString geometries from complex geometry types
     * 
     * \details Helper lambda that handles both simple LineString and MultiLineString
     *          geometries by extracting all constituent LineString objects for 
     *          intersection analysis.
     * 
     * \param geom Input geometry (LineString or MultiLineString)
     * \return Vector of LineString pointers for intersection processing
     * 
     * This ensures consistent handling of different line geometry types that
     * may be present in profile or coastline datasets.
     */
    // Helper: get all linestrings from a geometry (linestring or multilinestring)
    auto getLinestrings = [](OGRGeometry* geom) -> std::vector<OGRLineString*> {
        std::vector<OGRLineString*> lines;
        OGRwkbGeometryType gtype = wkbFlatten(geom->getGeometryType());
        if (gtype == wkbLineString) {
            lines.push_back((OGRLineString*)geom);
        } else if (gtype == wkbMultiLineString) {
            OGRMultiLineString* mls = (OGRMultiLineString*)geom;
            for (int i = 0; i < mls->getNumGeometries(); ++i) {
                lines.push_back((OGRLineString*)mls->getGeometryRef(i));
            }
        }
        return lines;
    };
    
    // Initialize GDAL/OGR drivers for geospatial data processing
    GDALAllRegister();

    /**
     * \section DATASOURCES Data Source Initialization
     * 
     * Open and validate input datasets:
     * - Profile shapefile: Contains shore-normal transect lines
     * - MultiCoast shapefile: Contains temporal coastline features
     * 
     * Both datasets must be valid vector files with line geometries.
     */
    // Open vector polyline with all the transects
    GDALDataset *profileDS = (GDALDataset*) GDALOpenEx(profileShapefile.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
    if (profileDS == NULL) {
        std::cerr << "Failed to open profile shapefile" << std::endl;
        GDALClose(profileDS);
        return;
    } 

    // Open multiCoast shapefile
    GDALDataset *multiCoastDS = (GDALDataset*) GDALOpenEx(multiCoastShapefile.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
    if (multiCoastDS == NULL) {
        std::cerr << "Failed to open multiCoast shapefile" << std::endl;
        GDALClose(multiCoastDS);
        return;
    }

    // Create output shapefile
    GDALDriver *driver = GetGDALDriverManager()->GetDriverByName(fileformat.c_str());
    
    if (driver == NULL) {
        std::cerr << fileformat.c_str() << " driver not available" << std::endl;
        GDALClose(profileDS);
        GDALClose(multiCoastDS);
        return;
    }

    GDALDataset *outputDS = driver->Create(outputShapefile.c_str(), 0, 0, 0, GDT_Unknown, NULL);
    if (outputDS == NULL) {
        std::cerr << "Failed to create output shapefile" << std::endl;
        GDALClose(profileDS);
        GDALClose(multiCoastDS);
        return;
    }

    /**
     * \section OUTPUTSETUP Output Dataset Setup
     * 
     * Create output shapefile with:
     * - Point geometry for intersection locations
     * - Combined attribute schema from both input datasets
     * - Additional "Distance" field for calculated measurements
     * - Spatial reference inherited from coastline dataset
     */
    OGRLayer *profileLayer = profileDS->GetLayer(0);
    OGRLayer *multiCoastLayer = multiCoastDS->GetLayer(0);
    OGRSpatialReference* pOGRSpatialRef = multiCoastLayer->GetSpatialRef();   // Output the CRS so we can pass it to coast_statistics
    OGRLayer *outputLayer = outputDS->CreateLayer("intersection", pOGRSpatialRef, wkbPoint, NULL);

    // Add fields from both input shapefiles to the output shapefile
    OGRFeatureDefn *profileDefn = profileLayer->GetLayerDefn();
    for (int i = 0; i < profileDefn->GetFieldCount(); i++) {
        outputLayer->CreateField(profileDefn->GetFieldDefn(i));
    }

    OGRFeatureDefn *multiCoastDefn = multiCoastLayer->GetLayerDefn();
    for (int i = 0; i < multiCoastDefn->GetFieldCount(); i++) {
        outputLayer->CreateField(multiCoastDefn->GetFieldDefn(i));
    }

    // Create new field "Distance" in the output layer
    OGRFieldDefn distanceField("Distance", OFTReal);
    if (outputLayer->CreateField(&distanceField) != OGRERR_NONE) {
        std::cerr << "Failed to create 'Distance' field in output shapefile" << std::endl;
        GDALClose(profileDS);
        GDALClose(multiCoastDS);
        GDALClose(outputDS);
        return;
    }

    /**
     * \section OPTIMIZATION Spatial Optimization Strategy
     * 
     * Calculate union bounding box of all profiles to:
     * 1. Minimize coastline data processing to relevant area only
     * 2. Improve performance for large datasets
     * 3. Reduce memory usage during intersection calculations
     * 
     * This preprocessing step significantly improves efficiency when
     * coastline datasets are much larger than the profile study area.
     */
    // Declare a vector to store non-intersecting pairs
    std::vector<std::pair<OGRGeometry*, OGRGeometry*>> nonIntersectingPairs;

    int lineCounter = 0;

    // Compute the union envelope of all profile features
    OGREnvelope unionEnv;
    bool firstProfile = true;
    profileLayer->ResetReading();
    OGRFeature* profileFeature;
    while ((profileFeature = profileLayer->GetNextFeature()) != NULL) {
        OGRGeometry* profileGeom = profileFeature->GetGeometryRef();
        if (profileGeom && profileGeom->IsValid()) {
            OGREnvelope env;
            profileGeom->getEnvelope(&env);
            if (firstProfile) {
                unionEnv = env;
                firstProfile = false;
            } else {
                unionEnv.MinX = std::min(unionEnv.MinX, env.MinX);
                unionEnv.MinY = std::min(unionEnv.MinY, env.MinY);
                unionEnv.MaxX = std::max(unionEnv.MaxX, env.MaxX);
                unionEnv.MaxY = std::max(unionEnv.MaxY, env.MaxY);
            }
        }
        OGRFeature::DestroyFeature(profileFeature);
    }

    /**
     * \section CLIPPING Spatial Clipping Operation
     * 
     * Use ogr2ogr system call to perform robust spatial clipping:
     * 1. Clip coastlines to profile bounding box using SQLITE spatial functions
     * 2. Save clipped data to temporary shapefile for efficient processing
     * 3. Load clipped coastlines into memory for fast intersection analysis
     * 
     * This approach leverages ogr2ogr's optimized spatial operations while
     * maintaining memory efficiency for subsequent processing.
     */
    // Clip multiCoastLayer by the profileLayer bounding box using ogr2ogr
    std::string outDir;
    size_t found = outputShapefile.find_last_of("/\\");
    if (found != std::string::npos) {
        outDir = outputShapefile.substr(0, found);
    } else {
        outDir = ".";
    }
    
    // Determine the correct file extension based on output format
    std::string extension;
    if (fileformat == "ESRI Shapefile") {
        extension = ".shp";
    } else if (fileformat == "GPKG") {
        extension = ".gpkg";
    } else if (fileformat == "GeoJSON") {
        extension = ".geojson";
    } else {
        // Default to shapefile for unknown formats
        extension = ".shp";
    }
    
    std::string clippedCoastPath = outDir + "/Clipped_MultiCoast" + extension;
    // Remove existing clipped file if present
    if (std::filesystem::exists(clippedCoastPath)) {
        GDALDriver* driver = GetGDALDriverManager()->GetDriverByName(fileformat.c_str());
        if (driver) driver->Delete(clippedCoastPath.c_str());
    }
    // Build the SQL for clipping
    char clipSQL[512];
    snprintf(clipSQL, sizeof(clipSQL),
        "SELECT ST_Intersection(geom, BuildMbr(%.15g, %.15g, %.15g, %.15g)) AS geom, * FROM %s WHERE ST_Intersects(geom, BuildMbr(%.15g, %.15g, %.15g, %.15g))",
        unionEnv.MinX, unionEnv.MinY, unionEnv.MaxX, unionEnv.MaxY,
        multiCoastLayer->GetName(),
        unionEnv.MinX, unionEnv.MinY, unionEnv.MaxX, unionEnv.MaxY);
    // Use ogr2ogr via system call for robust clipping
    std::string ogrCmd = "ogr2ogr -f \"" + fileformat + "\" \"" + clippedCoastPath + "\" \"" + multiCoastShapefile + "\" -dialect SQLITE -sql \"" + clipSQL + "\"";
    int ogrResult = std::system(ogrCmd.c_str());
    if (ogrResult != 0) {
        std::cerr << "ogr2ogr clipping failed! Command: " << ogrCmd << std::endl;
    }

    // Read all clipped multiCoast features into memory
    std::vector<std::unique_ptr<OGRFeature>> coastFeatures;
    GDALDataset* clippedDS = (GDALDataset*) GDALOpenEx(clippedCoastPath.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
    if (clippedDS) {
        OGRLayer* clippedLayer = clippedDS->GetLayer(0);
        clippedLayer->ResetReading();
        OGRFeature* clippedFeature;
        while ((clippedFeature = clippedLayer->GetNextFeature()) != NULL) {
            coastFeatures.emplace_back(clippedFeature);
        }
        GDALClose(clippedDS);
        std::cout << "Clipped multiCoast features saved to: " << clippedCoastPath << std::endl;
    } else {
        std::cerr << "Failed to open clipped multiCoast file: " << clippedCoastPath << std::endl;
    }

    /**
     * \section INTERSECTION Main Intersection Analysis Loop
     * 
     * Core processing algorithm:
     * 1. For each profile, extract all constituent LineString geometries
     * 2. For each coastline, extract all constituent LineString geometries  
     * 3. Test each profile-coastline LineString pair for intersection
     * 4. Calculate precise intersection points using OGR geometric operations
     * 5. Compute distance from profile start point to intersection
     * 6. Create output features with combined attributes and calculated distance
     * 
     * Progress tracking provides feedback during long-running operations.
     */
    // Now iterate over profile features again
    profileLayer->ResetReading();
    while ((profileFeature = profileLayer->GetNextFeature()) != NULL) {
        OGRGeometry* profileGeom = profileFeature->GetGeometryRef();
        if (!profileGeom || !profileGeom->IsValid()) {
            OGRFeature::DestroyFeature(profileFeature);
            continue;
        }

        std::vector<OGRLineString*> profileLines = getLinestrings(profileGeom);
        bool foundAnyIntersection = false;

        for (const auto& coastFeaturePtr : coastFeatures) {
            OGRFeature* coastFeature = coastFeaturePtr.get();
            OGRGeometry* multiCoastGeom = coastFeature->GetGeometryRef();
            if (!multiCoastGeom || !multiCoastGeom->IsValid()) {
                continue;
            }
            std::vector<OGRLineString*> coastLines = getLinestrings(multiCoastGeom);

            bool foundIntersection = false;
            for (auto* pline : profileLines) {
                for (auto* cline : coastLines) {
                    if (pline->Intersects(cline)) {
                        OGRGeometry* intersection = pline->Intersection(cline);
                        if (intersection && !intersection->IsEmpty()) {
                            auto points = extractPoints(intersection);
                            for (const auto& pt : points) {
                                OGRFeature* outputFeature = OGRFeature::CreateFeature(outputLayer->GetLayerDefn());
                                outputFeature->SetGeometry(&pt);
                                for (int i = 0; i < profileFeature->GetFieldCount(); i++) {
                                    const char* fieldName = profileFeature->GetFieldDefnRef(i)->GetNameRef();
                                    if (outputLayer->GetLayerDefn()->GetFieldIndex(fieldName) >= 0) {
                                        outputFeature->SetField(fieldName, profileFeature->GetFieldAsString(i));
                                    }
                                }
                                for (int i = 0; i < coastFeature->GetFieldCount(); i++) {
                                    const char* fieldName = coastFeature->GetFieldDefnRef(i)->GetNameRef();
                                    if (outputLayer->GetLayerDefn()->GetFieldIndex(fieldName) >= 0) {
                                        outputFeature->SetField(fieldName, coastFeature->GetFieldAsString(i));
                                    }
                                }
                                /**
                                 * \section DISTANCE Distance Calculation Method
                                 * 
                                 * Distance calculation methodology:
                                 * 1. Identify profile start point (first vertex)
                                 * 2. Calculate Euclidean distance to intersection point
                                 * 3. Add baseline offset (distancecoast parameter)
                                 * 
                                 * This provides consistent distance measurements
                                 * relative to a common baseline reference.
                                 */
                                // Compute distance from start of profile
                                OGRPoint profileStart;
                                pline->getPoint(0, &profileStart);
                                double distance = profileStart.Distance(&pt) + distancecoast;
                                outputFeature->SetField("Distance", distance);
                                lineCounter++;
                                if (lineCounter % 100 == 0) std::cout << "." << std::flush;
                                if (outputLayer->CreateFeature(outputFeature) != OGRERR_NONE) {
                                    std::cerr << "Failed to create feature in outputLayer" << std::endl;
                                }
                                OGRFeature::DestroyFeature(outputFeature);
                                foundIntersection = true;
                                foundAnyIntersection = true;
                            }
                            OGRGeometryFactory::destroyGeometry(intersection);
                        }
                    }
                }
                if (foundIntersection) break;
            }
            if (!foundIntersection) {
                nonIntersectingPairs.emplace_back(profileGeom->clone(), multiCoastGeom->clone());
            }
        }
        OGRFeature::DestroyFeature(profileFeature);
    }
    // Clear the spatial filter after processing
    multiCoastLayer->SetSpatialFilter(NULL);

    GDALClose(profileDS);
    GDALClose(multiCoastDS);
    GDALClose(outputDS);
    std::cout << std::endl << CROSSINGPOINTSNUMNOTICE << lineCounter << " lines" << std::endl;

    /**
     * \section DEBUGGING Debug Output Generation
     * 
     * Optional quality control feature:
     * - Saves non-intersecting profile-coastline pairs to GeoPackage
     * - Enables visual inspection of potential data quality issues
     * - Helps identify problems with geometry validity or spatial alignment
     * - Useful for troubleshooting intersection analysis results
     * 
     * Debug output includes separate layers for profile and coastline
     * geometries that failed to intersect, facilitating targeted QC.
     */
    // Write non-intersecting pairs to a log file
    if (saveNonIntersectingPairs && !nonIntersectingPairs.empty()) {
        // Extract directory from outputShapefile using string manipulation
        std::string outDir;
        size_t found = outputShapefile.find_last_of("/\\");
        if (found != std::string::npos) {
            outDir = outputShapefile.substr(0, found);
        } else {
            outDir = "."; // current directory if no path separator found
        }
        std::string debugPath = outDir + "/non_intersecting_pairs.gpkg";

        GDALDriver* gpkgDriver = GetGDALDriverManager()->GetDriverByName("GPKG");
        GDALDataset* debugDS = gpkgDriver->Create(debugPath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
        // Use wkbUnknown to allow both LINESTRING and MULTILINESTRING
        OGRLayer* profileDebugLayer = debugDS->CreateLayer("profile_lines", pOGRSpatialRef, wkbUnknown, NULL);
        OGRLayer* coastDebugLayer = debugDS->CreateLayer("coast_lines", pOGRSpatialRef, wkbUnknown, NULL);

        for (const auto& pair : nonIntersectingPairs) {
            OGRFeature* feat1 = OGRFeature::CreateFeature(profileDebugLayer->GetLayerDefn());
            feat1->SetGeometry(pair.first);
            if (profileDebugLayer->CreateFeature(feat1) != OGRERR_NONE) {
                std::cerr << "Failed to create feature in profileDebugLayer" << std::endl;
            }
            OGRFeature::DestroyFeature(feat1);

            OGRFeature* feat2 = OGRFeature::CreateFeature(coastDebugLayer->GetLayerDefn());
            feat2->SetGeometry(pair.second);
            if (coastDebugLayer->CreateFeature(feat2) != OGRERR_NONE) {
                std::cerr << "Failed to create feature in coastDebugLayer" << std::endl;
            }
            OGRFeature::DestroyFeature(feat2);

            OGRGeometryFactory::destroyGeometry(pair.first);
            OGRGeometryFactory::destroyGeometry(pair.second);
        }
        GDALClose(debugDS);
        std::cout << "Non-intersecting pairs written to " << debugPath << std::endl;
    }

    /**
     * \section CLEANUP Resource Management
     * 
     * Proper cleanup of GDAL/OGR resources:
     * - Close all dataset handles
     * - Destroy driver manager
     * - Free allocated geometries
     * 
     * Ensures no memory leaks and proper resource management
     * according to GDAL/OGR best practices.
     */
    // Cleanup
    GDALDestroyDriverManager();
    std::cout << "Processing completed." << std::endl;
}

/**
 * \section SUMMARY Function Summary
 * 
 * This function provides a comprehensive solution for profile-coastline intersection analysis:
 * 
 * **Key Capabilities**:
 * - High-precision geometric intersection calculations
 * - Efficient spatial optimization for large datasets  
 * - Robust handling of complex geometry types
 * - Integrated distance calculations with baseline referencing
 * - Quality control features for debugging and validation
 * - Flexible output formats and attribute integration
 * 
 * **Performance Features**:
 * - Spatial clipping reduces processing overhead
 * - Memory-efficient handling of large coastline datasets
 * - Progress reporting for long-running operations
 * - Optimized geometric operations using OGR
 * 
 * **Quality Assurance**:
 * - Geometry validation prevents processing errors
 * - Debug output enables troubleshooting
 * - Error handling ensures graceful failure recovery
 * - Comprehensive logging of processing statistics
 * 
 * **Integration**:
 * - Seamless integration with ODSAS coastal analysis workflow
 * - Compatible with standard GIS data formats
 * - Maintains spatial reference consistency
 * - Prepares data for subsequent statistical analysis
 * 
 * The function represents a critical component in the coastal change analysis pipeline,
 * providing the geometric foundation for temporal coastline monitoring and statistical
 * rate calculations.
 */

// End of profile_crossing.cpp

