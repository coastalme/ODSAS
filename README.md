# ODSAS++ - Open Digital Shoreline Analysis System

![License](https://img.shields.io/badge/license-GPL%20v3.0-blue.svg)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg)
![Language](https://img.shields.io/badge/language-C%2B%2B-orange.svg)

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Requirements](#requirements)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Configuration](#configuration)
  - [Input Files](#input-files)
  - [Configuration Parameters](#configuration-parameters)
- [Output Products](#output-products)
- [Architecture](#architecture)
  - [Program Flow](#program-flow)
  - [Error Handling](#error-handling)
  - [Cross-Platform Compatibility](#cross-platform-compatibility)
- [Dependencies](#dependencies)
- [Usage Examples](#usage-examples)
- [Contributing](#contributing)
- [License](#license)
- [Authors](#authors)
- [References](#references)

## Overview

**ODSAS++** is a high-performance C++ implementation of the Open Digital Shoreline Analysis System (ODSAS), designed for comprehensive coastal change analysis and shoreline evolution studies. This advanced geospatial tool provides researchers and coastal engineers with robust capabilities for analyzing historical shoreline positions and quantifying coastal change rates.

Built upon the foundation of the original ODSAS methodology, ODSAS++ offers enhanced performance, cross-platform compatibility, and extended analytical capabilities for processing large-scale coastal datasets.

## Features

- 📊 **Comprehensive Shoreline Analysis**: Calculate multiple shoreline change statistics (NSM, EPR, LRR, etc.)
- 🚀 **High Performance**: Optimized C++ implementation for large datasets
- 🌐 **Cross-Platform**: Compatible with Windows and Linux/Unix systems
- 📁 **Multiple Data Formats**: Support for GPKG vector files and CSV data
- 🔧 **Flexible Configuration**: Customizable processing parameters via INI files
- 📈 **Statistical Analysis**: Advanced coastal change rate calculations with uncertainty quantification
- 🗺️ **Geospatial Integration**: Built on GDAL/OGR for robust spatial data handling

## Requirements

### System Requirements
- **Operating System**: Windows 10+ or Linux (Ubuntu 18.04+, CentOS 7+)
- **Memory**: Minimum 4GB RAM (8GB+ recommended for large datasets)
- **Storage**: 1GB free disk space

### Software Dependencies
- **CMake**: Version 3.12 or higher
- **C++ Compiler**: GCC 7+ or MSVC 2019+
- **GDAL/OGR**: Version 3.0 or higher

## Installation

### Using CMake (Recommended)

1. **Clone or download** the ODSAS++ source code
2. **Navigate** to the project directory
3. **Configure** the build environment:
   ```bash
   cmake -S src -B build -DCMAKE_BUILD_TYPE=Release
   ```
4. **Compile** the application:
   ```bash
   cmake --build build --config Release
   ```

### Platform-Specific Notes

#### Windows
- Ensure GDAL is installed and properly configured in your system PATH
- Use Visual Studio 2019 or later for optimal compatibility

#### Linux
- Install GDAL development packages:
  ```bash
  sudo apt-get install libgdal-dev gdal-bin  # Ubuntu/Debian
  sudo yum install gdal-devel                # CentOS/RHEL
  ```

## Quick Start

1. **Prepare your data**:
   - Baseline vector file (GPKG format)
   - Historical shoreline positions (GPKG format)
   - Shoreline timestamps and uncertainty data (CSV format)

2. **Configure the analysis**:
   - Create or modify the initialization file (`.ini`)
   - Set up the run data file (`.dat`)

3. **Run the analysis**:
   ```bash
   ./odsas config/your_config.ini
   ```

## Configuration

### Input Files

ODSAS++ requires three primary input files:

#### 1. Initialization File (`.ini`)
Contains general configuration parameters for the analysis session.

#### 2. Run Data File (`.dat`)
Specifies run-specific parameters and input file paths.

#### 3. Data Files
- **Baselines**: Vector GPKG file containing points along the baseline where transects will be generated
- **Historical Shoreline Positions**: Vector GPKG file with historical shoreline geometries
- **Historical Shoreline Metadata**: CSV file with columns:
  - `ID`: Shoreline identifier
  - `Day`: Date in YYYY-MM-DD format
  - `Hour`: Time in HH:MM:SS format
  - `Uncertainty`: Root Mean Square uncertainty in meters

### Configuration Parameters

#### Key Settings
- **Profile Spacing**: Distance between shore-normal profile lines
- **Profile Length**: Seaward and landward extent of analysis profiles
- **Smoothing Method**: Coastline smoothing algorithm options:
  - Running mean
  - Savitzky-Golay filter

## Output Products

### Vector Outputs
- **Shore-Normal Transects** (`.gpkg`): Transect lines with calculated change metrics
- **Intersection Points** (`.gpkg`): Point geometries with detailed crossing analysis results

### Statistical Outputs
- **Transect Statistics** (`transects_stats_##.txt`): Comprehensive change statistics including:
  - `ID_Profile`: Profile identifier
  - `NSM`: Net Shoreline Movement
  - `EPR`: End Point Rate
  - `EPRunc`: End Point Rate uncertainty
  - `SCE`: Shoreline Change Envelope
  - `LRR`: Linear Regression Rate
  - `LR2`: Linear Regression R-squared
  - `WLR`: Weighted Linear Regression Rate
  - `WR2`: Weighted Linear Regression R-squared

### Configuration Summary
- **ODSAS Input Summary** (`.dat`): ASCII file containing complete analysis configuration details

## Architecture

### Program Flow

The application implements a structured analytical pipeline:

```mermaid
graph TD
    A[main()] --> B[CDelineation::nDoDelineation()]
    B --> C[Configuration Loading]
    C --> D[Data Input Processing]
    D --> E[Transect Generation]
    E --> F[Intersection Analysis]
    F --> G[Statistical Calculations]
    G --> H[Results Output]
```

### Error Handling

ODSAS++ implements a comprehensive error management system:

- **`RTN_OK`**: Successful operation completion
- **`RTN_ERR_*`**: Specific error codes for:
  - File I/O operations
  - Data processing errors
  - Memory allocation failures
  - Geospatial data validation issues

### Cross-Platform Compatibility

The system handles platform-specific requirements through:

- Dynamic file path separator detection
- Platform-optimized API calls
- Compiler-specific optimizations
- Thread-safe function implementations

## Dependencies

### Required Libraries
- **GDAL/OGR**: Geospatial data abstraction library for vector and raster processing
- **Standard C++ Libraries**: STL containers, file I/O streams, mathematical functions

### Optional Libraries
- Platform-specific libraries for enhanced functionality and performance optimization

## Usage Examples

### Basic Analysis
```bash
# Run analysis with default configuration
./odsas config/default.ini
```

### Advanced Configuration
```bash
# Run with custom parameters
./odsas config/high_resolution_analysis.ini
```

### Batch Processing
```bash
# Process multiple datasets
for config in config/*.ini; do
    ./odsas "$config"
done
```

## Contributing

We welcome contributions to ODSAS++! Please consider:

1. **Issues**: Report bugs or request features via the issue tracker
2. **Pull Requests**: Submit improvements following our coding standards
3. **Documentation**: Help improve user guides and technical documentation

## License

This project is licensed under the **GNU General Public License v3.0**. See the [LICENSE](LICENSE) file for complete terms and conditions.

## Authors

- **Andrés Payo** - Project Lead & Principal Developer
- **Cristina Torrecillas Lozano** - Core Developer

## References

[1] Payo, A.; Torrecillas, C.; et al. "ODSAS: Open Digital Shoreline Analysis System." *Journal of Marine Science and Engineering* 2022, 10(1), 26. [https://doi.org/10.3390/jmse10010026](https://doi.org/10.3390/jmse10010026)
