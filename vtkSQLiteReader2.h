/*=========================================================================

  Program:   AstroViz plugin for ParaView
  Module:    $RCSfile: vtkSQLiteReader2.h,v $

  Copyright (c) Rafael Kueng
  All rights reserved.
     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkSQLiteReader2 - Read points from a SQLite database
// .SECTION Description
// Reads points from a SQLite DB and displays them on screen


#ifndef __vtkSQLiteReader2_h
#define __vtkSQLiteReader2_h

#include "vtkPolyDataAlgorithm.h" // superclass
#include "sqlitelib/sqlite3.h" // sqlite headerfile

#include "vtkObjectFactory.h"
#include "vtkSmartPointer.h"
#include "vtkPolyData.h"
#include "vtkPoints.h"
#include "vtkCellArray.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include <vtkstd/vector>

class vtkPolyData;
class vtkCharArray;
class vtkIdTypeArray;
class vtkFloatArray;
class vtkPoints;
class vtkCellArray;
class vtkDataArraySelection;


class VTK_EXPORT vtkSQLiteReader2 : public vtkPolyDataAlgorithm
{
public:
	static vtkSQLiteReader2* New();
	vtkTypeRevisionMacro(vtkSQLiteReader2,vtkPolyDataAlgorithm);
	void PrintSelf(ostream& os, vtkIndent indent);

	// Set/Get the name of the file from which to read points.
	vtkSetStringMacro(FileName);
 	vtkGetStringMacro(FileName);

	vtkSetMacro(DisplayColliding,bool);
	vtkGetMacro(DisplayColliding,bool);

	vtkSetMacro(DisplayMerging,bool);
	vtkGetMacro(DisplayMerging,bool);

	vtkSetMacro(DisplayInverted,bool);
	vtkGetMacro(DisplayInverted,bool);

	vtkSetMacro(UpperLimit,double);
	vtkGetMacro(UpperLimit,double);

	vtkSetMacro(LowerLimit,double);
	vtkGetMacro(LowerLimit,double);



//BTX
protected:

	//functions

	vtkSQLiteReader2();
	~vtkSQLiteReader2();

	virtual int FillOutputPortInformation(int vtkNotUsed(port),	vtkInformation* info);

	int RequestInformation(vtkInformation*,	vtkInformationVector**,
		vtkInformationVector*);
	int RequestData(vtkInformation*,vtkInformationVector**,
		vtkInformationVector*);

	/* OUTDATED use vtkIdList instead
	struct Track2{
		int nPoints;
		vtkstd::vector<vtkIdType> PointIds;
	};

	struct Snap2{
		vtkstd::vector<vtkIdType> PointIds;
	};

	vtkstd::vector<Snap2> SnapInfo2;
	vtkstd::vector<Track2> TrackInfo2;
	*/
	vtkstd::vector< vtkSmartPointer<vtkIdList> > SnapInfo2;
	vtkstd::vector< vtkSmartPointer<vtkIdList> > TrackInfo2;


	//structs
	struct SnapshotInfo {
		vtkIdType Offset; //stores the id where this snapshot starts
		int lenght; // stores the amount of halos in this snapshot
		std::vector<int> PointId; //allocates gid to new id (PointId.at(gid) = id+offset) maybee save here some mem, by using smaller datatype..
		

		// dont store data in structs anymore...
		int snapshotNr; // snapshot Nr from simulation (this is not the id!)
		double redshift;
		double time;
		int npart;
		double CvAverage;
	};

	struct Track {
		int nPoints;
		//int StartSnap;
		//int EndSnap;
		std::vector<vtkIdType> PointsIds; //v of length nSnaps, entry of -1 if this track doesnt have a point in this snap
	};

	struct DataInformation {
		bool InitComplete;
		bool dataIsRead;
		int nPoints;
		int nTracks;
		int nSnapshots;
		int gidmax;
		double Omega0;
		double OmegaLambda;
		double Hubble;
		
		vtkstd::vector<int> SnapinfoDataColumns; // witch columns in db contain relevant data?
		vtkstd::vector<int> StatDataColumns; // witch columns in db contain relevant data?
		vtkstd::vector<int> StatCordinateColumns; // witch columns in db contain relevant data?
		//vtkstd::vector<int> StatVelocityColumns; // NOT USED witch columns in db contain relevant data?

		// these save witch column in the according table in db contains the vital ids
		int SnapinfoSnapidColumn;
		int StatSnapidColumn;
		int StatGidColumn;
		int TracksTrackidColumn;
		int TracksSnapidColumn;
		int TracksGidColumn;

		// pointer to important dataarrays
		vtkSmartPointer<vtkIdTypeArray> pGIdArray;
		vtkSmartPointer<vtkIdTypeArray> pTrackIdArray;
		vtkSmartPointer<vtkIdTypeArray> pSnapIdArray;
		int dataArrayOffset;
	};

	struct GuiStruct {
		bool * DisplayColliding;
		bool * DisplayMerging;
		bool * DisplayInverted;

		double * LowerLimit;
		double * UpperLimit;

		/*

		bool * DisplayOnlySelectedData;
		bool * DisplaySelected;
		bool * DisplaySelectedSnapshot;
		int * DisplaySelectedSnapshotNr;
		bool * DisplaySelectedTrack;
		int * DisplaySelectedTrackNr;
		bool * DisplayCalculated;
		double * CalculationImpactParameter;

		bool * calcHighlightCollisionPoints;
		bool * calcHighlightTrack;
		bool * calcHighlightSnapshot;
		bool * calcHighlightPoint;
		int * calcHighlightTrackNr;
		int * calcHighlightSnapshotNr;
		int * calcHighlightPointNr;

		bool * DisplayEstimateTolerance;
		*/
	};

	// very old, dont use this
	struct Data {
		vtkSmartPointer<vtkPoints>			Position;
		vtkSmartPointer<vtkFloatArray>		Velocity;
		vtkSmartPointer<vtkCellArray>		Cells;
		vtkSmartPointer<vtkCellArray>		Tracks;
		vtkSmartPointer<vtkIntArray>		TrackId;
		vtkSmartPointer<vtkIntArray>		GId;
		vtkSmartPointer<vtkIntArray>		SnapId;
		vtkSmartPointer<vtkFloatArray>		Mvir;
		vtkSmartPointer<vtkFloatArray>		Rvir;
		vtkSmartPointer<vtkFloatArray>		Vmax;
		vtkSmartPointer<vtkFloatArray>		Rmax;
		vtkSmartPointer<vtkFloatArray>		Redshift;
		vtkSmartPointer<vtkFloatArray>		Time;
		vtkSmartPointer<vtkFloatArray>		Cv;
		vtkSmartPointer<vtkFloatArray>		CvAverage;
		vtkSmartPointer<vtkFloatArray>		RGc;
		vtkSmartPointer<vtkUnsignedCharArray> CollisionTypePoint;
		vtkSmartPointer<vtkUnsignedCharArray> CollisionTypeTrack;
	};

	struct CalculationSettings {
		// OLD dont use this
		bool calcDone;
		double tolerance;
		int nCollisions;
		int nSelectedPoints; //-1 means all, single points selected to display
		int nSelectedTracks; // -1 means all
		int nAllSelectedPoints; // # of all points (inkl tracks, snaps) to display
		std::vector<int>	selectedPoints; // stores globalids from points to display
		std::vector<int>	selectedTracks; //stores trackid from points to display
	};

	/* old
	struct CollisionResultStruct{
		int nEvents;
		int nPoints;
		std::vector<int> vPointIds;
		int nTracks;
		std::vector<int> vTrackIds;
	};

	struct CollisionCalculationStruct{
		bool isDone;
		double upperTolerance;
		double lowerTolerance;
		CollisionResultStruct Colliding;
		CollisionResultStruct Merging;
	};
	*/

	struct CollisionCalculation{
		bool CalcIsDone;
		double CollisionTolerance;
		double MergingTolerance;
		vtkSmartPointer<vtkIdList> CollisionPointIds;
		vtkSmartPointer<vtkIdList> CollisionTrackIds;
		vtkSmartPointer<vtkIdList> MergingPointIds;
		vtkSmartPointer<vtkIdList> MergingTrackIds;
	};

/*	struct SelectionStruct{
		int nSelectedPoints;
		int nSelectedTracks;
		int nSelectedSnapshots;
		std::vector<int> vPointIds;
		std::vector<int> vTrackIds;
		std::vector<int> vSnapshotIds;
		std::vector<int> vPointIdMap;
		std::vector<int> vPointIdMapReverse; // equal to vPointIds!!
		std::vector<int> vTrackIdMap;

	};*/

	struct SelectionStruct {
		vtkSmartPointer<vtkIdList>	Points;
		vtkSmartPointer<vtkIdList>	Tracks;
	};

	struct ResultOfEsimationOfTolerance {
		int goal;
		double parameter;
		int nCollisions;
	};

	//variables (not used!)
	/*
	vtkSmartPointer<vtkPoints>			Position;
	vtkSmartPointer<vtkFloatArray>		Velocity;
	vtkSmartPointer<vtkCellArray>		Cells;
	vtkSmartPointer<vtkCellArray>		Tracks;
	vtkSmartPointer<vtkIdTypeArray>		TrackId;
	vtkSmartPointer<vtkIdTypeArray>		GId;
	vtkSmartPointer<vtkIdTypeArray>		SnapId;
	vtkSmartPointer<vtkFloatArray>		RVir;
	vtkSmartPointer<vtkUnsignedCharArray> colors;
	*/
	//vtkSmartPointer<vtkUnsignedCharArray> opacity;

	DataInformation dataInfo;
	GuiStruct Gui;
	Data allData, selectedData, emptyData;
	CalculationSettings calcInfo;
	SelectionStruct	selection;
	
	CollisionCalculation collisionCalc;
	//CollisionCalculationStruct collisionCalc;

	//std::vector<ResultOfEsimationOfTolerance> calcEstTol;
	//vtkSmartPointer<vtkFloatArray> calcEstTol2;
	
	//std::vector<SnapshotInfo> SnapInfo;
	//std::vector<Track> TracksInfo;

	//gui variables
	char* FileName;

	bool DisplayColliding;
	bool DisplayMerging;
	bool DisplayInverted;
	double LowerLimit;
	double UpperLimit;
	
	/*
	bool DisplayOnlySelectedData;

	bool DisplaySelected;
		bool DisplaySelectedSnapshot;
		int DisplaySelectedSnapshotNr;
		bool DisplaySelectedTrack;
		int DisplaySelectedTrackNr;

	bool DisplayCalculated;
		double CalculationImpactParameter;

		bool calcHighlightCollisionPoints;
		bool calcHighlightTrack;
		bool calcHighlightSnapshot;
		bool calcHighlightPoint;
		int calcHighlightTrackNr;
		int calcHighlightSnapshotNr;
		int calcHighlightPointNr;


	bool DisplayEstimateTolerance;
	*/


private:
	vtkSQLiteReader2(const vtkSQLiteReader2&);  // Not implemented.
	void operator=(const vtkSQLiteReader2&);  // Not implemented.

	//variables
	sqlite3 * db;

	vtkSmartPointer<vtkPolyData>	AllData;
	vtkSmartPointer<vtkPolyData>	SelectedData;
	vtkSmartPointer<vtkPolyData>	EmptyData;

	vtkSmartPointer<vtkPolyData>	TrackData;
	vtkSmartPointer<vtkPolyData>	SnapshotData;

	//functions
	int ReadHeader(); // reads the database header information

	int readSnapshots(); // reads the snapshots
	int readSnapshotInfo(); 
	int readTracks();
	int calculatePointData();
	//int generateColors();

	int findCollisions(CollisionCalculation*);
	
	//int doCalculations(double,int);
	int calcTolerance();


	int generateSelection(
		CollisionCalculation*, SelectionStruct*);
	int generateSelectedData(
		vtkSmartPointer<vtkPolyData>, SelectionStruct*);
	
	/*int fillIdList(std::vector<int>*, int*,
		std::vector<int>*, int*,
		CollisionResultStruct*, int);
	int generateIdMap();
	int generatePoints(SelectionStruct*, Data*);
	int generateTracks(SelectionStruct*, Data*);
	int reset();
	*/

	// helper
	vtkSmartPointer<vtkIdList> IdListUnion (
		vtkSmartPointer<vtkIdList>, vtkSmartPointer<vtkIdList>); 
	vtkSmartPointer<vtkIdList> IdListUnionUnique (
		vtkSmartPointer<vtkIdList>, vtkSmartPointer<vtkIdList>); 
	vtkSmartPointer<vtkIdList> IdListIntersect (
		vtkSmartPointer<vtkIdList>, vtkSmartPointer<vtkIdList>); 
	vtkSmartPointer<vtkIdList> IdListComplement (
		vtkSmartPointer<vtkIdList>, vtkSmartPointer<vtkIdList>); 

	int IdTypeArray2IdList(vtkIdList* destination, vtkIdTypeArray* source);

	int openDB(char*);
	vtkStdString Int2Str(int);
	double getDistance2(int, int);
	//double getDistanceToO(int);

	vtkSmartPointer<vtkFloatArray> CreateArray(
		vtkDataSet *output, const char* arrayName, int numComponents=1);
	vtkSmartPointer<vtkIntArray> CreateIntArray(
		vtkDataSet *output, const char* arrayName, int numComponents=1);
	vtkSmartPointer<vtkIdTypeArray> CreateIdTypeArray(
		vtkDataSet *output, const char* arrayName, int numComponents=1);

	int InitAllArrays(vtkDataSet *output, unsigned long numTuples);

	struct timings{
		double tot;
		double main;
		double init;
		double read;
		double calc;
		double refresh;
		double generateSelect;
		double executeSelect;
	} timing;

	struct initData{
		int nPoints;
		int nTracks;
		int nSnapshots;
	} initData;


//ETX
};

#endif