/*!
 *
 * \file cliffmetrics.cpp
 * \brief The start-up routine for ODSAS
 * \details TODO A more detailed description of this routine
 * \author Andres Payo, David Favis-Mortlock, Martin Hurst, Monica Palaseanu-Lovejoy
 * \date 2017
 * \copyright GNU General Public License
 *
 */

/*===============================================================================================================================

 This file is part of ODSAS, the Coastal Modelling Environment.

 ODSAS is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 3 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

===============================================================================================================================*/
// TODO Get the rest of ODSAS working :-)

#include "odsas.h"
#include "delineation.h"
#include <iostream>
#include <cstdlib>  // For setenv/putenv

#ifdef _WIN32
#include <stdlib.h>  // For _putenv_s on Windows
#endif

#ifdef GDAL_DATA
#include "cpl_conv.h"  // For CPLSetConfigOption
#endif

using std::cout;
using std::cerr;
using std::endl;
using std::ios;

/*===============================================================================================================================

 ODSAS's main function

===============================================================================================================================*/
int main (int argc, char* argv[])
{
   // Configure GDAL_DATA environment variable if defined at compile time
#ifdef GDAL_DATA
   // Set GDAL_DATA for this process to find GDAL data files
   #ifdef _WIN32
      // Windows: use _putenv_s
      _putenv_s("GDAL_DATA", GDAL_DATA);
   #else
      // Unix/Linux: use setenv
      setenv("GDAL_DATA", GDAL_DATA, 1);
   #endif
   
   // Also set it via GDAL's configuration system for extra safety
   CPLSetConfigOption("GDAL_DATA", GDAL_DATA);
   
   cout << "GDAL_DATA set to: " << GDAL_DATA << endl;
#endif

   // TODO This is supposed to enable the use of UTF-8 symbols in ODSAS output, such as the \u0394 character. But does it work? If not, remove it?
   setlocale(LC_ALL, "en_GB.UTF-8");
   // OK, create a CDelineation objeect (duh!)
   CDelineation* pDelineation	 = new CDelineation;

   // Run the simulation and then check how it ends
   int nRtn = pDelineation->nDoDelineation(argc, argv);
   pDelineation->DoDelineationEnd(nRtn);
  
   // Then go back to the OS
   return (nRtn);
}
