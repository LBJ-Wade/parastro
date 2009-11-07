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
#include "vtkTableAlgorithm.h" // super class
#include "vtkStringArray.h" // some class variables are vtkStringArrays

class vtkPolyData;
//----------------------------------------------------------------------------
enum BinUpdateType
{
	ADD, 
	MULTIPLY,
	SET
};

enum ColumnType
{
	AVERAGE,
	TOTAL,
	CUMULATIVE
};


//----------------------------------------------------------------------------
class VTK_EXPORT vtkProfileFilter : public vtkTableAlgorithm
{
public:
  static vtkProfileFilter* New();
  vtkTypeRevisionMacro(vtkProfileFilter, vtkTableAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent);
  // Description:
  // Get/Set the softening parameter
  vtkSetMacro(Softening, double);
  vtkGetMacro(Softening, double);
  // Description:
  // Get/Set the density parameter
  vtkSetMacro(Delta, double);
  vtkGetMacro(Delta, double);
  // Description:
  // Get/Set the center
  vtkSetVector3Macro(Center,double);
  vtkGetVectorMacro(Center,double,3);
  // Description:
  // Get/Set the number of bins
  vtkSetMacro(BinNumber,int);
  vtkGetMacro(BinNumber,int);
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
	// the ProfileElement protected nested class holds all the information
	// necessary to initialize and at runtime compute the value
	// of an additional profile element. Each of these are initialized
	// via the constructor
	// name : unique string name describing this element, 
	// only the base name, an affix will be added for any
	// quantities desired to be computed
	// number elements : the number of elements in each entry
	// funcPtr : the function to use to evaluate an update
	// average : if 1 compute the average of this quantity
	// total : if 1 compute the total of this quantity
	// cumulative: if 1 compute the cumulative value of this quantity
	// postprocess: if 1 only update during post processing
  class ProfileElement
  {
  public:
		vtkstd::string BaseName;
		int NumberComponents;
		double* (*Function)(double [], double []);
		double* (*PostProcessFunction)(vtkVariant, vtkVariant);
		ColumnType ProfileColumnType;
		int Postprocess;
		vtkstd::string ArgOneBaseName;
		ColumnType ArgOneColumnType;
		vtkstd::string ArgTwoBaseName;
		ColumnType ArgTwoColumnType;
		// Description:
		// quantities to be processed for each element in each bin with the
		// function *functPtr which takes in a radius and a velocity given
		// by double arrays
		ProfileElement(vtkstd::string baseName, int numberComponents,
			double* (*funcPtr)(double [], double []),
			ColumnType columnType);
		// Description:
		// if post processing is desired, then must specify two arguments, which
		// are profile elements. Their data (all computed) will be retrieved from
		// the output in postprocessing and a final computation will be performed
		// with functionPtr.
		//
		// The last four arguments specify which two columns
		// data should be handed to the postprocessing function, which
		// takes two vtkVariants as arguments and returns a double*,
		// thus requires that they are part of the input (for which
		//  CUMULATIVE,AVERAGE and TOTAL are computed for each array name)
		// or that they are specified as an additional profile element above
		ProfileElement(vtkstd::string baseName, int numberComponents,
			double* (*funcPtr)(vtkVariant, vtkVariant),
			vtkstd::string argOneBaseName, ColumnType argOneColumnType, 
			vtkstd::string argTwoBaseName, ColumnType argTwoColumnType);
		~ProfileElement();
 	};
	// Description:
	// Set in GUI, with defaults
	// Describes the softening of the simulation which can influence the 
	// root finding
	double Softening;
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
	// Max distance from center point to the data set boundaries, or to
	// the virial radius if applicable
	double MaxR;
	// Description:
	// Quantities to add to the input
	vtkstd::vector<ProfileElement> AdditionalProfileQuantities;
	// Description:
	// Generates the desired profile quantities and places them
	// in the output table by bin.
	void GenerateProfile(vtkPolyData* input,vtkTable* output);
  
	// Description:
  // Calculates the center and the maximum distance from the center
	// based upon the user's input and the boundaries of the dataset.
	void CalculateAndSetBounds(vtkPolyData* input, vtkDataSet* source);

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
	void InitializeBins(vtkPolyData* input, vtkTable* output);

	// Description:
	// Calculates the bin spacing and number of bins if necessary from
	// the user input.
	void CalculateAndSetBinExtents(vtkPolyData* input, vtkTable* output);

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
	void UpdateBinStatistics(vtkPolyData* input, 
		vtkIdType pointGlobalId,vtkTable* output);
	
	// Description:
	// returns the bin number in which this point lies.
	int GetBinNumber(double x[]);
	
	// Description:
	// Updates the data values of attribute specified in attributeName
	// in the bin specified by binNum, either additively or 	
	// multiplicatively as specified by updatetype by dataToAdd. Calls
	// either UpdateArrayBin or UpdateDoubleBin depending on the
	// type of data in the bin.
	void UpdateBin(int binNum, BinUpdateType updateType,
	 	vtkstd::string baseName, ColumnType columnType, double* updateData,
	 	vtkTable* output);
	
	// Description:
	// Overloaded updated bin, taking in a single double. If column to update
	// contains an array, also makes an array out of update data by replicating
	// it in each entry, then finally calls UpdateArrayBin
	void UpdateBin(int binNum, BinUpdateType updateType,
	 	vtkstd::string baseName, ColumnType columnType, double updateData,
		vtkTable* output);
	
	// Description:
	// If this bin contains an array, update with this method. size(updateData)
	// should be equal to size(oldData)
	void UpdateArrayBin(int binNum, BinUpdateType updateType,
	 	vtkstd::string baseName, ColumnType columnType, double* updateData,
		vtkAbstractArray* oldData, vtkTable* output);

	// Description:
	// If this bin contains a double, update with this method. 	
	void UpdateDoubleBin(int binNum, BinUpdateType updateType,
		vtkstd::string baseName, ColumnType columnType, double updateData,
	 	double oldData, vtkTable* output);
		
	// Description:
	// This function is useful for those data items who want to keep track of 
	// a cumulative number. E.g. N(<=r), calls updateBin on all bins >= binNum
	// updating the attribute specified
	void UpdateCumulativeBins(int binNum, BinUpdateType 		
		updateType, vtkstd::string baseName, ColumnType columnType,
		double* dataToAdd, vtkTable* output);

	// Description:
	// This function is useful for those data items who want to keep track of 
	// a cumulative number. E.g. N(<=r), calls updateBin on all bins >= binNum
	// updating the attribute specified. Identical to the one above, but calls
	// the double dataToAdd version of UpdatBin instead of the double*
	void UpdateCumulativeBins(int binNum, BinUpdateType 		
		updateType, vtkstd::string baseName, ColumnType columnType, 
		double dataToAdd, vtkTable* output);

	// Description:
	// Based upon the additionalQuantityName, returns a double
	// array representing the computation of this quantity. 
	// Always outputs a 3-vector, for quantities such as number in bin
	// which are scalars, result will be in the first position of the vector
	// and equal to the norm of the vector.
	// Currently all of these are calculated
	// from v and from r, which are 3-vectors taken as inputs. Would have to be 
	// rediefined to be more general if other quantities were desired.
	double* CalculateAdditionalProfileQuantity(vtkstd::string
		additionalQuantityName,double v[], double r[]);
	
	// Description:
   // After all points have updated the bin statistics, UpdateBinAverages
	// must be called to do the proper averaging and/or postprocessing on 
	// the accumlated columns.
	void BinAveragesAndPostprocessing(vtkPolyData* input, vtkTable* output);

	// Description:
	// given a base name, a variable index and a column type
	// (TOTAL,AVERAGE,or CUMULATIVE) returns a string representing
	// this data column's name
	vtkstd::string GetColumnName(vtkstd::string baseName, 
		ColumnType columnType);


	// Description:
	// Gets a column's data
	vtkVariant GetData(int binNum, vtkstd::string baseName,
		ColumnType columnType, vtkTable* output);

  virtual int FillInputPortInformation (int port, vtkInformation *info);
private:
  vtkProfileFilter(const vtkProfileFilter&); // Not implemented
  void operator=(const vtkProfileFilter&); // Not implemented
//ETX
};

#endif

