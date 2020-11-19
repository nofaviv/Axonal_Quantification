#include <vector>
#include <string>
#include <Windows.h>
#include "ReadImageFromIO.h"
#include "ipp.h"
#include "Algorithms.h"
#include <map>
#include <float.h>

#pragma warning( disable : 1079 )

using namespace std;

bool GetImageFileListFromDir(const string& InputDir, const string& FileType, vector<string>& ImageFileNames);

void main(int argc, char *argv[]) {

	// Init IPP
	ippInit();

	// Check number of inputs
	if(argc < 3) {
		printf("Number of inputs is incorrect\n");
		exit(0);
	}
	
	// Set type of algorithm
	bool SaveImages=false;
	bool LinesAlgorithm=true;
	bool CirclesAlgorithm=false;
	bool ThinLinesAlgorithm=false;
	for(unsigned int Cnt1=2;Cnt1<argc;Cnt1++) {
		if(!strcmp("SaveImages",argv[Cnt1])) {
			SaveImages=true;
		}
		else if(!strcmp("Lines",argv[Cnt1])) {
			LinesAlgorithm=true;
		}
		else if(!strcmp("Circles",argv[Cnt1])) {
			CirclesAlgorithm=true;
			LinesAlgorithm=true;
		}
		else if(!strcmp("ThinLines",argv[Cnt1])) {
			ThinLinesAlgorithm=true;
			LinesAlgorithm=true;
		}
		else {
			printf("Unknown input %s\n",argv[Cnt1]);
			exit(0);
		}
	}

	// Print information to screen
	printf("************************************************\n");
	printf("Input library: %s\n",argv[1]);
	printf("Save images: %d\n",SaveImages);
	printf("Lines %d Circles %d ThinLines: %d\n",LinesAlgorithm,CirclesAlgorithm,ThinLinesAlgorithm);
	printf("************************************************\n\n");
	
	// Get image list from dir
	vector<string> ImageFileNames;
	GetImageFileListFromDir(argv[1],"tif",ImageFileNames);
	if(!ImageFileNames.size()) {
		printf("Failed to find *.tif files in %s\n",argv[1]);
		exit(0);
	}

	// Loop on all images and run algorithm
	unsigned int ImageWidth=0,ImageHeight=0;
	int ByteStep=0;
	unsigned char* InputImage=NULL;
	map<unsigned int, map<string, double> > MapResults;
	for(unsigned int Cnt1=0;Cnt1<ImageFileNames.size();Cnt1++) {

		// Load image
		InputImage=ReadImageTIF(ImageFileNames[Cnt1],ImageWidth,ImageHeight,ByteStep);
		if(!InputImage) {
			printf("Failed while reading image %s\n",ImageFileNames[Cnt1].c_str());
			continue;
		}

		// Set saved file name
		string FilePrefix=ImageFileNames[Cnt1].c_str();
		FilePrefix.resize(FilePrefix.size() - 4);
		
		// Set output image
		unsigned char* ResultLineImage=NULL;
		int ResultLineByteStep=0;
		unsigned char* ResultCircleImage = NULL;
		int ResultCircleByteStep = 0;

		// Initialize results
		MapResults[Cnt1]["Circles"] = DBL_MAX;
		MapResults[Cnt1]["Lines"] = DBL_MAX;
		MapResults[Cnt1]["ThinLines"] = DBL_MAX;

		// Calculate lines algorithm
		double LineResult=0.0;
		if(LinesAlgorithm) {
			
			// Run algorithm
			if (!CalculateLines(InputImage, ImageWidth, ImageHeight, ByteStep, LineResult, ResultLineImage, ResultLineByteStep)) {
				printf("Failed while calculating lines over image %s\n",ImageFileNames[Cnt1].c_str());
				if(InputImage) {
					ippiFree(InputImage);
					InputImage=NULL;
				}
				if (ResultLineImage) {
					ippiFree(ResultLineImage);
					ResultLineImage = NULL;
				}
				if (ResultCircleImage) {
					ippiFree(ResultCircleImage);
					ResultCircleImage = NULL;
				}
				continue;
			}

			// Update results
			MapResults[Cnt1]["Lines"] = LineResult;

			// Save images
			if (SaveImages) {
				WritePgmFile<unsigned char>(FilePrefix + "_L.pgm", (const unsigned char*)ResultLineImage, ImageWidth, ImageHeight, ResultLineByteStep);
			}
		}		

		// Calculate dots algorithm
		if(CirclesAlgorithm) {
			
			// Run algorithm
			double CircleResult = 0.0;
			if (!CalculateCircles(InputImage, ImageWidth, ImageHeight, ByteStep, ResultLineImage, ResultLineByteStep, CircleResult, ResultCircleImage, ResultCircleByteStep)) {
				printf("Failed while calculating circles over image %s\n",ImageFileNames[Cnt1].c_str());
				if(InputImage) {
					ippiFree(InputImage);
					InputImage=NULL;
				}
				if (ResultLineImage) {
					ippiFree(ResultLineImage);
					ResultLineImage = NULL;
				}
				if (ResultCircleImage) {
					ippiFree(ResultCircleImage);
					ResultCircleImage = NULL;
				}
				continue;
			}

			// Update results
			MapResults[Cnt1]["Circles"] = CircleResult;

			// Save images
			if (SaveImages) {
				WritePgmFile<unsigned char>(FilePrefix + "_C.pgm", (const unsigned char*)ResultCircleImage, ImageWidth, ImageHeight, ResultCircleByteStep);
			}			
		}

		// Calculate thin lines algorithm
		double ThinLineResult=0.0;
		if(ThinLinesAlgorithm) {
			
			// Run algorithm
			if (!CalculateThinLines(ResultLineImage, ResultLineByteStep, ImageWidth, ImageHeight, ThinLineResult)) {
				printf("Failed while calculating thin lines over image %s\n",ImageFileNames[Cnt1].c_str());
				if(InputImage) {
					ippiFree(InputImage);
					InputImage=NULL;
				}
				if (ResultLineImage) {
					ippiFree(ResultLineImage);
					ResultLineImage = NULL;
				}
				if (ResultCircleImage) {
					ippiFree(ResultCircleImage);
					ResultCircleImage = NULL;
				}
				continue;
			}

			// Update results
			MapResults[Cnt1]["ThinLines"] = ThinLineResult;
			
			// Save images
			if (SaveImages) {
				//WritePgmFile<unsigned char>(FilePrefix + "_TL.pgm", (const unsigned char*)ResultImage, ImageWidth, ImageHeight, ResultByteStep);
			}
		}
		
		// Free memory
		ippiFree(InputImage);
		if (ResultLineImage)
			ippiFree(ResultLineImage);
		if (ResultCircleImage)
			ippiFree(ResultCircleImage);

		printf("Finished processing %u images out of %u\n",Cnt1+1,ImageFileNames.size());
	}

	// Open results file
	FILE* ResultsStream;
	if(fopen_s(&ResultsStream,"MayaResults.csv","wb")) {
		printf("Failed to open file to write results\n");
		exit(0);
	}

	// Write results to file
	fprintf(ResultsStream, "File name,ThinLines,Circles,Lines,\n");
	map<unsigned int, map<string, double> >::const_iterator Itr = MapResults.begin();
	for (; Itr != MapResults.end(); Itr++) {
		fprintf(ResultsStream, "%s,", ImageFileNames[Itr->first].c_str());
		if (Itr->second.at("ThinLines") == DBL_MAX)
			fprintf(ResultsStream, ",");
		else
			fprintf(ResultsStream, "%03.8lf,", Itr->second.at("ThinLines"));
		if (Itr->second.at("Circles") == DBL_MAX)
			fprintf(ResultsStream, ",");
		else
			fprintf(ResultsStream, "%03.8lf,", Itr->second.at("Circles"));
		if (Itr->second.at("Lines") == DBL_MAX)
			fprintf(ResultsStream, ",");
		else
			fprintf(ResultsStream, "%03.8lf,", Itr->second.at("Lines"));
		fprintf(ResultsStream, "\n");
	}

	// Close stream
	fclose(ResultsStream);
}

#pragma warning( pop )

bool GetImageFileListFromDir(const string& InputDir,const string& FileType,vector<string>& ImageFileNames) {

	// Find files in Dir
	HANDLE Handle=NULL;
	WIN32_FIND_DATAA FindFileData;
	bool bStatus=true;
	bool IsType=false;
	bool IsContain=false;
	string FileTypeToSearch=InputDir;
	FileTypeToSearch.append("\\*.*");

	// Create handle to first file or directory
	Handle=FindFirstFileA(FileTypeToSearch.c_str(),&FindFileData);	

	// Loop until all files are found
	while(Handle != INVALID_HANDLE_VALUE && bStatus) {

		if (*(FindFileData.cFileName) != '.') {

			// Check if directory
			if(FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {

				IsType=false;
				IsContain=false;

				// Recursively call to search the next dir
				GetImageFileListFromDir(InputDir + "\\" + FindFileData.cFileName,FileType,ImageFileNames);
			}
			else {
				string FileName=FindFileData.cFileName;
				IsType=(FileName.find(FileType) != string::npos);
				IsType&=(FileName.find("_Comp") == string::npos);
			}

			// Update the name list
			if(IsType == true) {
				ImageFileNames.push_back(InputDir + "\\"+ FindFileData.cFileName);
			}
		}

		// Read next file
		if (!FindNextFileA(Handle,&FindFileData))			
			if (GetLastError() == ERROR_NO_MORE_FILES)	
				bStatus=false;
	}

	return true;
}