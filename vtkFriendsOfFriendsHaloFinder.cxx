/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile: vtkFriendsOfFriendsHaloFinder.cxx,v $
=========================================================================*/
#include "vtkFriendsOfFriendsHaloFinder.h"
#include "vtkIdList.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkSmartPointer.h"
#include "vtkPKdTree.h"
#include "vtkPointSet.h"
#include "vtkPoints.h"
#include "vtkGenericPointIterator.h"
#include "vtkDataArray.h"
#include "vtkMath.h"
#include "vtkMultiProcessController.h"
#include "vtkPKdTree.h"
#include "vtkDistributedDataFilter.h"
#include "vtkCallbackCommand.h"
#include <vtkstd/vector>
#include "astrovizhelpers/DataSetHelpers.h"


vtkCxxRevisionMacro(vtkFriendsOfFriendsHaloFinder, "$Revision: 1.72 $");
vtkStandardNewMacro(vtkFriendsOfFriendsHaloFinder);
vtkCxxSetObjectMacro(vtkFriendsOfFriendsHaloFinder,Controller,
	vtkMultiProcessController);
vtkCxxSetObjectMacro(vtkFriendsOfFriendsHaloFinder,PKdTree,vtkPKdTree);
vtkCxxSetObjectMacro(vtkFriendsOfFriendsHaloFinder,D3,
	vtkDistributedDataFilter);

//----------------------------------------------------------------------------
vtkFriendsOfFriendsHaloFinder::vtkFriendsOfFriendsHaloFinder()
{
  this->LinkingLength = 1e-6; //default
	this->PKdTree  = NULL;
	this->Controller = NULL;
	this->D3 = NULL;
  this->SetController(vtkMultiProcessController::GetGlobalController());
}

//----------------------------------------------------------------------------
vtkFriendsOfFriendsHaloFinder::~vtkFriendsOfFriendsHaloFinder()
{
  this->SetPKdTree(NULL);
  this->SetController(NULL);
  this->SetD3(NULL);
}

//----------------------------------------------------------------------------
void vtkFriendsOfFriendsHaloFinder::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
  os << indent << "Linking Length: " << this->LinkingLength << "\n";
}

//----------------------------------------------------------------------------
int vtkFriendsOfFriendsHaloFinder::FillInputPortInformation(int, vtkInformation* info)
{
  // This filter uses the vtkDataSet cell traversal methods so it
  // suppors any data set type as input.
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkPointSet");
  return 1;
}

int vtkFriendsOfFriendsHaloFinder::FindHaloes(vtkPointSet* input,
	vtkPointSet* output)
{
	// Building the Kd tree, should already be built
	vtkSmartPointer<vtkPKdTree> pointTree = vtkSmartPointer<vtkPKdTree>::New();
		pointTree->BuildLocatorFromPoints(input);
	// calculating the initial haloes
	vtkSmartPointer<vtkIdTypeArray> haloIdArray = \
		pointTree->BuildMapForDuplicatePoints(this->LinkingLength);
	haloIdArray->SetNumberOfComponents(1);
	haloIdArray->SetNumberOfTuples(input->GetPoints()->GetNumberOfPoints());
	haloIdArray->SetName("halo ID");
	// Now assign halos, if this point has at least one other pair,
	// it is a halo, if not it is not (set to 0)
	vtkSmartPointer<vtkIdTypeArray> halo = \
		vtkSmartPointer<vtkIdTypeArray>::New();
	halo->SetNumberOfComponents(1);
	halo->SetNumberOfTuples(input->GetPoints()->GetNumberOfPoints());
	for(int nextHaloId = 0;
		nextHaloId < haloIdArray->GetNumberOfTuples();
	 	++nextHaloId)
		{
		haloIdArray->GetTuples(nextHaloId,nextHaloId,halo);
		if(halo->GetNumberOfTuples()<2)
			{
			// this means this point does not belong to a halo, as it has no pair
			// setting its value to zero
			haloIdArray->SetValue(nextHaloId,0);
			}
		}
	output->GetPointData()->AddArray(haloIdArray);
}

//----------------------------------------------------------------------------
int vtkFriendsOfFriendsHaloFinder::RequestData(vtkInformation*,
                                 vtkInformationVector** inputVector,
                                 vtkInformationVector* outputVector)
{
  // Get input and output data.
  vtkPointSet* input = vtkPointSet::GetData(inputVector[0]);
  vtkPointSet* output = vtkPointSet::GetData(outputVector);
  output->ShallowCopy(input);
	this->FindHaloes(input,output);
	// Finally, some memory management
  output->Squeeze();
  return 1;
}
