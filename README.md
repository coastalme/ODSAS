# ODSAS++ - Open Dogital Shoreline Analysis System

## Overview

ODSAS++ is a C++ version of the ODSAS tool [1].

[1] J. Mar. Sci. Eng. 2022, 10(1), 26; https://doi.org/10.3390/jmse10010026

## Authors
- Andres Payo
- Cristina Torrecillas Lozano

## License
GNU General Public License v3.0

## Architecture Overview

### Main Program Flow

The application follows a structured workflow implemented primarily in the `CDelineation` class:

```
main() → CDelineation::nDoDelineation() → Processing Pipeline → Results Output
```

### Configuration and Input Files

#### Required Files:
1. **Initialization File** (`.ini`): General configuration parameters
2. **Run Data File** (`.dat`): Run-specific parameters and input file paths
3. **Baselines**: Vector GPKG file with the points along the line where transects will be drawn
3. **Historical shorelines positions**: Vector GPKG file with the historical shoreline positions
4. **Hostorical shorelines time stamp and uncertainty**: CSV file with ID,Day (YYYY-MM-DD),Hour (HH:MM:SS),Uncertainty (RMS in meters)


### Output Products

#### Vector Outputs:
- **Normals**: GPKG Shore-normal transect lines with the metrics of change per normal
- **Intersection Points**: GPKG point file with crossing analysis results

#### CSV Outputs:
- **transects_stats_##.txt**: ID_Profile,NSM,EPR,EPRunc,SCE,LRR,LR2,WLR,WR2
- **odsas input summary**: dat ASCII file with the ODSAS set up config details

### Processing Parameters

#### Key Configuration Options:
- **Profile Spacing**: Distance between shore-normal profiles
- **Profile Length**: Seaward and landward extent of profiles
- **Smoothing Method**: Coastline smoothing algorithm (running mean, Savitzky-Golay)

### Error Handling

The system uses a comprehensive error code system with specific return codes for different failure modes:
- `RTN_OK`: Successful completion
- `RTN_ERR_*`: Specific error conditions for file I/O, data processing, memory allocation, etc.

### Cross-Platform Compatibility

ODSAS is designed to run on both Windows and Linux/Unix systems with platform-specific code handling:
- File path separators
- System-specific API calls
- Compiler-specific optimizations
- Thread-safe functions

### Dependencies

#### Required Libraries:
- **GDAL/OGR**: Geospatial data processing
- **Standard C++ Libraries**: STL containers, file I/O, mathematical functions

#### Optional Libraries:
- Platform-specific libraries for enhanced functionality

## Building and Installation

The project uses CMake for cross-platform building:

```bash
# Configure build
cmake -S src -B build

# Compile
cmake --build build --config Release
```

## Usage

```bash
# Basic usage
./odsas [configuration_file.ini]

# The program reads configuration from .ini files and processes
# the specified DTM data according to the parameters
```
