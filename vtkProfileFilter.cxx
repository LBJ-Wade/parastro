/*=========================================================================

  Program:   AstroViz plugin for ParaView
  Module:    $RCSfile: vtkProfileFilter.cxx,v $
=========================================================================*/
#include "vtkProfileFilter.h"
#include "AstroVizHelpersLib/AstroVizHelpers.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkDoubleArray.h"
#include "vtkIntArray.h"
#include "vtkCellData.h"
#include "vtkTable.h"
#include "vtkSortDataArray.h"
#include "vtkMath.h"
#include "vtkInformationDataObjectKey.h"
#include "vtkPointSet.h" 
#include "vtkMultiProcessController.h"
#include "vtkSmartPointer.h"
#include "vtkPointData.h"
#include "vtkLine.h"
#include "vtkPlane.h"
#include <cmath>
using vtkstd::string;

vtkCxxRevisionMacro(vtkProfileFilter, "$Revision: 1.72 $");
vtkStandardNewMacro(vtkProfileFilter);
vtkCxxSetObjectMacro(vtkProfileFilter, Controller, vtkMultiProcessController);
//----------------------------------------------------------------------------
vtkProfileFilter::vtkProfileFilter()
{
  this->SetNumberOfInputPorts(2);
	this->SetInputArrayToProcess(
    0,
    0,
    0,
    vtkDataObject::FIELD_ASSOCIATION_POINTS_THEN_CELLS,
    vtkDataSetAttributes::SCALARS);
	// Defaults for quantities which will be computed based on user's
	// later input
	this->MaxR=1.0;
	this->Delta=1; 
	this->BinNumber=30;
	this->Controller = NULL;
  this->SetController(vtkMultiProcessController::GetGlobalController());
}

//----------------------------------------------------------------------------
vtkProfileFilter::~vtkProfileFilter()
{
 	this->SetController(0);
}

//----------------------------------------------------------------------------
void vtkProfileFilter::PrintSelf(ostream& os, vtkIndent indent)
{
  os << indent << "bin number: " << this->BinNumber << "\n";
}

//----------------------------------------------------------------------------
void vtkProfileFilter::SetSourceConnection(vtkAlgorithmOutput* algOutput)
{
  this->SetInputConnection(1, algOutput);
}

int vtkProfileFilter::FillInputPortInformation (int port, 
	vtkInformation *info)
{
  this->Superclass::FillInputPortInformation(port, info);
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkPointSet");
  return 1;
}

//----------------------------------------------------------------------------
int vtkProfileFilter::RequestData(vtkInformation *request,
	vtkInformationVector **inputVector,
	vtkInformationVector *outputVector)
{
	// Now we can get the input with which we want to work
 	vtkPointSet* input = vtkPointSet::GetData(inputVector[0]);
	// Will set the center based upon the selection in the GUI
	vtkDataSet* centerInfo = vtkDataSet::GetData(inputVector[1]);
	// Get name of data array containing mass
	vtkDataArray* massArray = this->GetInputArrayToProcess(0, inputVector);
  if (!massArray)
    {
    vtkErrorMacro("Failed to locate mass array");
    return 0;
  	}
	vtkTable* const output = vtkTable::GetData(outputVector,0);
	output->Initialize();

	// Choosing which quantities to profile. Right now choosing all,
	// could later by modified to use user's input to select
	this->AdditionalProfileQuantities.push_back(
		ProfileElement("angular momentum",3,&ComputeAngularMomentum,AVERAGE));
	this->AdditionalProfileQuantities.push_back(
		ProfileElement("radial velocity",3,&ComputeRadialVelocity,AVERAGE));
	this->AdditionalProfileQuantities.push_back(
		ProfileElement("tangential velocity",3,&ComputeTangentialVelocity,
		AVERAGE));
	this->AdditionalProfileQuantities.push_back(
		ProfileElement("velocity squared",1,&ComputeVelocitySquared,AVERAGE));
	this->AdditionalProfileQuantities.push_back(
		ProfileElement("radial velocity squared",1,&ComputeRadialVelocitySquared,
		AVERAGE));
	this->AdditionalProfileQuantities.push_back(
		ProfileElement("tangential velocity squared",1,
		&ComputeTangentialVelocitySquared,AVERAGE));
	// These use a different constructor as they are elements to be
	// postprocessed. 
	this->AdditionalProfileQuantities.push_back(
		ProfileElement("velocity dispersion",3,
		&ComputeVelocityDispersion,
		"velocity squared",AVERAGE,
		"velocity",AVERAGE));
	this->AdditionalProfileQuantities.push_back(
		ProfileElement("tangential velocity dispersion",3,
		&ComputeVelocityDispersion,
		"tangential velocity squared",AVERAGE,
		"tangential velocity",AVERAGE));
	this->AdditionalProfileQuantities.push_back(
		ProfileElement("radial velocity dispersion",3,
		&ComputeVelocityDispersion,
		"radial velocity squared",AVERAGE,
		"radial velocity",AVERAGE));
	this->AdditionalProfileQuantities.push_back(
		ProfileElement("circular velocity",1,
		&ComputeCircularVelocity,
		massArray->GetName(),CUMULATIVE,
		"bin radius",TOTAL));
	this->AdditionalProfileQuantities.push_back(
		ProfileElement("density",1,
		&ComputeDensity,
		massArray->GetName(),CUMULATIVE,
		"bin radius",TOTAL));
	// runs in parallel, syncing class member data, if necessary, if not
	// functions in serial
	this->SetBoundsAndBinExtents(input,centerInfo); 
	if(RunInParallel(this->Controller))
		{
		int procId=this->Controller->GetLocalProcessId();
		int numProc=this->Controller->GetNumberOfProcesses();
		vtkSmartPointer<vtkTable> localTable = \
			vtkSmartPointer<vtkTable>::New();
		localTable->Initialize();
		if(procId==0)
			{
			// only take the time to initialize on process 0
			this->InitializeBins(input,localTable);
			// Syncronizing the intialized table with the other processes
			this->Controller->Broadcast(localTable,0);
			this->UpdateStatistics(input,localTable);
			// Receive computations from each process and merge the table into
			// the localTable of process 0
			for(int proc = 1; proc < numProc; ++proc)
				{
				vtkSmartPointer<vtkTable> recLocalTable = \
					vtkSmartPointer<vtkTable>::New();
				recLocalTable->Initialize();
				this->Controller->Receive(recLocalTable,proc,DATA_TABLE);
				this->MergeTables(input,localTable,recLocalTable);
				}
			// Perform final computations
			// Updating averages and doing relevant postprocessing
			this->BinAveragesAndPostprocessing(input,localTable);
			// Copy process 0's local table to output, which should have collected
			// answer
			output->DeepCopy(localTable);
			}
		else
			{
			// Syncing initialized, empty table
			this->Controller->Broadcast(localTable,0);
			// Updating table with the data on this processor
			this->UpdateStatistics(input,localTable);
			// sending result to root
			this->Controller->Send(localTable,0,DATA_TABLE);
			}
		}	
	else
		{
		this->InitializeBins(input,output);
		this->UpdateStatistics(input,output);
		// Updating averages and doing relevant postprocessing
		this->BinAveragesAndPostprocessing(input,output);
		}
	return 1;
}

//----------------------------------------------------------------------------
void vtkProfileFilter::MergeTables(vtkPointSet* input,
	vtkTable* originalTable, vtkTable* tableToMerge)
{
	assert(originalTable->GetNumberOfRows()==tableToMerge->GetNumberOfRows());
	for(int binNum = 0; binNum < originalTable->GetNumberOfRows(); ++binNum)
		{
		this->MergeBins(binNum,ADD,"number in bin",TOTAL,
			originalTable,tableToMerge);	
		this->MergeBins(binNum,ADD,"number in bin",CUMULATIVE,
			originalTable,tableToMerge);
		assert(0<=binNum<=this->BinNumber);
		// Updating quanties for the input data arrays
		for(int i = 0; i < input->GetPointData()->GetNumberOfArrays(); ++i)
			{
			vtkSmartPointer<vtkDataArray> nextArray = \
			 	input->GetPointData()->GetArray(i);
			// Merging the bins
			string baseName = nextArray->GetName();
			this->MergeBins(binNum,ADD,baseName,TOTAL,originalTable,tableToMerge);
			this->MergeBins(binNum,ADD,baseName,AVERAGE,originalTable,tableToMerge);	
			this->MergeBins(binNum,ADD,baseName,CUMULATIVE,
				originalTable,tableToMerge);
			}
		for(int i = 0; i < this->AdditionalProfileQuantities.size(); ++i)
			{
			ProfileElement nextElement=this->AdditionalProfileQuantities[i];
			if(!nextElement.Postprocess)
				{
				this->MergeBins(binNum,ADD,nextElement.BaseName,
					nextElement.ProfileColumnType, 
					originalTable,tableToMerge);
				}
			}
		}
}

//----------------------------------------------------------------------------
void vtkProfileFilter::SetBoundsAndBinExtents(vtkPointSet* input, 
	vtkDataSet* source)
{
	if(RunInParallel(this->Controller))
		{
		int procId=this->Controller->GetLocalProcessId();
		int numProc=this->Controller->GetNumberOfProcesses();
		if(procId==0)
			{
			double* sourceCenter=CalculateCenter(source);
			for(int i = 0; i < 3; ++i)
				{
				this->Center[i]=sourceCenter[i];
				}
			// Syncronizing the centers
			this->Controller->Broadcast(this->Center,3,0);			
			}
		else
			{
			// Syncronizing the centers
			this->Controller->Broadcast(this->Center,3,0);
			}
		// calculating the max R
		this->MaxR=ComputeMaxRadiusInParallel(this->Controller,
			input,this->Center);
		}
	else
		{
		// we aren't using MPI or have only one process
		double* sourceCenter=CalculateCenter(source);
		for(int i = 0; i < 3; ++i)
			{
			this->Center[i]=sourceCenter[i];
			}
		//calculating the the max R
		this->MaxR=ComputeMaxR(input,this->Center);			
		}
	// this->MaxR, this->BinNumber are already set/synced
	// whether we are in parallel or serial, and each process can perform
	// computation on its own
	this->BinSpacing=this->CalculateBinSpacing(this->MaxR,this->BinNumber);
}

//----------------------------------------------------------------------------
int vtkProfileFilter::GetBinNumber(double x[])
{
  double distanceToCenter;
  if(this->ProfileAxis[0]==0 && this->ProfileAxis[1]==0 && this->ProfileAxis[2]==0) {// spherical profile
		distanceToCenter = sqrt(vtkMath::Distance2BetweenPoints(this->Center,x));
  }	
  else {
    distanceToCenter = vtkLine::DistanceToLine(x, this->Center, this->ProfileAxis);
  }
  
  if(this->ProfileHeight!=0) {
    if(this->ProfileAxis[0]==0 && this->ProfileAxis[1]==0 && this->ProfileAxis[2]==0){ //spherical profile
      if(distanceToCenter > this->ProfileHeight){
        return -1;
      }
    }
    else {
      double projectedPoint[3];
      vtkPlane::ProjectPoint(x, this->Center, this->ProfileAxis, projectedPoint); // TODO: this ProfileAxis is not the correct normal vector 
      if( sqrt(vtkMath::Distance2BetweenPoints(x,projectedPoint)) > this->ProfileHeight) {
        return -1;
      }
    }
    
  }  
    
  return floor(distanceToCenter/this->BinSpacing);
}


//----------------------------------------------------------------------------
void vtkProfileFilter::InitializeBins(vtkPointSet* input,
	vtkTable* output)
{
	// the first column will be the bin radius (max)
	string binRadiusColumnName=this->GetColumnName("bin radius",
		TOTAL);
  // the second column will  be the bin radius (min)
  string binRadiusMinColumnName=this->GetColumnName("bin radius min",
                                                 TOTAL);
	AllocateDataArray(output,binRadiusColumnName.c_str(),1,this->BinNumber);
  AllocateDataArray(output,binRadiusMinColumnName.c_str(),1,this->BinNumber);

	// setting the bin radii in the output
	for(int binNum = 0; binNum < this->BinNumber; ++binNum)
		{
		this->UpdateBin(binNum,SET,
			"bin radius",TOTAL,(binNum+1)*this->BinSpacing,output);
    this->UpdateBin(binNum,SET,
       "bin radius min",TOTAL,binNum*this->BinSpacing,output);
      
		}
  
  
	// always need this for averages
	AllocateDataArray(output,GetColumnName("number in bin",TOTAL).c_str(),
		1,this->BinNumber);
	AllocateDataArray(output,GetColumnName("number in bin",
		CUMULATIVE).c_str(),1,this->BinNumber);
	for(int i = 0; i < input->GetPointData()->GetNumberOfArrays(); ++i)
		{
		// Next array data
		vtkSmartPointer<vtkDataArray> nextArray = \
		 	input->GetPointData()->GetArray(i);
		int numComponents = nextArray->GetNumberOfComponents();
		string baseName = nextArray->GetName();
		AllocateDataArray(output,GetColumnName(baseName,TOTAL).c_str(),
				numComponents,this->BinNumber);
		AllocateDataArray(output,GetColumnName(baseName,AVERAGE).c_str(),
			numComponents,this->BinNumber);
		AllocateDataArray(output,GetColumnName(baseName,CUMULATIVE).c_str(),
			numComponents,this->BinNumber);
		}
	// For our additional quantities, allocating a column for the average 
	// and the sum. These are currently restricted to be 3-vectors
	for(int i = 0; 
		i < this->AdditionalProfileQuantities.size();
	 	++i)
		{
		ProfileElement nextElement = this->AdditionalProfileQuantities[i];
		AllocateDataArray(output,
			GetColumnName(nextElement.BaseName,
			nextElement.ProfileColumnType).c_str(),nextElement.NumberComponents,
			this->BinNumber);
		}
}

//----------------------------------------------------------------------------
double vtkProfileFilter::CalculateBinSpacing(double maxR,int binNumber)
{
	return maxR/binNumber;
}

//----------------------------------------------------------------------------
void vtkProfileFilter::UpdateStatistics(vtkPointSet* input,vtkTable* output)
{
	for(unsigned long nextPointId = 0;
	 		nextPointId < input->GetPoints()->GetNumberOfPoints();
	 		++nextPointId)
		{
			this->UpdateBinStatistics(input,nextPointId,output);
		}
}

//----------------------------------------------------------------------------
void vtkProfileFilter::UpdateBinStatistics(vtkPointSet* input,
 	vtkIdType pointGlobalId,vtkTable* output)
{
	double* x = GetPoint(input,pointGlobalId);
	// As we bin by radius always need
	double* r=PointVectorDifference(x,this->Center);
	// Many of the quantities explicitely require the velocity
	double* v=GetDataValue(input,"velocity",pointGlobalId);
	int binNum=this->GetBinNumber(x);
  if(binNum < 0){
    // This indicates the point is not to be included.
    return;
  }
	this->UpdateBin(binNum,ADD,"number in bin",TOTAL,1.0,output);	
	this->UpdateCumulativeBins(binNum,ADD,
		"number in bin",CUMULATIVE,1.0,output);	
	assert(0<=binNum<=this->BinNumber);
	// Updating quanties for the input data arrays
	for(int i = 0; i < input->GetPointData()->GetNumberOfArrays(); ++i)
		{
		vtkSmartPointer<vtkDataArray> nextArray = \
		 	input->GetPointData()->GetArray(i);
		// getting the data for this point
		double* nextData = GetDataValue(input,
			nextArray->GetName(),pointGlobalId);
		// Updating the total bin
		string baseName = nextArray->GetName();
		this->UpdateBin(binNum,ADD,baseName,TOTAL,nextData, output);	
		this->UpdateBin(binNum,ADD,baseName,AVERAGE,nextData, output);	
		this->UpdateCumulativeBins(binNum,ADD,baseName,CUMULATIVE,nextData,
			output);
		//Finally some memory management
		delete [] nextData;
		}
	for(int i = 0; i < 
		this->AdditionalProfileQuantities.size(); 
		++i)
		{
		ProfileElement nextElement=this->AdditionalProfileQuantities[i];
		if(!nextElement.Postprocess)
			{
			double* additionalData = \
				nextElement.Function(v,r);
			this->UpdateBin(binNum,ADD,nextElement.BaseName,
				nextElement.ProfileColumnType,additionalData,output);
			}
		}
	// Finally some memory management
	delete [] x;
	delete [] r;
	delete [] v;
}

//----------------------------------------------------------------------------
void 	vtkProfileFilter::BinAveragesAndPostprocessing(
	vtkPointSet* input,vtkTable* output)
{
	for(int binNum = 0; binNum < this->BinNumber; ++binNum)
		{
		int binSize=output->GetValueByName(binNum,
			GetColumnName("number in bin",TOTAL).c_str()).ToInt();
		// For each input array, update its average column by getting
		// the total from the total column then dividing by 
		// the number in the bin. Only do this if the number in the bin
		// is greater than zero
			if(binSize>0)
				{
				vtkSmartPointer<vtkDataArray> nextArray;
				for(int i = 0; i < input->GetPointData()->GetNumberOfArrays(); ++i)
					{
					nextArray = input->GetPointData()->GetArray(i);
					string baseName = nextArray->GetName();
					this->UpdateBin(binNum,MULTIPLY,baseName,AVERAGE,1./binSize,output);
					}
				// For each additional quantity we also divide by N in the average
				// if requested
				for(int i = 0; 
					i < this->AdditionalProfileQuantities.size();
				 	++i)
					{
					ProfileElement nextElement = \
						this->AdditionalProfileQuantities[i];
					if(nextElement.ProfileColumnType==AVERAGE)
						{
						this->UpdateBin(binNum,MULTIPLY,nextElement.BaseName,
							AVERAGE,1./binSize,output);
						}
					}
				}

		// Finally post processing those items which are marked as such, don't
		// do this within the average loop as these quantities are allowed
		// to depend on averages themselves
		for(int i = 0; 
			i < this->AdditionalProfileQuantities.size();
		 	++i)
			{
			ProfileElement nextElement = \
				this->AdditionalProfileQuantities[i];
			if(nextElement.Postprocess)
				{
				vtkVariant argumentOne = \
		 			this->GetData(binNum, nextElement.ArgOneBaseName,
					nextElement.ArgOneColumnType, output);
				vtkVariant argumentTwo = \
					this->GetData(binNum, nextElement.ArgTwoBaseName,
					nextElement.ArgTwoColumnType,	output);
				double* updateData = \
					nextElement.PostProcessFunction(argumentOne,argumentTwo);
				this->UpdateBin(binNum,SET,nextElement.BaseName,TOTAL,
					updateData,output);
				// memory management
				delete [] updateData;
				}
			}
	}
}

//----------------------------------------------------------------------------
string vtkProfileFilter::GetColumnName(string baseName, 
	ColumnType columnType)
{
	switch(columnType)
		{
		case AVERAGE:
			return baseName+"_average";
		case TOTAL:
			return baseName+"_total";
		case CUMULATIVE:
			return baseName+"_cumulative";
		default:
			vtkDebugMacro("columnType not found for function GetColumnName, returning error string");
			return "error";
		}
}

//----------------------------------------------------------------------------
void vtkProfileFilter::UpdateBin(int binNum, BinUpdateType updateType,
 	string baseName, ColumnType columnType, double* updateData,
 	vtkTable* output)
{
	vtkVariant oldData = this->GetData(binNum,baseName,columnType,output);
	if(oldData.IsArray())
		{
		this->UpdateArrayBin(binNum,updateType,baseName,columnType,
			updateData,oldData.ToArray(),output);
		}
	else
		{
		this->UpdateDoubleBin(binNum,updateType,baseName,columnType,
			updateData[0],oldData.ToDouble(),output);
		}
}

//----------------------------------------------------------------------------
void vtkProfileFilter::MergeBins(int binNum, BinUpdateType updateType,
 	string baseName, ColumnType columnType, vtkTable* originalTable,
	vtkTable* tableToMerge)
{
	vtkVariant originalData = this->GetData(binNum,baseName,columnType,
		originalTable);
	vtkVariant mergeData = this->GetData(binNum,baseName,columnType,
		tableToMerge);
	if(originalData.IsArray() && mergeData.IsArray())
		{
		this->UpdateArrayBin(binNum,updateType,baseName,columnType,
			mergeData.ToArray(),originalData.ToArray(),originalTable);
		}
	else
		{
		assert(originalData.IsNumeric() && mergeData.IsNumeric());
		this->UpdateDoubleBin(binNum,updateType,baseName,columnType,
			mergeData.ToDouble(),originalData.ToDouble(),originalTable);
		}
}

//----------------------------------------------------------------------------
void vtkProfileFilter::UpdateBin(int binNum, BinUpdateType updateType,
 	string baseName, ColumnType columnType, double updateData,
 	vtkTable* output)
{
	vtkVariant oldData = this->GetData(binNum,baseName,columnType,output);
	if(oldData.IsArray())
		{
		int sizeOfUpdateData = oldData.ToArray()->GetNumberOfComponents();
		double* arrayUpdateData = new double[sizeOfUpdateData];
		for(int comp = 0; comp < sizeOfUpdateData; ++comp)
			{
			arrayUpdateData[comp] = updateData;
			}
		this->UpdateArrayBin(binNum,updateType,baseName,columnType,
			arrayUpdateData,oldData.ToArray(),output);
		// finally some memory management
		delete [] arrayUpdateData;
		}
	else
		{
		this->UpdateDoubleBin(binNum,updateType,baseName,columnType,
			updateData,oldData.ToDouble(),output);
		}
}

//----------------------------------------------------------------------------
void vtkProfileFilter::UpdateDoubleBin(int binNum, BinUpdateType updateType,
 	string baseName, ColumnType columnType, double updateData, double oldData,
 	vtkTable* output)
{
	switch(updateType)
		{
		case ADD:
			updateData+=oldData;
			break;
		case MULTIPLY:
			updateData*=oldData;
			break;
		case SET:
			break;
		}
		output->SetValueByName(binNum,
			GetColumnName(baseName,columnType).c_str(),updateData);
}

//----------------------------------------------------------------------------
void vtkProfileFilter::UpdateArrayBin(int binNum, BinUpdateType updateType,
 	string baseName, ColumnType columnType, vtkAbstractArray* updateData,
 	vtkAbstractArray* oldData, vtkTable* output)
{
	for(int comp = 0; comp < oldData->GetNumberOfComponents(); ++comp)
		{
		switch(updateType)
			{
			case ADD:
				oldData->InsertVariantValue(comp,
					updateData->GetVariantValue(comp).ToDouble()+\
					oldData->GetVariantValue(comp).ToDouble());
				break;
			case MULTIPLY:
				oldData->InsertVariantValue(comp,
					updateData->GetVariantValue(comp).ToDouble()*\
					oldData->GetVariantValue(comp).ToDouble());
				break;
			case SET:
				oldData->InsertVariantValue(comp,
					updateData->GetVariantValue(comp).ToDouble());
				break;
			}
		}
	output->SetValueByName(binNum,GetColumnName(baseName,columnType).c_str(),
		oldData);
}


//----------------------------------------------------------------------------
void vtkProfileFilter::UpdateArrayBin(int binNum, BinUpdateType updateType,
 	string baseName, ColumnType columnType, double* updateData,
 	vtkAbstractArray* oldData, vtkTable* output)
{

	for(int comp = 0; comp < oldData->GetNumberOfComponents(); ++comp)
		{
		switch(updateType)
			{
			case ADD:
				oldData->InsertVariantValue(comp,
					updateData[comp]+oldData->GetVariantValue(comp).ToDouble());
				break;
			case MULTIPLY:
				oldData->InsertVariantValue(comp,
					updateData[comp]*oldData->GetVariantValue(comp).ToDouble());
				break;
			case SET:
				oldData->InsertVariantValue(comp,updateData[comp]);
				break;
			}
		}
	output->SetValueByName(binNum,GetColumnName(baseName,columnType).c_str(),
		oldData);
}

//----------------------------------------------------------------------------
void vtkProfileFilter::UpdateCumulativeBins(int binNum, BinUpdateType 		
	updateType, string baseName, ColumnType columnType, double* dataToAdd,
	vtkTable* output)
{
	for(int bin = binNum; bin < this->BinNumber; ++bin)
		{
		this->UpdateBin(bin,updateType,baseName,columnType,dataToAdd,output);
		}
}

//----------------------------------------------------------------------------
void vtkProfileFilter::UpdateCumulativeBins(int binNum, BinUpdateType 		
	updateType, string baseName, ColumnType columnType, double dataToAdd,
	vtkTable* output)
{
	for(int bin = binNum; bin < this->BinNumber; ++bin)
		{
		this->UpdateBin(bin,updateType,baseName,columnType,dataToAdd,output);
		}
}
//----------------------------------------------------------------------------
vtkVariant vtkProfileFilter::GetData(int binNum, string baseName,
	ColumnType columnType, vtkTable* output)
{
	return output->GetValueByName(binNum,
		GetColumnName(baseName,columnType).c_str());
}

//----------------------------------------------------------------------------
vtkProfileFilter::ProfileElement::ProfileElement(string baseName, 
	int numberComponents, double* (*funcPtr)(double [], double []),
	ColumnType columnType)
{
	this->BaseName = baseName;
	this->NumberComponents = numberComponents;
	this->Function = funcPtr;
	this->ProfileColumnType = columnType;
	this->Postprocess = 0;	
}

vtkProfileFilter::ProfileElement::ProfileElement(string baseName, 
	int numberComponents, double* (*funcPtr)(vtkVariant, vtkVariant),
	string argOneBaseName, ColumnType argOneColumnType, 
	string argTwoBaseName, ColumnType argTwoColumnType)
{
	this->BaseName = baseName;
	this->NumberComponents = numberComponents;
	this->PostProcessFunction = funcPtr;
	this->ProfileColumnType = TOTAL;
	this->Postprocess = 1;
	this->ArgOneBaseName=argOneBaseName;
	this->ArgOneColumnType=argOneColumnType;
	this->ArgTwoBaseName=argTwoBaseName;
	this->ArgTwoColumnType=argTwoColumnType;
}

//----------------------------------------------------------------------------
vtkProfileFilter::ProfileElement::~ProfileElement()
{
	
}









