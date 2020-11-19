bool CalculateLines(const unsigned char* Image,unsigned int Width,unsigned int Height,unsigned int ByteStep,double& Result,unsigned char*& ResultImage,int& ResultByteStep);
bool CalculateCircles(const unsigned char* InputImage,unsigned int InputImageWidth,unsigned int InputImageHeight,unsigned int InputImageByteStep,
					  const unsigned char* MaskImage,unsigned int MaskImageByteStep,double& Result,
					  unsigned char*& ResultImage, int& ResultByteStep);
bool CalculateThinLines(unsigned char* InputImage,unsigned int InputImageByteStep,unsigned int InputImageWidth,unsigned int InputImageHeight,double& Result);