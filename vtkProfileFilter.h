/*=========================================================================

  Program:   AstroViz plugin for ParaView
  Module:    $RCSfile: vtkProfileFilter.h,v $

  Copyright (c) Christine Corbett Moran
  All rights reserved.
     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkProfileFilter - shrink cells composing an arbitrary data set
// .SECTION Description
// Plots the mass function N(>M) as a scatter plot
// .SECTION See Also

#ifndef __vtkProfileFilter_h
#define __vtkProfileFilter_h

#include "vtkExtractHistogram.h"
#include "vtkDataSetAttributes.h" // needed to declare the field list below

//----------------------------------------------------------------------------
enum BinUpdateType
{
	add, 
	multiply 
};

//----------------------------------------------------------------------------
class VTK_EXPORT vtkProfileFilter : public vtkExtractHistogram
{
public:
  static vtkProfileFilter* New();
  vtkTypeRevisionMacro(vtkProfileFilter, vtkExtractHistogram);
  void PrintSelf(ostream& os, vtkIndent indent);
  // Description:
  // Get/Set the density parameter
  vtkSetMacro(Delta, double);
  vtkGetMacro(Delta, double);
  // Description:
  // Get/Set the number of bins
  vtkSetVector3Macro(Center,double);
  vtkGetVectorMacro(Center,double,3);

  // Description:
  // Get/Set whether the bins should be only from the center to the virial 
	// radius
	vtkSetMacro(CutOffAtVirialRadius,int);
	vtkGetMacro(CutOffAtVirialRadius,int);

  // Description:
  // Specify the point locations used to probe input. Any geometry
  // can be used. New style. Equivalent to SetInputConnection(1, algOutput).
  void SetSourceConnection(vtkAlgorithmOutput* algOutput);
//BTX
protected:
  vtkProfileFilter();
  ~vtkProfileFilter();
  // Description:
  // This is called within ProcessRequest when a request asks the algorithm
  // to do its work. This is the method you should override to do whatever the
  // algorithm is designed to do. This happens during the fourth pass in the
  // pipeline execution process.
  virtual int RequestData(vtkInformation*, 
                          vtkInformationVector**, 
                          vtkInformationVector*);
  // Description:
	// Set in GUI, with defaults
	// Overdensity
	double Delta; 
  // Description:
	// Center around which to compute radial bins
	double Center[3];
  // Description:
	// Whether to only compute the profile up to the virial radius
	int CutOffAtVirialRadius;
  // Description:
	// Number of bins to use
	int BinNumber;
  // Description:
	// Spacing between bins, automatically calculated based upon other
	// selections
	double BinSpacing;
  // Description:
	// Quantities to add to the input computing averages for. Note:
	// if you were to add to this, you would also have to update the function
	// CalculateAdditionalProfileQuantity
	vtkStringArray* AdditionalProfileQuantities;
  // Description:
 	// Quantities to compute cumulative Sum(<=binR) for in the output table.
 	// Note: the name of this quantity must either be definined in the 
	// input data or in the array AdditionalProfileQuantities, with 
	// corresponding modification to CalculateAdditionalProfileQuantity
	vtkStringArray* CumulativeQuantities; 	
	  // Description:
  // Build the field lists containing the central point to be probed
  void CalculateAndSetCenter(vtkDataSet* source);

	// Description:
	// Calculates the bin spacing and number of bins if necessary from
	// the user input.
	//
	// For each dataarray in the input, define a total and an
	// average column in the binned output table.
	//
	// For each additional quantity as specified by the 
	// AdditionalProfileQuantities array, define a average column in the
	// binned output table
	//
	// For each cumulative array as specified by the CumulativeQuantitiesArray
	//
	void InitializeBins(vtkPolyData input,vtkTable* output);

	// Description:
	// Calculates the bin spacing and number of bins if necessary from
	// the user input.
	void CalculateAndSetBinExtents(vtkPolyData input);

	// Description:
	// For each point in the input, update the relevant bins and bin columns
	// to reflect this point's data. Finally compute the averages, relevant
	// dispersions, and global statistics.
	void ComputeStatistics(vtkPolyData* input,vtkTable* output);

	// Description:
	// For each quantity initialized in InitializeBins updates the statistics
	// of the correct bin to reflect the data values of the point identified
	// with pointGlobalId. Note: for quantities that are averages, or require
	// post processing this are updated additively as with totals. This is
	// why after all points have updated the bin statistics,
	// BinAveragesAndPostprocessing must be called to do the proper averaging
	// and/or postprocessing on  the accumlated columns.
	void UpdateBinStatistics(UpdateBinStatistics(vtkPolyData* input,
		vtkIdType pointGlobalId,vtkTable* output);
	
	// Description:
	// returns the bin number in which this radius lies.
	int GetBinNum(double r[]);
	
	// Description:
	// Updates the data values of attribute specified in attributeName
	// in the bin specified by binNum, either additively or 	
	// multiplicatively as specified by updateddType by dataToAdd
	void UpdateBin(int binNum, BinUpdateType updateType, char* attributeName,
		int attributeNumComponents, double* dataToAdd, vtkTable* output);
		
	// Description:
	// This function is useful for those data items who want to keep track of 
	// a cumulative number. E.g. N(<=r), calls updateBin on all bins >= binNum
	// updating the attribute specified
	void UpdateCumulativeBins(int binNum, BinUpdateType updateType, 
		char* attributeName, int attributeNumComponents, double* dataToAdd,
		vtkTable* output);

	// Description:
	// Base upon the additionalQuantityName, returns a double array representing
	// the computation of this quantity. Currently all of these are calculated
	// from v and from r, which are 3-vectors taken as inputs. Would have to be 
	// rediefined to be more general if other quantities were desired.
	double* CalculateAdditionalProfileQuantity(vtkstd::string
		additionalQuantityName,double v[], double r[]);
	
	// Description:
   // After all points have updated the bin statistics, UpdateBinAverages
	// must be called to do the proper averaging and/or postprocessing on 
	// the accumlated columns.
	void BinAveragesAndPostprocessing(vtkTable* output);
		
private:
  vtkProfileFilter(const vtkProfileFilter&); // Not implemented
  void operator=(const vtkProfileFilter&); // Not implemented
//ETX
};

#endif


