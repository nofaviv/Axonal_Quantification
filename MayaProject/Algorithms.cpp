#include "Algorithms.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include "ipp.h"

using namespace std;

#define max(a,b)    (((a) > (b)) ? (a) : (b))
#define min(a,b)    (((a) < (b)) ? (a) : (b))

bool CalculateMeanStd(const unsigned char* Input,unsigned int InputByteStep,
					  const unsigned char* Mask,unsigned int MaskByteStep,
					  unsigned int Width,unsigned int Height,
					  double& Mean,double& Std);

bool CalculateLines(const unsigned char* InputImage,unsigned int Width,unsigned int Height,unsigned int ByteStep,
					double& Result,unsigned char*& ResultImage,int& ResultByteStep) {
	
	IppStatus Status=ippStsNoErr;

	const unsigned int BinSize=64;
	const double MinMeanGL=10.0;
	const double MinStdGL=5.0;

	// Calculate number of bins
	unsigned int NumberOfBinsX=Width/BinSize;
	unsigned int NumberOfBinsY=Height/BinSize;
	if(Width%BinSize)
		NumberOfBinsX++;
	if(Height%BinSize)
		NumberOfBinsY++;

	// Allocate result buffer
	ResultImage=ippiMalloc_8u_C1(Width,Height,&ResultByteStep);
	if(!ResultImage) {
		printf("CalculateLines failed while trying to allocate result image buffer\n");
		return false;
	}
	unsigned char* OtsuThreshold=ippsMalloc_8u(NumberOfBinsX*NumberOfBinsY);
	if(!OtsuThreshold) {
		printf("CalculateLines failed while trying to allocate Otsu threshold buffer\n");
		ippiFree(ResultImage);
		return false;
	}
	double* StdBuffer=ippsMalloc_64f(NumberOfBinsX*NumberOfBinsY);
	if(!StdBuffer) {
		printf("CalculateLines failed while trying to allocate Std buffer\n");
		ippiFree(ResultImage);
		ippsFree(OtsuThreshold);
		return false;
	}
	double* MeanBuffer=ippsMalloc_64f(NumberOfBinsX*NumberOfBinsY);
	if(!MeanBuffer) {
		printf("CalculateLines failed while trying to allocate Std buffer\n");
		ippiFree(ResultImage);
		ippsFree(OtsuThreshold);
		ippsFree(StdBuffer);
		return false;
	}
	unsigned char* OtsuBuffer=ippsMalloc_8u(3*BinSize*3*BinSize);
	if(!OtsuBuffer) {
		printf("CalculateLines failed while trying to allocate Otsu temporary buffer\n");
		ippiFree(ResultImage);
		ippsFree(OtsuThreshold);
		ippsFree(StdBuffer);
		ippsFree(MeanBuffer);
		return false;
	}
	
	// Set Results to zero
	memset(ResultImage,0,Width*Height);

	// Loop on all bins
	IppiSize Roi={(int)Width,(int)Height};
	for(unsigned int Cnt1=0;Cnt1<NumberOfBinsY;Cnt1++) {
		for(unsigned int Cnt2=0;Cnt2<NumberOfBinsX;Cnt2++) {
			
			// Calculate indices to image
			unsigned int StartIndexX=min(Width-1,Cnt2*BinSize);
			unsigned int StartIndexY=min(Height-1,Cnt1*BinSize);
			Roi.width = min(Width, (Cnt2 + 1)*BinSize) - StartIndexX;
			Roi.height = min(Height, (Cnt1 + 1)*BinSize) - StartIndexY;

			// Calculate bin std and mean
			double Mean=0.0,Std=0.0;
			unsigned char MinGL=UCHAR_MAX;
			Status=ippiMean_StdDev_8u_C1R(InputImage+StartIndexY*ByteStep+StartIndexX,ByteStep,Roi,&Mean,&Std);
			Status=ippiMin_8u_C1R(InputImage+StartIndexY*ByteStep+StartIndexX,ByteStep,Roi,&MinGL);
			while((Mean < (MinGL+MinMeanGL))&&(Std < MinStdGL)) {
				if((Roi.width >= Width) && (Roi.height >= Height)) {
					break;
				}
				StartIndexX=max(0,(int)StartIndexX-64);
				StartIndexY=max(0,(int)StartIndexY-64);
				Roi.width=min(Width,StartIndexX+Roi.width+128)-StartIndexX;
				Roi.height=min(Height,StartIndexY+Roi.height+128)-StartIndexY;
				Status=ippiMean_StdDev_8u_C1R(InputImage+StartIndexY*ByteStep+StartIndexX,ByteStep,Roi,&Mean,&Std);
			}
			
			// Calculate image threshold
			Status = ippiComputeThreshold_Otsu_8u_C1R(InputImage + StartIndexY*ByteStep + StartIndexX, ByteStep, Roi, OtsuThreshold + Cnt1*NumberOfBinsX + Cnt2);

			// Reset ROI and indices in case they were changed
			StartIndexX=min(Width-1,Cnt2*BinSize);
			StartIndexY=min(Height-1,Cnt1*BinSize);
			Roi.width=min(Width,(Cnt2+1)*BinSize)-StartIndexX;
			Roi.height=min(Height,(Cnt1+1)*BinSize)-StartIndexY;

			// Perform upper threshold
			Status=ippiThreshold_GTVal_8u_C1R(InputImage+StartIndexY*ByteStep+StartIndexX,ByteStep,
											  ResultImage+StartIndexY*ResultByteStep+StartIndexX,ResultByteStep,
											  Roi,OtsuThreshold[Cnt1*NumberOfBinsX+Cnt2]-1,255);
			
			// Calculate Std of pixels below threshold
			CalculateMeanStd(InputImage+StartIndexY*ByteStep+StartIndexX,ByteStep,
							 ResultImage+StartIndexY*ResultByteStep+StartIndexX,ResultByteStep,
							 Roi.width,Roi.height,
							 MeanBuffer[Cnt1*NumberOfBinsX+Cnt2],StdBuffer[Cnt1*NumberOfBinsX+Cnt2]);
		}
	}

//	WritePgmFile<unsigned char>("D:\\Maya\\TestA.pgm",InputImage,Width,Height,ByteStep);
//	WritePgmFile<unsigned char>("D:\\Maya\\TestB.pgm",ResultImage,Width,Height,ResultByteStep);

	// Find minimum and mean of std
//	double MinStd=0.0,MeanStd=0.0;
//	Status=ippsMin_64f(StdBuffer,NumberOfBinsX*NumberOfBinsY,&MinStd);
//	Status=ippsMean_64f(StdBuffer,NumberOfBinsX*NumberOfBinsY,&MeanStd);
//	MeanStd=max(MinStdGL,min(1.2*MinStd,MeanStd));
	
	// Second iteration loop
	for(unsigned int Cnt1=0;Cnt1<NumberOfBinsY;Cnt1++) {
		for(unsigned int Cnt2=0;Cnt2<NumberOfBinsX;Cnt2++) {

			// Calculate indices to image
			unsigned int StartIndexX=min(Width-1,Cnt2*BinSize);
			unsigned int StartIndexY=min(Height-1,Cnt1*BinSize);
			Roi.width=min(Width,(Cnt2+1)*BinSize)-StartIndexX;
			Roi.height=min(Height,(Cnt1+1)*BinSize)-StartIndexY;

			// Calculate bin std and mean
			if((MeanBuffer[Cnt1*NumberOfBinsX+Cnt2] < MinMeanGL)&&(StdBuffer[Cnt1*NumberOfBinsX+Cnt2] >= MinStdGL)) {
				StartIndexX=max(0,(int)StartIndexX-64);
				StartIndexY=max(0,(int)StartIndexY-64);
				Roi.width=min(Width,StartIndexX+Roi.width+128)-StartIndexX;
				Roi.height=min(Height,StartIndexY+Roi.height+128)-StartIndexY;
				CalculateMeanStd(InputImage+StartIndexY*ByteStep+StartIndexX,ByteStep,
								 ResultImage+StartIndexY*ResultByteStep+StartIndexX,ResultByteStep,
								 Roi.width,Roi.height,
								 MeanBuffer[Cnt1*NumberOfBinsX+Cnt2],StdBuffer[Cnt1*NumberOfBinsX+Cnt2]);
			}
			else if(StdBuffer[Cnt1*NumberOfBinsX+Cnt2] < MinStdGL) {
				Status=ippiThreshold_LTVal_8u_C1R(InputImage+StartIndexY*ByteStep+StartIndexX,ByteStep,
												  ResultImage+StartIndexY*ResultByteStep+StartIndexX,ResultByteStep,
												  Roi,OtsuThreshold[Cnt1*NumberOfBinsX+Cnt2],0);
				Status=ippiThreshold_GTVal_8u_C1IR(ResultImage+StartIndexY*ResultByteStep+StartIndexX,ResultByteStep,
												   Roi,OtsuThreshold[Cnt1*NumberOfBinsX+Cnt2]-1,255);
				continue;
			}
		
			// Get pixels that didn't pass previous thresholding operation
			const unsigned char* Pixel=0;
			unsigned int NumberOfPixels=0;
			for(unsigned int Cnt3=0;Cnt3<(unsigned int)Roi.height;Cnt3++) {
				Pixel=InputImage+(StartIndexY+Cnt3)*ByteStep+StartIndexX;
				for(unsigned int Cnt4=0;Cnt4<(unsigned int)Roi.width;Cnt4++) {
					if(Pixel[Cnt4] < OtsuThreshold[Cnt1*NumberOfBinsX+Cnt2]) {
						OtsuBuffer[NumberOfPixels]=Pixel[Cnt4];
						NumberOfPixels++;
					}					
				}
			}

			// Calculate image threshold
			Roi.width=NumberOfPixels;
			Roi.height=1;
			Status=ippiComputeThreshold_Otsu_8u_C1R(OtsuBuffer,NumberOfPixels,Roi,OtsuThreshold+Cnt1*NumberOfBinsX+Cnt2);
			
			// Reset ROI and indices in case they were changed
			StartIndexX=min(Width-1,Cnt2*BinSize);
			StartIndexY=min(Height-1,Cnt1*BinSize);
			Roi.width=min(Width,(Cnt2+1)*BinSize)-StartIndexX;
			Roi.height=min(Height,(Cnt1+1)*BinSize)-StartIndexY;
			
			// Perform threshold
			Roi.width=min(Width,(Cnt2+1)*BinSize)-StartIndexX;
			Roi.height=min(Height,(Cnt1+1)*BinSize)-StartIndexY;
			Status=ippiThreshold_LTVal_8u_C1R(InputImage+StartIndexY*ByteStep+StartIndexX,ByteStep,
											  ResultImage+StartIndexY*ResultByteStep+StartIndexX,ResultByteStep,
											  Roi,OtsuThreshold[Cnt1*NumberOfBinsX+Cnt2],0);
			Status=ippiThreshold_GTVal_8u_C1IR(ResultImage+StartIndexY*ResultByteStep+StartIndexX,ResultByteStep,
											   Roi,OtsuThreshold[Cnt1*NumberOfBinsX+Cnt2]-1,255);
		}
	}

//	WritePgmFile<unsigned char>("D:\\Maya\\TestC.pgm",ResultImage,Width,Height,ResultByteStep);
	
/*
		// Set lines in image
		for(unsigned int Cnt1=0;Cnt1<Height;Cnt1++) {
			if(!(Cnt1%BinSize))
				memset(ResultImage+Cnt1*ResultByteStep,255,Width);
			for(unsigned int Cnt2=0;Cnt2<Width;Cnt2++) {
				if(!(Cnt2%BinSize))
					ResultImage[Cnt1*ResultByteStep+Cnt2]=255;
			}
		}
*/

	// Set ROI
	Roi.width=Width;
	Roi.height=Height;

	// Calculate result
	Status=ippiSum_8u_C1R(ResultImage,ResultByteStep,Roi,&Result);
	Result*=(100.0/255.0/(double)Width/(double)Height);	

//	WritePgmFile<unsigned char>("D:\\Maya\\TestD.pgm",ResultImage,Width,Height,ResultByteStep);

	// Free memory
	ippsFree(OtsuThreshold);
	ippsFree(StdBuffer);
	ippsFree(MeanBuffer);
	ippsFree(OtsuBuffer);

	return true;
}

bool CalculateMeanStd(const unsigned char* Input,unsigned int InputByteStep,
					  const unsigned char* Mask,unsigned int MaskByteStep,
					  unsigned int Width,unsigned int Height,
					  double& Mean,double& Std) {

	double SumX=0.0,SumX2=0.0,SumN=0.0;
	const unsigned char* InputlLine=NULL;
	const unsigned char* MaskLine=NULL;
	for(unsigned int Cnt1=0;Cnt1<Height;Cnt1++) {
		MaskLine=Mask+Cnt1*MaskByteStep;
		InputlLine=Input+Cnt1*InputByteStep;
	  for(unsigned int Cnt2=0;Cnt2<Width;Cnt2++) {
		  if(MaskLine[Cnt2] == 255)
			  continue;
		  SumX+=(double)InputlLine[Cnt2];
		  SumX2+=((double)InputlLine[Cnt2]*(double)InputlLine[Cnt2]);
		  SumN+=1.0;
	  }
	}
	Mean=SumX/SumN;
	Std=sqrt(SumX2/SumN-Mean*Mean);

	return true;
}

bool CalculateCircles(const unsigned char* InputImage, unsigned int Width, unsigned int Height, unsigned int InputImageByteStep,
					  const unsigned char* MaskImage, unsigned int MaskImageByteStep, double& Result,
					  unsigned char*& ResultImage, int& ResultByteStep) {

	IppStatus Status=ippStsNoErr;

	// Allocate result buffer
	ResultImage = ippiMalloc_8u_C1(Width, Height, &ResultByteStep);
	if (!ResultImage) {
		printf("CalculateCircles failed while trying to allocate result image buffer\n");
		return false;
	}
	memset(ResultImage, 0, ResultByteStep * Height);

	unsigned int NumberOfCircles = 0;
	unsigned char Threshold = 240;
	for(unsigned int Cnt1 = 0; Cnt1 < Height; ++Cnt1) {
		const unsigned char* ImageLine=InputImage + Cnt1 * InputImageByteStep;
		const unsigned char* MaskLine=MaskImage + Cnt1 * InputImageByteStep;
		unsigned char* ResultLine = ResultImage + Cnt1 * ResultByteStep;
		for(unsigned int Cnt2 = 0; Cnt2 < Width; ++Cnt2) {
			if (MaskLine[Cnt2] && (ImageLine[Cnt2] >= Threshold)) {
				NumberOfCircles++;
				ResultLine[Cnt2] = UCHAR_MAX;
			}
		}
	}

	// Calculate number of lines
	IppiSize Roi = {(int)Width,(int)Height};
	Status = ippiSum_8u_C1R(MaskImage,MaskImageByteStep,Roi,&Result);
	Result /= 255.0;
	Result = (double)NumberOfCircles / (Result-(double)NumberOfCircles);

	return true;
}

bool CalculateThinLines(unsigned char* InputImage,unsigned int InputImageByteStep,unsigned int InputImageWidth,unsigned int InputImageHeight,double& Result) {

	IppStatus Status=ippStsNoErr;

	// Check inputs
	if(!(InputImage && InputImageByteStep && InputImageWidth && InputImageHeight)) {
		printf("Inputs to thin lines algorithms are incorrect\n");
		return false;
	}

	// Create mask
	const int R=8;
	unsigned char Mask[(2*R+1)*(2*R+1)];
	memset(Mask,0,(2*R+1)*(2*R+1));
	for(int Cnt1=-R;Cnt1<=R;Cnt1++) {
		for(int Cnt2=-R;Cnt2<=R;Cnt2++) {
			double Radius=sqrt(pow((double)Cnt1,2)+pow((double)Cnt2,2));
			if(Radius <= (double)R) {
				Mask[(Cnt1+R)*(2*R+1)+Cnt2+R]=1;
			}
		}
	}

	// Init the morphology state
	IppiSize MaskSize={2*R+1,2*R+1};
	IppiPoint Anchor={R,R};
	IppiMorphState* MorphState=NULL;
	Status=ippiMorphologyInitAlloc_8u_C1R(InputImageWidth,Mask,MaskSize,Anchor,&MorphState);

	// Allocate result image
	int MorphResultByteStep=0;
	unsigned char* MorphResultBuffer=ippiMalloc_8u_C1(InputImageWidth,2*InputImageHeight,&MorphResultByteStep);
	if(!MorphResultBuffer) {
		printf("Failed to allocate memory for morphological image\n");
		ippiMorphologyFree(MorphState);
	}
	unsigned char* MorphResult1=MorphResultBuffer;
	unsigned char* MorphResult2=MorphResultBuffer+InputImageHeight*MorphResultByteStep;

//	WritePgmFile<unsigned char>("D:\\Maya\\TestA.pgm",InputImage,InputImageWidth,InputImageHeight,InputImageByteStep);

	// Apply erosion
	IppiSize Roi={(int)InputImageWidth,(int)InputImageHeight};
	Status=ippiErodeBorderReplicate_8u_C1R(InputImage,InputImageByteStep,MorphResult1,MorphResultByteStep,Roi,ippBorderRepl,MorphState);
	
//	WritePgmFile<unsigned char>("D:\\Maya\\TestErosion.pgm",MorphResult1,InputImageWidth,InputImageHeight,MorphResultByteStep);

	// Apply dilation
	Status=ippiDilateBorderReplicate_8u_C1R(MorphResult1,MorphResultByteStep,MorphResult2,MorphResultByteStep,Roi,ippBorderRepl,MorphState);

//	WritePgmFile<unsigned char>("D:\\Maya\\TestDilation.pgm",MorphResult2,InputImageWidth,InputImageHeight,MorphResultByteStep);

	// Apply not
	Status=ippiNot_8u_C1IR(MorphResult2,MorphResultByteStep,Roi);

//	WritePgmFile<unsigned char>("D:\\Maya\\TestNot.pgm",MorphResult2,InputImageWidth,InputImageHeight,MorphResultByteStep);

	// Apply and
	Status=ippiAnd_8u_C1IR(MorphResult2,MorphResultByteStep,InputImage,InputImageByteStep,Roi);

//	WritePgmFile<unsigned char>("D:\\Maya\\TestAnd.pgm",InputImage,InputImageWidth,InputImageHeight,InputImageByteStep);

	// Free memory
	ippiMorphologyFree(MorphState);
	ippiFree(MorphResultBuffer);

	// Calculate result
	Status=ippiSum_8u_C1R(InputImage,InputImageByteStep,Roi,&Result);
	Result*=(100.0/255.0/(double)InputImageWidth/(double)InputImageHeight);

	return true;
}