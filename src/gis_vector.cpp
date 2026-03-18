/*!
 *
 * \file gis_vector.cpp
 * \brief These functions use GDAL to read and write vector GIS files in several formats. This version will build with GDAL version 2
 * \details TODO A more detailed description of these routines.
 * \author Andres Payo 
 * \author David Favis-Mortlock
 * \author Martin Husrt
 * \author Monica Palaseanu-Lovejoy
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
#include <iostream>
using std::cout;
using std::cerr;
using std::endl;
using std::ios;

#include <gdal_priv.h>

#include "ogrsf_frmts.h" // Ensure this include is present for GDAL/OGR functions
#include "ogr_spatialref.h"
#include "odsas.h"
#include "delineation.h"
#include "coast.h"


/*==============================================================================================================================

 Writes vector GIS files using OGR

===============================================================================================================================*/
bool CDelineation::bWriteVectorGIS(int const nDataItem, string const* strPlotTitle)
{
   // Begin constructing the file name for this save
   string strFilePathName(m_strOutPath);
   std::string strLayerName;
  
   switch (nDataItem)
   {
      case (PLOT_COAST):
      {
         strFilePathName.append(VECTOR_COAST_NAME);
         strLayerName.append(VECTOR_COAST_NAME);
         break;
      }

      case (PLOT_NORMALS):
      {
         strFilePathName.append(VECTOR_NORMALS_NAME);
         strLayerName.append(VECTOR_NORMALS_NAME);
         break;
      }

      case (PLOT_INVALID_NORMALS):
      {
         strFilePathName.append(VECTOR_INVALID_NORMALS_NAME);
         strLayerName.append(VECTOR_INVALID_NORMALS_NAME);
         break;
      }

      case (PLOT_COAST_CURVATURE):
      {
         strFilePathName.append(VECTOR_COAST_CURVATURE_NAME);
         strLayerName.append(VECTOR_COAST_CURVATURE_NAME);
         break;
      }

      case (PLOT_COAST_POINT):
      {
         strFilePathName.append(VECTOR_COAST_POINT_NAME);
         strLayerName.append(VECTOR_COAST_POINT_NAME);
         break;
      }
   }

   // Append the 'RunName' to the filename
   strFilePathName.append("_");
   strFilePathName.append(m_strRunName);

   // Make a copy of the filename without any extension
   string strFilePathNameNoExt = strFilePathName;

   // If desired, append an extension
   if (! m_strOGRVectorOutputExtension.empty())
      strFilePathName.append(m_strOGRVectorOutputExtension);

   // Set up the vector driver
   GDALDriver* pGDALDriver = GetGDALDriverManager()->GetDriverByName(m_strVectorGISOutFormat.c_str());
   if (pGDALDriver == NULL)
   {
      cerr << ERR << "vector GIS output driver " << m_strVectorGISOutFormat << CPLGetLastErrorMsg() << endl;
      return false;
   }

   // Now create the dataset
   GDALDataset* pGDALDataSet = NULL;
   pGDALDataSet = pGDALDriver->Create(strFilePathName.c_str(), 0, 0, 0, GDT_Unknown, m_papszGDALVectorOptions);
   if (pGDALDataSet == NULL)
   {
      cerr << ERR << "cannot create " << m_strVectorGISOutFormat << " named " << strFilePathName << "\n" << CPLGetLastErrorMsg() << endl;
      return false;
   }

   // Create the output layer
   OGRLayer* pOGRLayer = NULL;
   //OGRSpatialReference* pOGRSpatialRef = NULL;     // TODO add spatial reference 
   OGRwkbGeometryType eGType = wkbUnknown;
   string strType = "unknown";

   //pOGRLayer = pGDALDataSet->CreateLayer(strFilePathNameNoExt.c_str(), pOGRSpatialRef, eGType, m_papszGDALVectorOptions);
   pOGRLayer = pGDALDataSet->CreateLayer(strLayerName.c_str(), m_poSRS.get(), eGType, m_papszGDALVectorOptions);
   if (pOGRLayer == NULL)
   {
      cerr << ERR << "cannot create '" << strType << "' layer in " << strFilePathName << "\n" << CPLGetLastErrorMsg() << endl;
      return false;
   }
   
   pOGRLayer->ResetReading();
   
   switch (nDataItem)
   {
      case (PLOT_COAST):
      {
         eGType = wkbLineString;
         strType = "line";

         // The layer has been created, so create an integer-numbered value (the number of the coast object) for the multi-line
         string strFieldValue1 = "Coast";
         OGRFieldDefn OGRField1(strFieldValue1.c_str(), OFTInteger);
         if (pOGRLayer->CreateField(&OGRField1) != OGRERR_NONE)
         {
            cerr << ERR << "cannot create " << strType << " attribute field 1 '" << strFieldValue1 << "' in " << strFilePathName << "\n" << CPLGetLastErrorMsg() << endl;
            return false;
         }

         // OK, now do features
         OGRLineString OGRls;

         for (int i = 0; i < static_cast<int>(m_VCoast.size()); i++)
         {
            // Create a feature object, one per coast
            OGRFeature *pOGRFeature = NULL;
            pOGRFeature = OGRFeature::CreateFeature(pOGRLayer->GetLayerDefn());

                
            // Set the feature's attribute (the coast number) << This is the line causing ERROR 1: Invalid Index : -XXXX
            pOGRFeature->SetField(strFieldValue1.c_str(), i);
            
            // Now attach a geometry to the feature object
            for (int j = 0; j < m_VCoast[i].pLGetCoastline()->nGetSize(); j++)
               //  In external CRS
               OGRls.addPoint(m_VCoast[i].pPtGetVectorCoastlinePoint(j)->dGetX(), m_VCoast[i].pPtGetVectorCoastlinePoint(j)->dGetY());

            pOGRFeature->SetGeometry(&OGRls);

            // Create the feature in the output layer
            if (pOGRLayer->CreateFeature(pOGRFeature) != OGRERR_NONE)
            {
               cerr << ERR << "cannot create  " << strType << " feature " << strPlotTitle << " for coast " << i << " in " << strFilePathName << "\n" << CPLGetLastErrorMsg() << endl;
               return false;
            }

            // Tidy up: empty the line string and get rid of the feature object
            OGRls.empty();
            OGRFeature::DestroyFeature(pOGRFeature);
         }

         break;
      }

      case (PLOT_COAST_CURVATURE):
      {
         eGType = wkbPoint;
         strType = "point";

         // The layer has been created, so create a real-numbered value associated with each point
         string strFieldValue1 = "Curve"; 
         
         OGRFieldDefn OGRField1(strFieldValue1.c_str(), OFTReal);
         if (pOGRLayer->CreateField(&OGRField1) != OGRERR_NONE)
         {
            cerr << ERR << "cannot create " << strType << " attribute field 1 '" << strFieldValue1 << "' in " << strFilePathName << "\n" << CPLGetLastErrorMsg() << endl;
            return false;
         }

         // OK, now create features
         OGRLineString OGRls;
         OGRMultiLineString OGRmls;
         OGRPoint OGRPt;

         for (int i = 0; i < static_cast<int>(m_VCoast.size()); i++)
         {
            for (int j = 0; j < m_VCoast[i].pLGetCoastline()->nGetSize(); j++)
            {
               // Create a feature object, one per coastline point
               OGRFeature *pOGRFeature = NULL;
               pOGRFeature = OGRFeature::CreateFeature(pOGRLayer->GetLayerDefn());

               // Set the feature's geometry (in external CRS)
               OGRPt.setX(m_VCoast[i].pPtGetVectorCoastlinePoint(j)->dGetX());
               OGRPt.setY(m_VCoast[i].pPtGetVectorCoastlinePoint(j)->dGetY());
               pOGRFeature->SetGeometry(&OGRPt);

               double dCurvature = m_VCoast[i].dGetCurvature(j);
               if (dCurvature == DBL_NODATA)
                     continue;

               // Set the feature's attribute
               pOGRFeature->SetField(strFieldValue1.c_str(), dCurvature);
          
               // Create the feature in the output layer
               if (pOGRLayer->CreateFeature(pOGRFeature) != OGRERR_NONE)
               {
                  cerr << ERR << "cannot create " << strType << " feature " << strPlotTitle << " for coast " << i << " point " << j << " in " << strFilePathName << "\n" << CPLGetLastErrorMsg() << endl;
                  return false;
               }

               // Get rid of the feature object
               OGRFeature::DestroyFeature(pOGRFeature);
            }
         }

         break;
      }
      case (PLOT_NORMALS):
      case (PLOT_INVALID_NORMALS):
      {
         eGType = wkbLineString;
         strType = "line";

         // The layer has been created, so create an integer-numbered value (the number of the normal) associated with the line
         string strFieldValue1 = "nProf";  //CT change Normal to nprof
         OGRFieldDefn OGRField1(strFieldValue1.c_str(), OFTInteger);
         if (pOGRLayer->CreateField(&OGRField1) != OGRERR_NONE)
         {
            cerr << ERR << "cannot create " << strType << " attribute field 1 '" << strFieldValue1 << "' in " << strFilePathName << "\n" << CPLGetLastErrorMsg() << endl;
            return false;
         }

         // Also create other integer-numbered values for the category codes of the coastline-normalprofile
         string
            strFieldValue2 = "StartCoast",
            strFieldValue3 = "EndCoast",
            strFieldValue4 = "HitLand",
            strFieldValue5 = "HitCoast",
            strFieldValue6 = "HitNormal",
	        strFieldValue7 = "nCoast";
            
         OGRFieldDefn 
            OGRField2(strFieldValue2.c_str(), OFTInteger),
            OGRField3(strFieldValue3.c_str(), OFTInteger),
            OGRField4(strFieldValue4.c_str(), OFTInteger),
            OGRField5(strFieldValue5.c_str(), OFTInteger),
            OGRField6(strFieldValue6.c_str(), OFTInteger),
            OGRField7(strFieldValue7.c_str(), OFTInteger);
         if (pOGRLayer->CreateField(&OGRField2) != OGRERR_NONE)
         {
            cerr << ERR << "cannot create " << strType << " attribute field 2 '" << strFieldValue2 << "' in " << strFilePathName << "\n" << CPLGetLastErrorMsg() << endl;
            return false;
         }
         if (pOGRLayer->CreateField(&OGRField3) != OGRERR_NONE)
         {
            cerr << ERR << "cannot create " << strType << " attribute field 3 '" << strFieldValue3 << "' in " << strFilePathName << "\n" << CPLGetLastErrorMsg() << endl;
            return false;
         }
         if (pOGRLayer->CreateField(&OGRField4) != OGRERR_NONE)
         {
            cerr << ERR << "cannot create " << strType << " attribute field 4 '" << strFieldValue4 << "' in " << strFilePathName << "\n" << CPLGetLastErrorMsg() << endl;
            return false;
         }
         if (pOGRLayer->CreateField(&OGRField5) != OGRERR_NONE)
         {
            cerr << ERR << "cannot create " << strType << " attribute field 5 '" << strFieldValue5 << "' in " << strFilePathName << "\n" << CPLGetLastErrorMsg() << endl;
            return false;
         }
         if (pOGRLayer->CreateField(&OGRField6) != OGRERR_NONE)
         {
            cerr << ERR << "cannot create " << strType << " attribute field 6 '" << strFieldValue6 << "' in " << strFilePathName << "\n" << CPLGetLastErrorMsg() << endl;
            return false;
         }
         if (pOGRLayer->CreateField(&OGRField7) != OGRERR_NONE)
         {
            cerr << ERR << "cannot create " << strType << " attribute field 7 '" << strFieldValue7 << "' in " << strFilePathName << "\n" << CPLGetLastErrorMsg() << endl;
            return false;
         }
         
         // OK, now create features
         OGRLineString OGRls;
         for (int i = 0; i < static_cast<int>(m_VCoast.size()); i++) // for each coast object
         {
            for (int j = 0; j < m_VCoast[i].nGetNumProfiles(); j++)
            {
               CProfile* pProfile = m_VCoast[i].pGetProfile(j);

               if (((nDataItem == PLOT_NORMALS) && (pProfile->bOKIncStartAndEndOfCoast())) || ((nDataItem == PLOT_INVALID_NORMALS) && (! pProfile->bOKIncStartAndEndOfCoast())))
               {
                  // Create a feature object, one per profile
                  OGRFeature *pOGRFeature = NULL;
                  pOGRFeature = OGRFeature::CreateFeature(pOGRLayer->GetLayerDefn());

                // Set the feature's attributes
                  pOGRFeature->SetField(strFieldValue1.c_str(), j);
                  pOGRFeature->SetField(strFieldValue2.c_str(), 0);
                  pOGRFeature->SetField(strFieldValue3.c_str(), 0);
                  pOGRFeature->SetField(strFieldValue4.c_str(), 0);
                  pOGRFeature->SetField(strFieldValue5.c_str(), 0);
                  pOGRFeature->SetField(strFieldValue6.c_str(), 0);
                  pOGRFeature->SetField(strFieldValue7.c_str(), i);
                  if (pProfile->bStartOfCoast())
                     pOGRFeature->SetField(strFieldValue2.c_str(), 1);
                  if (pProfile->bEndOfCoast())
                     pOGRFeature->SetField(strFieldValue3.c_str(), 1);
                  if (pProfile->bHitLand())
                     pOGRFeature->SetField(strFieldValue4.c_str(), 1);
                  if (pProfile->bHitCoast())
                     pOGRFeature->SetField(strFieldValue5.c_str(), 1);
                  if (pProfile->bHitAnotherProfile())
                     pOGRFeature->SetField(strFieldValue6.c_str(), 1);

                  // Now attach a geometry to the feature object
                  for (int k = 0; k < pProfile->nGetProfileSize(); k++)
                     OGRls.addPoint(pProfile->pPtGetPointInProfile(k)->dGetX(), pProfile->pPtGetPointInProfile(k)->dGetY());

                  pOGRFeature->SetGeometry(&OGRls);
                  OGRls.empty();

                  // Create the feature in the output layer
                  if (pOGRLayer->CreateFeature(pOGRFeature) != OGRERR_NONE)
                  {
                     cerr << ERR << "cannot create  " << strType << " feature " << strPlotTitle << " for coast " << i << " and profile " << j << " in " << strFilePathName << "\n" << CPLGetLastErrorMsg() << endl;
                     return false;
                  }

                  // Tidy up: get rid of the feature object
                  OGRFeature::DestroyFeature(pOGRFeature);
               }
            }
         }

         break;
      }
   //  case (PLOT_CLIFF_TOP):
   //  case (PLOT_CLIFF_TOE):
      case (PLOT_COAST_POINT):
      {
         eGType = wkbPoint;
         strType = "point";

         // The layer has been created, so create an integer-numbered value (the number of the coast) associated with the line and the profile
         string strFieldValue1 = "nCoast";
         string strFieldValue2 = "nProf";
         string strFieldValue3 = "bisOK";
         string strFieldValue6 = "nPoint";
     
	      OGRFieldDefn OGRField1(strFieldValue1.c_str(), OFTInteger);
         OGRFieldDefn OGRField2(strFieldValue2.c_str(), OFTInteger);
	      OGRFieldDefn OGRField3(strFieldValue3.c_str(), OFTInteger);
         OGRFieldDefn OGRField6(strFieldValue6.c_str(), OFTInteger);
         
         if (pOGRLayer->CreateField(&OGRField1) != OGRERR_NONE)
         {
            cerr << ERR << "cannot create " << strType << " attribute field 1 '" << strFieldValue1 << "' in " << strFilePathName << "\n" << CPLGetLastErrorMsg() << endl;
            return false;
         }
         if (pOGRLayer->CreateField(&OGRField2) != OGRERR_NONE)
         {
            cerr << ERR << "cannot create " << strType << " attribute field 2 '" << strFieldValue2 << "' in " << strFilePathName << "\n" << CPLGetLastErrorMsg() << endl;
            return false;
         }
         if (pOGRLayer->CreateField(&OGRField3) != OGRERR_NONE)
         {
            cerr << ERR << "cannot create " << strType << " attribute field 3 '" << strFieldValue3 << "' in " << strFilePathName << "\n" << CPLGetLastErrorMsg() << endl;
            return false;
         }
         if (pOGRLayer->CreateField(&OGRField6) != OGRERR_NONE)
         {
            cerr << ERR << "cannot create " << strType << " attribute field 6 '" << strFieldValue6 << "' in " << strFilePathName << "\n" << CPLGetLastErrorMsg() << endl;
            return false;
         }

         
         // OK, now create features
         OGRLineString OGRls;
         OGRMultiLineString OGRmls;
         OGRPoint OGRPt;

         for (int i = 0; i < static_cast<int>(m_VCoast.size()); i++) // for each coast object
         {
            for (int j = 0; j < m_VCoast[i].nGetNumProfiles(); j++) // for each profile object
            {
               CProfile* pProfile = m_VCoast[i].pGetProfile(j);
               //bool profileExists = false;                                                    //CT
               
               if (pProfile->bOKIncStartAndEndOfCoast()) 
               {
                  // Create a feature object, one per profile
                  OGRFeature *pOGRFeature = NULL;
                  pOGRFeature = OGRFeature::CreateFeature(pOGRLayer->GetLayerDefn());

                  // Set the feature's geometry (in external CRS)
                  int 
                  //  CliffPointIndex,
                    nX,  
                    nY,  
		              isQualityOK,
                    nCoastPoint;
                  double
                    dX,
                    dY;
		           // dChainage;

                  (void)nX;  // Dummy use to avoid warning
                  (void)nY;  // Dummy use to avoid warning

		            if (nDataItem == PLOT_COAST_POINT)  
                  {
                 // dChainage	  = 0;
                     // Get the Feature ID for this point
                     nCoastPoint = pProfile->nGetNumCoastPoint();

                     if (m_nTypeProfile == 0 || m_nTypeProfile == 1)    //CT
                     {
                        nX = pProfile->pPtiVGetCellsInProfile()->at(0).nGetX();
                        nY = pProfile->pPtiVGetCellsInProfile()->at(0).nGetY();
                        dX = dGridCentroidXToExtCRSX(pProfile->pPtiVGetCellsInProfile()->at(0).nGetX());
                        dY = dGridCentroidYToExtCRSY(pProfile->pPtiVGetCellsInProfile()->at(0).nGetY());
                        isQualityOK = 1; // to do, sanity check for coastline points
                     }
                     else if (m_nTypeProfile == 2)
                     {
                     C2DPoint PtStart;  // PtStart has coordinates in external CRS
                     PtStart.SetX(dGridCentroidXToExtCRSX(m_VCoast[i].pPtiGetCellMarkedAsCoastline(nCoastPoint)->nGetX()));  // there is points without profile
                     PtStart.SetY(dGridCentroidYToExtCRSY(m_VCoast[i].pPtiGetCellMarkedAsCoastline(nCoastPoint)->nGetY()));    //
                     
                     dX = PtStart.dGetX(); // Retrieve the x coordinate after setting it
                     dY = PtStart.dGetY(); // Retrieve the y coordinate after setting it
                     isQualityOK = 1; // to do, sanity check for coastline points
                     }

                     OGRPt.setX(dX);
                     OGRPt.setY(dY);
                     pOGRFeature->SetGeometry(&OGRPt);

                  // Get the elevation for both consolidated and unconsolidated sediment on this cell
                  // double dVProfileZ = m_pRasterGrid->pGetCell(nX, nY)->dGetSedimentTopElev();
                     


                     // Set the feature's attributes
                     pOGRFeature->SetField(strFieldValue1.c_str(), i);
                     pOGRFeature->SetField(strFieldValue2.c_str(), j);
                     pOGRFeature->SetField(strFieldValue3.c_str(), isQualityOK);
                  // pOGRFeature->SetField(strFieldValue4.c_str(), dVProfileZ);
                  // pOGRFeature->SetField(strFieldValue5.c_str(), dChainage);
                     pOGRFeature->SetField(strFieldValue6.c_str(), nCoastPoint);

                     // Create the feature in the output layer
                     if (pOGRLayer->CreateFeature(pOGRFeature) != OGRERR_NONE)
                     {
                        cerr << ERR << "cannot create  " << strType << " feature " << strPlotTitle << " for coast " << i << " and profile " << j << " in " << strFilePathName << "\n" << CPLGetLastErrorMsg() << endl;
                        return false;
                     }

                     // Tidy up: get rid of the feature object
                     OGRFeature::DestroyFeature(pOGRFeature);
                  }
               }
            }
         }  
         

         break;
      }
   }

   // Get rid of the dataset object
   GDALClose(pGDALDataSet);

   return true;
}

/*==============================================================================================================================

 Reads a vector User defined Coastline data to the vector array

===============================================================================================================================*/
int CDelineation::nReadVectorCoastlineData(void)
{
   // Open the User defined coastline  << from https://gdal.org/tutorials/vector_api_tut.html
   GDALDataset       *poDS;
   poDS = (GDALDataset*) GDALOpenEx( m_strInitialCoastlineFile.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL );
   if (poDS == NULL)
   {
      // Can't open file (note will already have sent GDAL error message to stdout)
      cerr << ERR << "cannot open " << m_strInitialCoastlineFile << " for input: " << CPLGetLastErrorMsg() << endl;
      return RTN_ERR_VECTOR_FILE_READ;
   }
   
   // Get shapefile properties
   m_strOGRICDriverCode = poDS->GetDriver()->GetDescription();
   // m_strOGRICDataType = poDS->G;
   //m_strOGRICDataValue = 
   //m_strOGRICGeometry = 
   
   // Check how many layers the User defined coastline vector shape have
   int nLayers = poDS->GetLayerCount();
    if (nLayers > 1)
   {
      // Warn the user that only one layer of Coastline vector is used
      cout << WARN << "user coastline " << m_strInitialCoastlineFile << " has more than one layer, only first one is used" << endl;
   }
   // Read in the first layer of vector Coastline
   OGRLayer  *poLayer;

   poLayer = poDS->GetLayer(0);
   poLayer->ResetReading();
   OGRFeature *poFeature;

   int nCoast = static_cast<int>(m_VUserCoast.size()-1); // index of the last coast object in the vector array

   while( (poFeature = poLayer->GetNextFeature()) != NULL )
    {
        OGRGeometry *poGeometry = poFeature->GetGeometryRef();
        if( poGeometry != NULL && wkbFlatten(poGeometry->getGeometryType()) == wkbPoint)
        {
            OGRPoint *poPoint = (OGRPoint *) poGeometry;
      	    m_VUserCoast[nCoast].AppendToCoastline( poPoint->getX(), poPoint->getY() );
            //printf( "%.2f,%.2f,%0lld\n", poPoint->getX(), poPoint->getY(), poFeature->GetFID());
        }
        else
        {
            // Error: Geometry type of Shape file for user defined coastline is not Point type
            cerr << ERR << "Not Point geometry type in " << m_strInitialCoastlineFile << endl;
            return RTN_ERR_VECTOR_GIS_OUT_FORMAT;
        }
        OGRFeature::DestroyFeature( poFeature );
    }

   GDALClose( poDS );

   return RTN_OK;
}

/*==============================================================================================================================

 Reads a vector User defined MCoastline data to the vector array

===============================================================================================================================*/


int CDelineation::nReadVectorMCoastlineData(void)
{
   // Open the User defined coastline  << from https://gdal.org/tutorials/vector_api_tut.html
   GDALDataset *poDS;
   poDS = (GDALDataset*) GDALOpenEx(m_strMulticoastlineFile.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
   if (poDS == NULL)
   {
      // Can't open file (note will already have sent GDAL error message to stdout)
      cerr << ERR << "cannot open " << m_strMulticoastlineFile << " for input: " << CPLGetLastErrorMsg() << endl;
      return RTN_ERR_VECTOR_FILE_READ;
   }
   
   // Get shapefile properties
   m_strOGRICDriverCode = poDS->GetDriver()->GetDescription();

   int nLayers = poDS->GetLayerCount();
   if (nLayers > 1)
   {
      // Warn the user that only one layer of Coastline vector is used
      cout << WARN << "user coastline " << m_strMulticoastlineFile << " has more than one layer, only first one is used" << endl;
   }

   // Read in the first layer of vector Coastline
   OGRLayer *poLayer;
   poLayer = poDS->GetLayer(0);
   m_poSRS.reset(poLayer->GetSpatialRef()->Clone()); // Read the CRS reference of the historical waterlines
   
   if (!m_poSRS.get()) {
        printf("Failed to get the CRS from the Historical Shorelines layer.\n");
        return false;
    }

   poLayer->ResetReading();
   OGRFeature *poFeature;

   while ((poFeature = poLayer->GetNextFeature()) != NULL)
   {
      OGRGeometry *poGeometry = poFeature->GetGeometryRef();
      if (poGeometry != NULL)
      {
         OGRwkbGeometryType eType = wkbFlatten(poGeometry->getGeometryType());
         if (eType == wkbLineString || eType == wkbMultiLineString)
         {
            // Geometry type is LineString or MultiLineString, verified successfully
            OGRFeature::DestroyFeature(poFeature);
            continue;
         }
         else
         {
            // Error: Geometry type of Shape file for user defined coastline is not LineString or MultiLineString type
            cerr << ERR << "Not LineString or MultiLineString geometry type in " << m_strMulticoastlineFile << endl;
            OGRFeature::DestroyFeature(poFeature);
            return RTN_ERR_VECTOR_GIS_OUT_FORMAT;
         }
      }
      else
      {
         // Error: Geometry is NULL
         cerr << ERR << "Geometry is NULL in " << m_strMulticoastlineFile << endl;
         OGRFeature::DestroyFeature(poFeature);
         return RTN_ERR_VECTOR_GIS_OUT_FORMAT;
      }
   }

   GDALClose(poDS);
   return RTN_OK;
}
