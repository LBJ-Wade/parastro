/*=========================================================================
Modified from vtkSimplePointsReader and from Doug Potter's Tipsylib, 
this depends on a few header files as well as the Tipsylib library.

Only reads in standard format Tipsy files.
@author corbett
=========================================================================*/
#include <math.h>
#include <assert.h>
#include "vtkTipsyReader.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"
#include "vtkPoints.h"
#include "vtkCellArray.h"
#include "vtkFloatArray.h" 
#include "vtkIntArray.h"
#include "astrovizhelpers/DataSetHelpers.h"

vtkCxxRevisionMacro(vtkTipsyReader, "$Revision: 1.0 $");
vtkStandardNewMacro(vtkTipsyReader);

//----------------------------------------------------------------------------
vtkTipsyReader::vtkTipsyReader()
{
  this->MarkFileName = 0; // this file is optional
	this->AttributeFile = 0; // this file is also optional
  this->FileName = 1;
	this->ReadPositionsOnly = 0;
  this->SetNumberOfInputPorts(0); 
}

//----------------------------------------------------------------------------
vtkTipsyReader::~vtkTipsyReader()
{
  this->SetFileName(0);
  this->SetAttributeFile(0);
  this->SetMarkFileName(0);
	this->SetReadPositionsOnly(0);
}

//----------------------------------------------------------------------------
void vtkTipsyReader::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
  os << indent << "FileName: "
     << (this->FileName ? this->FileName : "(none)") << "\n"
		 << indent << "MarkFileName: "
     << (this->MarkFileName ? this->MarkFileName : "(none)") << "\n";
}

//----------------------------------------------------------------------------
TipsyHeader vtkTipsyReader::ReadTipsyHeader(ifTipsy& tipsyInfile)
{
	// Initializing for reading  
	TipsyHeader       h; 
	// Reading in the header
  tipsyInfile >> h;
	return h;
}

//----------------------------------------------------------------------------
queue<int> vtkTipsyReader::ReadMarkedParticleIndices(\
														TipsyHeader& tipsyHeader,ifTipsy& tipsyInfile)
{
	ifstream markInFile(this->MarkFileName);
	queue<int> markedParticleIndices;
	if(!markInFile)
 		{
 		vtkErrorMacro("Error opening marked particle file: " 
									<< this->MarkFileName 
									<< " please specify a valid mark file or none at all.\
									 For now reading all particles.");
 		}
	else
		{
		int mfIndex,mfBodies,mfGas,mfStar,mfDark,numBodies;
		// first line of the mark file is of a different format:
		// intNumBodies intNumGas intNumStars
		if(markInFile >> mfBodies >> mfGas >> mfStar)
			{
	 		mfDark=mfBodies-mfGas-mfStar;
			if(mfBodies!=tipsyHeader.h_nBodies || mfDark!=tipsyHeader.h_nDark \
				|| mfGas!=tipsyHeader.h_nSph || mfStar!=tipsyHeader.h_nStar)
	 			{
	 			vtkErrorMacro("Error opening marked particle file, wrong format,\
	 										number of particles do not match Tipsy file: " 
											<< this->MarkFileName 
											<< " please specify a valid mark file or none at all.\
											For now reading all particles.");
	 			}
			else
				{
				// The rest of the file is is format: intMarkIndex\n
				// Read in the rest of the file file, noting the marked particles
				while(markInFile >> mfIndex)
					{
					// Insert the next marked particle index into the queue
					// subtracting one as the indices in marked particle file
					// begin at 1, not 0
					markedParticleIndices.push(mfIndex-1);
					}
				// closing file
				markInFile.close();
				// now the number of bodies is equal to the number of marked particles
				tipsyHeader.h_nBodies=markedParticleIndices.size();
				// read file successfully
				vtkDebugMacro("Read " << numBodies<< " marked point indices.");
				}	
	 		}
		}
		//empty if none were read, otherwise size gives number of marked particles
		return markedParticleIndices;
}

//----------------------------------------------------------------------------
void vtkTipsyReader::ReadAllParticles(TipsyHeader& tipsyHeader,\
											ifTipsy& tipsyInfile,vtkPolyData* output)
{
	// Allocates vtk scalars and vector arrays to hold particle data, 
	this->AllocateAllTipsyVariableArrays(tipsyHeader,output);
	for(int i=0; i<tipsyHeader.h_nBodies; i++)  // we could have many bodies
  	{ 
			this->ReadParticle(tipsyInfile,output);
  	}
}

//----------------------------------------------------------------------------
void vtkTipsyReader::ReadMarkedParticles(\
											queue<int> markedParticleIndices,\
											TipsyHeader& tipsyHeader,\
											ifTipsy& tipsyInfile,\
											vtkPolyData* output)
{
	// Allocates vtk scalars and vector arrays to hold particle data, 
	// As marked file was read, only allocates numBodies which 
	// now equals the number of marked particles
	this->AllocateAllTipsyVariableArrays(tipsyHeader,output);
	int nextMarkedParticleIndex;
  while(!markedParticleIndices.empty())
		{
			nextMarkedParticleIndex=markedParticleIndices.front();
			markedParticleIndices.pop();
			// navigating to the next marked particle
			if(nextMarkedParticleIndex < tipsyHeader.h_nSph)
				{
				// we are seeking a gas particle
				tipsyInfile.seekg(tipsypos(tipsypos::gas,nextMarkedParticleIndex));	
				}
			else if(nextMarkedParticleIndex <\
			 				tipsyHeader.h_nDark+tipsyHeader.h_nSph)
				{
				// we are seeking a dark particle
				tipsyInfile.seekg(tipsypos(tipsypos::dark,nextMarkedParticleIndex));	
				}
			else if (nextMarkedParticleIndex < tipsyHeader.h_nBodies)
				{
				// we are seeking a star particle
				tipsyInfile.seekg(tipsypos(tipsypos::star,nextMarkedParticleIndex));	
				}
			else
				{
				vtkErrorMacro("A marked particle index is greater than \
											the number of particles in the file, unable to read")	
				break;
				}
			// reading in the particle
			ReadParticle(tipsyInfile,output);
		}
}

//----------------------------------------------------------------------------
vtkIdType vtkTipsyReader::ReadParticle(ifTipsy& tipsyInfile,\
														vtkPolyData* output) 
{
  // allocating variables for reading
  vtkIdType id;
  TipsyGasParticle  g;
  TipsyDarkParticle d;
  TipsyStarParticle s;
  switch(tipsyInfile.tellg().section()) 
		{
   case tipsypos::gas:
     tipsyInfile >> g;
		 id=this->ReadGasParticle(output,g);
     break;
   case tipsypos::dark:
     tipsyInfile >> d;
		 id=this->ReadDarkParticle(output,d);
     break;
   case tipsypos::star:
     tipsyInfile >> s;
		 id=this->ReadStarParticle(output,s);
     break;
   default:
     assert(0);
     break;
    }
  return id;
}


//----------------------------------------------------------------------------
vtkIdType vtkTipsyReader::ReadBaseParticle(vtkPolyData* output,\
														TipsyBaseParticle& b) 
{
	vtkIdType id = SetPointValue(output,b.pos);
  if(!this->ReadPositionsOnly)
		{
		SetDataValue(output,"velocity",id,b.vel);
	  SetDataValue(output,"mass",id,&b.mass);
		SetDataValue(output,"potential",id,&b.phi);
		}
	return id;
}

//----------------------------------------------------------------------------
vtkIdType vtkTipsyReader::ReadGasParticle(vtkPolyData* output,\
 																					TipsyGasParticle& g)
{
 	vtkIdType id=ReadBaseParticle(output,g);
  if(!this->ReadPositionsOnly)
		{
	  SetDataValue(output,"rho",id,&g.rho);
	  SetDataValue(output,"temperature",id,&g.temp);
	  SetDataValue(output,"hsmooth",id,&g.hsmooth);
	  SetDataValue(output,"metals",id,&g.metals);	
		}
	return id;
}

//----------------------------------------------------------------------------
vtkIdType vtkTipsyReader::ReadStarParticle(vtkPolyData* output,\
 																					TipsyStarParticle& s)
{
	vtkIdType id=ReadBaseParticle(output,s);
	if(!this->ReadPositionsOnly)
		{
	  SetDataValue(output,"eps",id,&s.eps);
	  SetDataValue(output,"metals",id,&s.metals);
	  SetDataValue(output,"tform",id,&s.tform);
		}
	return id;
}
//----------------------------------------------------------------------------	
vtkIdType vtkTipsyReader::ReadDarkParticle(vtkPolyData* output,\
 																					TipsyDarkParticle& d)
{
 	vtkIdType id=this->ReadBaseParticle(output,d);
  if(!this->ReadPositionsOnly)
		{
	 	SetDataValue(output,"eps",id,&d.eps);
		}
	return id;
}
		
//----------------------------------------------------------------------------
void vtkTipsyReader::AllocateAllTipsyVariableArrays(TipsyHeader& tipsyHeader,\
											vtkPolyData* output)
{
  // Allocate objects to hold points and vertex cells. 
	// Storing the points and cells in the output data object.
  output->SetPoints(vtkSmartPointer<vtkPoints>::New());
  output->SetVerts(vtkSmartPointer<vtkCellArray>::New()); 
	// Only allocate the other arrays if we are to read this data in
  if(!this->ReadPositionsOnly)
		{
		// the default scalars to be displayed
		// Only allocate if we are to read thes in
	  AllocateDataArray(output,"potential",1,tipsyHeader.h_nBodies);
		// the rest of the scalars
	  AllocateDataArray(output,"mass",1,tipsyHeader.h_nBodies);
	  AllocateDataArray(output,"eps",1,tipsyHeader.h_nBodies);
	  AllocateDataArray(output,"rho",1,tipsyHeader.h_nBodies);
	  AllocateDataArray(output,"hsmooth",1,tipsyHeader.h_nBodies);
	  AllocateDataArray(output,"temperature",1,tipsyHeader.h_nBodies);
	  AllocateDataArray(output,"metals",1,tipsyHeader.h_nBodies);
	  AllocateDataArray(output,"tform",1,tipsyHeader.h_nBodies);
		// the default vectors to be displayed
	  AllocateDataArray(output,"velocity",3,tipsyHeader.h_nBodies);
		}
}
/*
* Reads a file, optionally only the marked particles from the file, 
* in the following order:
* 1. Open Tipsy binary
* 2. Read Tipsy header (tells us the number of particles of each type we are 
*    dealing with)
* 3. Read mark file indices from marked particle file, if there is one
* 4. Read either marked particles only or all particles
*/
//----------------------------------------------------------------------------
int vtkTipsyReader::RequestData(vtkInformation*,
                                       vtkInformationVector**,
                                       vtkInformationVector* outputVector)
{
	// Make sure we have a file to read.
  if(!this->FileName)
	  {
    vtkErrorMacro("A FileName must be specified.");
    return 0;
    }
	// Open the tipsy standard file and abort if there is an error.
	ifTipsy tipsyInfile;
  tipsyInfile.open(this->FileName,"standard");
  if (!tipsyInfile.is_open()) 
		{
	  vtkErrorMacro("Error opening file " << this->FileName);
	  return 0;	
    }
	//All helper functions will need access to this
	vtkPolyData* output = vtkPolyData::GetData(outputVector);
  // Read the header from the input
	TipsyHeader tipsyHeader=this->ReadTipsyHeader(tipsyInfile);
	// Next considering whether to read in a mark file, 
	// and if so whether that reading was a success 
	queue<int> markedParticleIndices;
	if(strcmp(this->MarkFileName,"")!=0)
		{
		vtkDebugMacro("Reading marked point indices from file:" \
									<< this->MarkFileName);
		markedParticleIndices=this->ReadMarkedParticleIndices(tipsyHeader,
			tipsyInfile);
		}
  // Read every particle and add their position to be displayed, 
	// as well as relevant scalars
	if(markedParticleIndices.empty())
		{
		// no marked particle file or there was an error reading the mark file, 
		// so reading all particles
		vtkDebugMacro("Reading all points from file " << this->FileName);
		this->ReadAllParticles(tipsyHeader,tipsyInfile,output);
		}
	else 
		{
		//reading only marked particles
		vtkDebugMacro("Reading only the marked points in file: " \
									<< this->MarkFileName << " from file " << this->FileName);
		this->ReadMarkedParticles(markedParticleIndices,\
															tipsyHeader,tipsyInfile,output);	
		}
  // Close the tipsy in file.
	tipsyInfile.close();
	// Read Successfully
	vtkDebugMacro("Read " << output->GetPoints()->GetNumberOfPoints() \
								<< " points.");
 	return 1;
}
