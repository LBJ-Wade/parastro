#include "ProfileHelpers.h"
#include "vtkPointData.h"
#include "vtkCellArray.h"
#include "vtkFloatArray.h"
#include "vtkDoubleArray.h"
#include "vtkIntArray.h"
#include "vtkSmartPointer.h"
#include "DataSetHelpers.h"
#include <assert.h>
#include <cmath>
#include "vtkMath.h"

//----------------------------------------------------------------------------
double IllinoisRootFinder(double (*func)(double,void *),void *ctx,\
											double r,double s,double xacc,double yacc,\
											int *pnIter) 
{
	// This code copied from Doug Potter's and Joachim Stadel's
	// pkdgrav, class master.c, method illinois
	// NOTE: Only use for positive roots. 
  const int maxIter = 100;
  double t,fr,fs,ft,phis,phir,gamma;
  int i;

  fr = func(r,ctx);
  fs = func(s,ctx);
	if(fr*fs>0)
		{
			// used to be an assert, but removing 
			throw "something went wrong with the root finding";
		}
  t = (s*fr - r*fs)/(fr - fs);

  for(i=0; i<maxIter && fabs(t-s) > xacc; ++i) 
		{
		ft = func(t,ctx);
		if (fabs(ft)<=yacc)
		 {
		 break;
		 }
		if(ft*fs<0) 
			{
	   	/*
	   	** Unmodified step.
	   	*/
	   	r = s;
	   	s = t;
	   	fr = fs;
	   	fs = ft;
			}
		else 
			{
	   	/*
	   	** Modified step to make sure we do not retain the 
	   	** endpoint r indefinitely.
	   	*/
			phis = ft/fs;
	    phir = ft/fr;
	    gamma = 1 - (phis/(1-phir));  /* method 3, for true illinois gamma=0.5*/
	    if(gamma < 0) 
				{
				gamma = 0.5;
	   		}
	   	fr *= gamma;
	   	s = t;
	   	fs = ft;
			}
		t = (s*fr - r*fs)/(fr - fs);
		}	
		
  if(pnIter)
		{
		*pnIter = i;
		}
		
  return(t);
}

//----------------------------------------------------------------------------
double ComputeMaxR(vtkPointSet* input,double point[])
{
	double bounds[6]; //xmin,xmax,ymin,ymax,zmin,zmax
	input->GetPoints()->ComputeBounds();
	input->GetPoints()->GetBounds(bounds);
	double maxR=0;
	double testR=0;
	// for each of the 8 corners of the bounding box, compute the 
	// distance to the point. maxR is the max distance.
	for(int x = 0; x < 2; ++x)
		{
		for(int y = 2; y <4; ++y)
			{
			for(int z = 4; z < 6; ++z)
				{
				double testCorner[3] = {bounds[x],bounds[y],bounds[z]};
				testR = sqrt(vtkMath::Distance2BetweenPoints(testCorner,point));
				// only if our test R is greater than the current max do we update
				maxR=std::max(maxR,testR);
				}
			}
		}
	return maxR;
}

//----------------------------------------------------------------------------
double OverDensityInSphere(double r,void* inputVirialRadiusInfo)
{
	VirialRadiusInfo* virialRadiusInfo = \
	 											static_cast<VirialRadiusInfo*>(inputVirialRadiusInfo);
	vtkSmartPointer<vtkIdList> pointsInRadius = \
																vtkSmartPointer<vtkIdList>::New();
	virialRadiusInfo->locator->FindPointsWithinRadius(r,
		virialRadiusInfo->center,
		pointsInRadius);
	// calculating the average mass, dividing this by the volume of the sphere
	// to get the density
	double totalMass=0;
	vtkPointSet* dataSet=\
		vtkPointSet::SafeDownCast(
		virialRadiusInfo->locator->GetDataSet());
	for(int pointLocalId = 0; 
			pointLocalId < pointsInRadius->GetNumberOfIds(); 
			++pointLocalId)
		{
		vtkIdType pointGlobalId = pointsInRadius->GetId(pointLocalId);
		double* nextPoint=GetPoint(dataSet,pointGlobalId);
		// extracting the mass
		// has to be double as this version of VTK doesn't have 
		double* mass=GetDataValue(dataSet,
															"mass",pointGlobalId);
		totalMass+=mass[0];
		// Finally, some memory management
		delete [] mass;
		delete [] nextPoint;
		}
	// Returning the density minus the critical density
	return totalMass/(4./3*M_PI*pow(r,3)) - virialRadiusInfo->criticalDensity;
}

VirialRadiusInfo ComputeVirialRadius(vtkPointSet* input,
																		double overdensity,double center[])
{
		// calculating the the max an min r of this pointset
		double maxR=ComputeMaxR(input,center);
		// Building the point locator and the struct to use as an 
		// input to the rootfinder.
		// 1. Building the point locator
		vtkPointLocator* locator = vtkPointLocator::New();
			locator->SetDataSet(input);
			locator->BuildLocator();
		// 2. Building the struct
		VirialRadiusInfo virialRadiusInfo;
		virialRadiusInfo.locator=locator;
		// copies the contents of center to virialRadiusInfo's arg center
		for(int i = 0; i < 3; ++i)
		{
			virialRadiusInfo.center[i]=center[i];
		}
		virialRadiusInfo.criticalDensity=overdensity;
		// but IllinoisRootFinder takes in a void pointer
		void* pntrVirialRadiusInfo = &virialRadiusInfo;
		// 3. Now we are ready to run the root finder
		int numIter=0;
		try
			{
			virialRadiusInfo.virialRadius=IllinoisRootFinder(OverDensityInSphere,
				pntrVirialRadiusInfo,\
				maxR,1e-11f,//minR is almost zero
				0.0,0.0,
			  &numIter);
			}
		catch (const char* e)
			{
				// This indicates that something has gone wrong with the root finding
				virialRadiusInfo.virialRadius=-1; 
			}
  	return virialRadiusInfo;
}

//----------------------------------------------------------------------------
vtkPolyData* CopyPolyPointsAndData(vtkPolyData* dataSet, vtkIdList*
 	pointsInRadius)
{
	// TODO: I was using CopyCells method of vtkPolyData
	// but this wasn't working so I decided to do manually
	// go back to finding the way using the VTK api to do this
	int numNewPoints=pointsInRadius->GetNumberOfIds();
	// Initilizing
	vtkPolyData* newDataSet = vtkPolyData::New(); // this memory must be managed
		// Initializing points and verts
	  newDataSet->SetPoints(vtkSmartPointer<vtkPoints>::New());
		newDataSet->SetVerts(vtkSmartPointer<vtkCellArray>::New());
	  // Initializing data
	vtkSmartPointer<vtkDataArray> nextArray;
	for(int i = 0; i < dataSet->GetPointData()->GetNumberOfArrays(); ++i)
		{
		nextArray = dataSet->GetPointData()->GetArray(i);
		AllocateDataArray(newDataSet,
			nextArray->GetName(),
			nextArray->GetNumberOfComponents(),
			numNewPoints);
		}
	// Copying

	for(int pointLocalId = 0; 
			pointLocalId < pointsInRadius->GetNumberOfIds(); 
			++pointLocalId)
		{
		vtkIdType pointGlobalId = pointsInRadius->GetId(pointLocalId);
		double* dbNextPoint=GetPoint(dataSet,pointGlobalId);
		float* nextPoint=DoublePointToFloat(dbNextPoint);
		vtkIdType newId =SetPointValue(newDataSet,nextPoint);
		// adding this to the newDataSet
		// adding this point's data to the newDataSet, for each data array
			for(int i = 0; i < dataSet->GetPointData()->GetNumberOfArrays(); ++i)
				{
				nextArray = dataSet->GetPointData()->GetArray(i);
				double* nextData = GetDataValue(dataSet,
					nextArray->GetName(),pointGlobalId);
				SetDataValue(newDataSet,nextArray->GetName(),newId,nextData);
				delete [] nextData;
				}
			delete [] nextPoint;
			delete [] dbNextPoint;
			}
	return newDataSet;
}

//----------------------------------------------------------------------------
vtkPolyData* GetDatasetWithinVirialRadius(VirialRadiusInfo virialRadiusInfo)
{

	vtkSmartPointer<vtkIdList> pointsInRadius = \
		vtkSmartPointer<vtkIdList>::New();
	virialRadiusInfo.locator->FindPointsWithinRadius(
//TODO: this is for testing, to capture all points instead
// of just thos in the virial radius
//		virialRadiusInfo.virialRadius,
		1.0,
		virialRadiusInfo.center,
		pointsInRadius);
  vtkPolyData* dataSet = \
		vtkPolyData::SafeDownCast(virialRadiusInfo.locator->GetDataSet());	
	// Creating a new dataset
	// first allocating
	vtkPolyData* newDataSet = \
		CopyPolyPointsAndData(dataSet,pointsInRadius);
	return newDataSet;
}

//----------------------------------------------------------------------------
void ComputeStatisticsInRadialBin(vtkPolyData* inputDataSet, double center[], 
	int binNum, vtkIdList* pointsInBin, vtkTable* output)
{
	double vAve[3],vRadAve[3],vTanAve[3],jAve[3],\
		vSquaredAve[3],vRadSquaredAve[3],vTanSquaredAve[3],\
		vDisp[3],vRadDisp[3],vTanDisp[3];
	double* x_i,v_i,r_i,vRad_i,vTan_i,j_i\
		vSquared_i,vRadSquared_i,vTanSquared_i;// useful for calculating
		// the velocity dispersion of the respective quantities
	// for id in id list
	for(int pointLocalId = 0; 
			pointLocalId < pointsInBin->GetNumberOfIds(); 
			++pointLocalId)
		{
		vtkIdType pointGlobalId = pointsInBin->GetId(pointLocalId);
		x_i=GetPoint(dataSet,pointGlobalId);
		// extracting the mass
		r_i=PointVectorDifference(x_i,center);
		// has to be double as this version of VTK doesn't have 
		v_i=GetDataValue(dataSet,"velocity",pointGlobalId);
		// calculate all the i quantities
		vRad_i=ComputeProjection(v_i,r_i);
		vTan_i=PointVectorDifference(v_i,vRad_i);
		j_i=ComputeAngularMomentum(v_i,r_i);
		vSquared_i=ComputeProjection(v_i,v_i);
		vRadSquared_i=ComputeProjection(vRad_i,vRad_i);
		vTanSquared_i=ComputeProjection(vTan_i,vTan_i);
		// update the total averages
		for(int coord = 0; coord < 3; ++coord)
			{
			vAve[coord]+=v_i[coord];
			vRadAve[coord]+=vRad_i[coord];
			vTanAve[coord]+=vTan_i[coord];
			jAve[coord]+=j_i[coord];
			vSquaredAve[coord]+=vSquared_i[coord];
			vRadSquaredAve[coord]+=vRadSquared_i[coord];
			vTanSquaredAve[coord]+=vTanSquared_i[coord];
			}
		}
	//done with for loop divide everything by N
	VecMultConstant(vAve,1./N);
	VecMultConstant(vRadAve,1./N);
	VecMultConstant(vTanAve,1./N);
	VecMultConstant(jAve,1./N);	
	VecMultConstant(vSquaredAve,1./N);	
	VecMultConstant(vRadSquaredAve,1./N);		
	VecMultConstant(vTanSquaredAve,1./N);		
	// calculate velocity dispersions, taking the necessary square roots	
	CalculateVelocityDispersion(vSquaredAve,vAve,vDisp);
	CalculateVelocityDispersion(vRadSquaredAve,vRadAve,vRadDisp);
	CalculateVelocityDispersion(vTanSquaredAve,vTanAve,vTanDisp);
	// add vAve, vRadAve, vTanAve,, vAveDisp, vRadAveDisp, vTanAveDisp and j
	// to the ouput table.
	
	// finally some memory management
	delete [] x_i;
	delete [] v_i;
	delete [] r_i;
	delete [] vRad_i;
	delete [] vTan_i;
	delete [] j_i;
	delete [] vSquared_i;
	delete [] vRadSquared_i;
	delete [] vTanSquared_i;	
}

//----------------------------------------------------------------------------
double* ComputeProjection(double  vectorOne[],double vectorTwo[])
{
	double normVectorTwo = Norm(vectorTwo);
	double projectionMagnitude = Dot(vectorOne,vectoTwo)/normVectorTwo;
	double* projectionVector = new double[3];
	for(int i = 0; i < 3; ++i)
	{
		projectionVector[i] = projectionMagnitude * vectorTwo[i] /normVectorTwo;
	}
	return projectionVector;
}

//----------------------------------------------------------------------------
double* PointVectorDifference(double vectorOne[], double vectorTwo[])
{
	double* pointVectorDifference = new double[3];
	for(int i = 0; i < 3; ++i)
	{
		pointVectorDifference[i] = vectorOne[i] - vectorTwo[i];
	}
	return pointVectorDifference;
}

//----------------------------------------------------------------------------
void VecMultConstant(double vector[],double constant)
{
	for(int i = 0; i < 3; ++i)
	{
		vector[i] *= constant;
	}
}

//----------------------------------------------------------------------------
double* ComputeAngularMomentum(double v[], double r[])
{
	double* angularMomentum = new double[3];
	Cross(v,r,angularMomentum);
	return angularMomentum;
}

double* CalcVelocityDispersion(double vSquaredAve[], double  vAve[],
	double velocityDipersion)
{
	for(int i = 0; i < 3; ++i)
	{
		velocityDispersion[i] = sqrt(vSquaredAve[i] - pow(vAve[i],2));
	}
}
	
	
	
	
	
	
	
	
	