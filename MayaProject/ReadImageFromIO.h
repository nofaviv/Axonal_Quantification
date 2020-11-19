#include <string>

unsigned char* ReadImageTIF(const std::string& InputFileName, unsigned int& ImageWidth, unsigned int& ImageHeight, int& ImageByteStep);
template <class T> bool WritePgmFile(const std::string& FileName,const T* Image,unsigned int Width,unsigned int Height,unsigned int ByteStep);