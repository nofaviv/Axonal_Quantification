#include "ReadImageFromIO.h"

#include "tiffio.h"
#include "ipp.h"
#include <math.h>

using namespace std;

//#include "stdfilein.h"
//#include "uic_tiff_dec.h"


template bool WritePgmFile<unsigned char>(const string&,const unsigned char*,unsigned int,unsigned int,unsigned int);
template bool WritePgmFile<unsigned short>(const string&,const unsigned short*,unsigned int,unsigned int,unsigned int);
template bool WritePgmFile<unsigned int>(const string&,const unsigned int*,unsigned int,unsigned int,unsigned int);
template bool WritePgmFile<float>(const string&,const float*,unsigned int,unsigned int,unsigned int);
template bool WritePgmFile<double>(const string&,const double*,unsigned int,unsigned int,unsigned int);

unsigned char* ReadImageTIF(const string& InputFileName, unsigned int& ImageWidth, unsigned int& ImageHeight, int& ImageByteStep) {

	//Initialize output variables
	ImageWidth = 0;
	ImageHeight = 0;
	ImageByteStep = 0;

	// Set warning handler
	TIFFSetWarningHandler(NULL);

	// Initialize tiff image pointer
	TIFF* InputImage = TIFFOpen(InputFileName.c_str(), "r");
	if (!InputImage) {
		printf("ReadImageTIF failed to open file %s\n", InputFileName.c_str());
		return NULL;
	}

	// Get tiff image parameters
	unsigned short NumberOfBitsPerChannel = 0, NumberOfChannels = 0;
	unsigned int TileWidth = 0, TileHeight = 0;
	TIFFGetField(InputImage, TIFFTAG_BITSPERSAMPLE, &NumberOfBitsPerChannel);
	TIFFGetField(InputImage, TIFFTAG_SAMPLESPERPIXEL, &NumberOfChannels);
	TIFFGetField(InputImage, TIFFTAG_IMAGEWIDTH, &ImageWidth);
	TIFFGetField(InputImage, TIFFTAG_IMAGELENGTH, &ImageHeight);
	const unsigned int StripSize = TIFFStripSize(InputImage);
	const unsigned int NumberOfStrips = TIFFNumberOfStrips(InputImage);
	TIFFGetField(InputImage, TIFFTAG_TILEWIDTH, &TileWidth);
	TIFFGetField(InputImage, TIFFTAG_TILELENGTH, &TileHeight);

	// Check validity of header parameters
	if ((ImageWidth == 0) || (ImageHeight == 0) || (NumberOfChannels == 0) || (NumberOfBitsPerChannel == 0) || (StripSize == 0) || (NumberOfStrips == 0)) {
		printf("ReadImageTIF found invalid parameters in image %s header\n", InputFileName.c_str());
		TIFFClose(InputImage);
		return NULL;
	}

	// Calculate number of rows per strip
	const unsigned int NumberOfRowsPerStrip = StripSize / ImageWidth / NumberOfChannels / (NumberOfBitsPerChannel / 8);

	// Allocate strip buffer
	unsigned char* StripBuffer = ippsMalloc_8u(StripSize);
	if (!StripBuffer) {
		printf("ReadImageTIF failed to allocate strip buffer of size %u [Bytes]", StripSize);
		TIFFClose(InputImage);
		return NULL;
	}

	// Allocate output image
	unsigned char* OutputImage = ippiMalloc_8u_C1(ImageWidth, ImageHeight, &ImageByteStep);
	if (!OutputImage) {
		printf("ReadImageTIF failed to allocate output image buffer of size %u [Bytes]", ImageHeight*ImageByteStep);
		TIFFClose(InputImage);
		ippsFree(StripBuffer);
		return NULL;
	}

	// Read and copy data
	for (unsigned int Cnt1 = 0, BufferOffset = 0; Cnt1 < NumberOfStrips; ++Cnt1){
		int NumberOfBytesRead = TIFFReadEncodedStrip(InputImage, Cnt1, StripBuffer, StripSize);
		if (NumberOfBytesRead == -1) {
			printf("ReadImageTIF failed to read strip %u from input image", Cnt1);
			TIFFClose(InputImage);
			ippsFree(StripBuffer);
			ippiFree(OutputImage);
			return NULL;
		}
		unsigned char* ImageRow = OutputImage + Cnt1 * NumberOfRowsPerStrip * ImageByteStep;
		if (NumberOfChannels == 1) {
			if (NumberOfBitsPerChannel == 8) {
				memcpy(ImageRow, StripBuffer, ImageWidth*sizeof(unsigned char));
			}
			else if (NumberOfBitsPerChannel == 16) {
				for (unsigned int Cnt2 = 0; Cnt2 < (StripSize / (NumberOfBitsPerChannel / 8)); ++Cnt2) {
					ImageRow[Cnt2] = (unsigned char)(((unsigned short*)StripBuffer)[Cnt2] >> 8);
				}
			}
		}
		else if (NumberOfChannels == 3) {			
			for (unsigned int Cnt2 = 0, Cnt3 = Cnt2 * NumberOfChannels; Cnt2 < StripSize; Cnt2 += NumberOfChannels, ++Cnt3) {
				ImageRow[Cnt3] = StripBuffer[Cnt2 + 0];
			}
		}
	}

	// Free buffers
	TIFFClose(InputImage);
	ippsFree(StripBuffer);

	//WritePgmFile<unsigned char>("C:\\Temp\\Aviv.pgm", OutputImage, ImageWidth, ImageHeight, ImageByteStep);

	// Return output image
	return OutputImage;
}
/*
unsigned char* ReadImageTIF(const string& InputFileName,unsigned int& Width,unsigned int& Height,int& ByteStep) {

	UIC::ExcStatus Status=ExcStatusOk;

	// Open file for reading
	CStdFileInput FileInputStream;
	if(!UIC::BaseStream::IsOk(FileInputStream.Open(InputFileName.c_str()))) {
		printf("ReadImageTIF failed to open file %s\n",InputFileName.c_str());
		return NULL;
	}
	
	// Init decoder
	UIC::TIFFDecoder ImageDecoder;
	Status=ImageDecoder.Init();
	if(Status != ExcStatusOk) {
		printf("ReadImageTIF failed to init decoder for image %s\n",InputFileName.c_str());
		FileInputStream.Close();
		return NULL;
	}	
	
	// Attach decoder to stream
	Status=ImageDecoder.AttachStream(FileInputStream);
	if(Status != ExcStatusOk) {
		printf("ReadImageTIF failed to attach decoder to stream for image %s\n",InputFileName.c_str());
		FileInputStream.Close();
		return NULL;
	}	

	// Read header
	UIC::ImageColorSpec ColorSpace;
	UIC::ImageSamplingGeometry Geometry;
	Status=ImageDecoder.ReadHeader(ColorSpace,Geometry);
	if(Status != ExcStatusOk) {
		printf("ReadImageTIF failed to ReadHeader for image %s\n",InputFileName.c_str());
		FileInputStream.Close();	
		return NULL;
	}

	// Get data from header
	unsigned int NumberOfChannels=Geometry.NOfComponents();
	const UIC::ImageDataRange* DataRange=ColorSpace.DataRange();
	unsigned int NumberOfBitsPerChannel=DataRange->BitDepth()+1;
	bool IsDataSigned=DataRange->IsSigned();
	UIC::ImageDataType DataType=DataRange->DataType();
	const UIC::ImageEnumColorSpace EnumColorSpace=ColorSpace.EnumColorSpace();
	Height=Geometry.RefGridRect().Height();
	Width=Geometry.RefGridRect().Width();

	// Check validity of header parameters
	if(!(Width && Height && NumberOfChannels && (NumberOfBitsPerChannel == 8))) {
		printf("ReadImageTIF found invalid parameters in image %s header\n",InputFileName.c_str());
		FileInputStream.Close();	
		return NULL;
	}

	// Allocate image
	UIC::ImageDataPtr MyImage;
	if(NumberOfChannels == 1)
		MyImage.p8u=ippiMalloc_8u_C1(Width,Height,&ByteStep);
	else if(NumberOfChannels == 2) {
		MyImage.p16u=ippiMalloc_16u_C1(Width,Height,&ByteStep);
		NumberOfChannels=1;
		DataType=T16u;
		NumberOfBitsPerChannel=8*NOfBytes(DataType);
	}
	else if(NumberOfChannels == 3) {
		NumberOfBitsPerChannel=NumberOfChannels*NOfBytes(DataType)*8;
		MyImage.p8u=ippiMalloc_8u_C1(NumberOfChannels*Width,Height,&ByteStep);
	}
	else {
		printf("ReadImageTIF found unsupported number of channels %u for image %s\n",NumberOfChannels,InputFileName.c_str());
		FileInputStream.Close();	
		return NULL;
	}
	if(!MyImage.p8u) {
		printf("ReadImageTIF failed to allocate image buffer for image %s\n",InputFileName.c_str());
		FileInputStream.Close();	
		return NULL;
	}
	
	// Set data order for output image
	UIC::ImageDataOrder DataOrder;
	DataOrder.ReAlloc(UIC::Interleaved,NumberOfChannels);
	DataOrder.SetDataType(DataType);
	*(DataOrder.PixelStep())=NumberOfBitsPerChannel/8;
	*(DataOrder.LineStep())=ByteStep;

	// Define image control
	UIC::Image ImageCn;
	ImageCn.Buffer().Attach(&MyImage,DataOrder,Geometry);
	ImageCn.ColorSpec().ReAlloc(NumberOfChannels);
	ImageCn.ColorSpec().SetColorSpecMethod(UIC::Enumerated);
	ImageCn.ColorSpec().SetComponentToColorMap(UIC::Direct);
	ImageCn.ColorSpec().SetEnumColorSpace(EnumColorSpace);
	for(unsigned int Cnt1=0;Cnt1<NumberOfChannels;Cnt1++) {

		// Define pointer to data range
		UIC::ImageDataRange* OutputDataRange=ImageCn.ColorSpec().DataRange()+Cnt1;

		// Set data range of each channel
		OutputDataRange->SetAsRange8u(UCHAR_MAX);
	}

	// Read image
	Status=ImageDecoder.ReadData(ImageCn.Buffer().DataPtr(),DataOrder);
	if(Status != ExcStatusOk) {
		printf("ReadImageTIF failed to read buffer data for image %s\n",InputFileName.c_str());
		FileInputStream.Close();
		delete[] MyImage.p8u;
		return NULL;
	}

	// Close the stream
	FileInputStream.Close();

	// Detach buffer
	ImageCn.Buffer().FreeOrDetach();
	
	if(DataType == T16u) {
		for(unsigned int Cnt1=0;Cnt1<Height;Cnt1++) {
			unsigned char* Pointer8u=MyImage.p8u+Cnt1*ByteStep/sizeof(unsigned char);
			unsigned short* Pointer16u=MyImage.p16u+Cnt1*ByteStep/sizeof(unsigned short);
			for(unsigned int Cnt2=0;Cnt2<Width;Cnt2++)
				Pointer8u[Cnt2]=(unsigned char)(Pointer16u[Cnt2]);
		}
	}
	if(NumberOfChannels == 3) {
		for(unsigned int Cnt1=0;Cnt1<Height;Cnt1++) {
			unsigned char* Pointer8u=MyImage.p8u+Cnt1*ByteStep/sizeof(unsigned char);
			for(unsigned int Cnt2=0;Cnt2<Width;Cnt2++) {
//				Pointer8u[Cnt2]=Pointer8u[Cnt2*NumberOfChannels];
				
				Pointer8u[Cnt2]=Pointer8u[Cnt2*NumberOfChannels+1];
				//double Gamma=3.0;
				//Pointer8u[Cnt2]=(unsigned char)floor(pow(255.0,1.0-Gamma)*pow((double)Pointer8u[Cnt2],Gamma)+0.5);
//				Pointer8u[Cnt2]=(unsigned char)(0.299*(double)Pointer8u[Cnt2*NumberOfChannels]+
//												0.587*(double)Pointer8u[Cnt2*NumberOfChannels+1]+
//												0.114*(double)Pointer8u[Cnt2*NumberOfChannels+2]);
			}
		}
	}

//	WritePgmFile<unsigned char>("D:\\Maya\\Test.pgm",MyImage.p8u,Width,Height,ByteStep);

	// Return image atom
	return MyImage.p8u;
}

*/

// Read Quantum PGM data
template <class T> bool WritePgmFile(const string& FileName,const T* Image,unsigned int Width,unsigned int Height,unsigned int ByteStep) {

	// Open file for reading
	FILE* PgmFileStream=NULL;
	if(fopen_s(&PgmFileStream,FileName.c_str(),"wb")) {
		printf("WritePgmFile failed to open file %s\n",FileName.c_str());
		return false;
	}

	// Write header
	if(sizeof(T) == 1)
		fprintf(PgmFileStream,"P5\n%u %u\n%u\n",Width,Height,UCHAR_MAX);
	else if(sizeof(T) == 4) {
		fprintf(PgmFileStream,"P9\n%u %u\n%u\n",Width,Height,UINT_MAX);	}
	else {
		printf("WritePgmFile received unknown image format\n");
		fclose(PgmFileStream);
		return false;
	}

	// Write image data
	for(unsigned int Cnt1=0;Cnt1<Height;Cnt1++) {
		if(fwrite(Image+Cnt1*ByteStep/sizeof(T),sizeof(T),Width,PgmFileStream) != Width) {
			printf("WritePgmFile failed write image data to file %s\n",FileName.c_str());
			fclose(PgmFileStream);
			return false;
		}
	}
	

	// Close file stream
	fclose(PgmFileStream);

	return true;
}