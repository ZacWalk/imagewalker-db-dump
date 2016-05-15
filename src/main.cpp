// imagewalker-db-dump.cpp : Defines the entry point for the console application.
//

#include "targetver.h"

#include <iostream>
#include <cstdint>
#include <vector>
#include <fstream>
#include <sstream> 

#include <windows.h>
#include <gdiplus.h>

#include "sqlite3.h"
#include "zlib/zlib.h"

#pragma comment (lib,"Gdiplus")

enum class iw_type : uint32_t
{
	TypeMask = 0xFF000000,
	NodeStart = 0x01000000,
	NodeEnd = 0x02000000,
	String = 0x04000000,
	Integer = 0x08000000,
	Blob = 0x10000000,
	Date = 0x20000000,
	Exit = 0x80000000,
};

enum class iw_propery
{
	Flags = 1,
	TimeDelay,
	BackGround,
	Transparent,
	PageLeft,
	PageTop,
	PageRight,
	PageBottom,
	Format,
	ImageData,
	XPelsPerMeter,
	YPelsPerMeter,
	Statistics,
	LoaderName,
	Errors,
	Warnings,
	OriginalImageX,
	OriginalImageY,
	OriginalBpp,
	Pages,
	MetaData,
	Type,
	SubType,
	Orientation,
	TimeTaken,
	IsoSpeed,
	WhiteBalance,
	ApertureN,
	ApertureD,
	ExposureTimeN,
	ExposureTimeD,
	FocalLengthN,
	FocalLengthD,
	DateTaken,
	ExposureTime35mm,
	Title,
	Tags,
	Description,
	ObjectName,
};

enum class iw_format
{
	PF1,
	PF2,
	PF4,
	PF8,
	PF8Alpha,
	PF8GrayScale,
	PF555,
	PF565,
	PF24,
	PF32,
	PF32Alpha,
	none
};


std::vector<uint8_t> uncompress_data(const uint8_t* src_data, int src_len)
{
	const int temp_buffer_len = 1024 * 200;
	uint8_t buffer[temp_buffer_len];

	std::vector<uint8_t> result;

	z_stream zInfo = { 0 };
	zInfo.total_in = zInfo.avail_in = src_len;
	zInfo.total_out = zInfo.avail_out = temp_buffer_len;
	zInfo.next_in = (uint8_t*)src_data;
	zInfo.next_out = buffer;

	auto err = inflateInit(&zInfo);               // zlib function
	if (err == Z_OK) {
		err = inflate(&zInfo, Z_FINISH);     // zlib function
		if (err == Z_STREAM_END) {
			// -1 or len of output
			result.assign(buffer, buffer + zInfo.total_out);
		}
	}
	inflateEnd(&zInfo);   // zlib function

	return result;
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	UINT  num = 0;          // number of image encoders
	UINT  size = 0;         // size of the image encoder array in bytes

	Gdiplus::GetImageEncodersSize(&num, &size);
	if (size == 0)
		return -1;  // Failure

	auto pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
	if (pImageCodecInfo == NULL)
		return -1;  // Failure

	Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);

	for (UINT j = 0; j < num; ++j)
	{
		if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;  // Success
		}
	}

	free(pImageCodecInfo);
	return -1;  // Failure
}

int main()
{
	const auto db_file = "C:\\Users\\Zac\\AppData\\Local\\ImageWalker\\ImageWalkerDatabase.DB3";

	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	sqlite3 *db = nullptr;
	int rc = sqlite3_open(db_file, &db);	

	if (rc != SQLITE_OK)
	{
		std::cout << "Can't open database: " << std::endl;
		std::cout << sqlite3_errmsg(db) << std::endl;
	}
	else
	{
		sqlite3_stmt *stmt = nullptr;
		auto returnCode = sqlite3_prepare(db, "select fileName, imageData from thumbnail", -1, &stmt, nullptr);

		if (returnCode == SQLITE_OK)
		{
			auto blob_count = 0;

			while (sqlite3_step(stmt) == SQLITE_ROW)
			{ 
				auto name = (const wchar_t *)sqlite3_column_text16(stmt, 0);
				auto blob_data = (const uint8_t*)sqlite3_column_blob(stmt, 1);
				auto blob_len = sqlite3_column_bytes(stmt, 1);

				std::wcout << name << std::endl;

				uint32_t pageLeft = 0;
				uint32_t pageTop = 0;
				uint32_t pageRight = 0;
				uint32_t pageBottom = 0;
				iw_format format = iw_format::none;

				auto data = uncompress_data(blob_data, blob_len);
				auto p = (const uint8_t*)data.data();
				auto p_end = (const uint8_t*)(p + data.size());

				while (p < p_end)
				{
					const auto header = *(const uint32_t*)p;
					const auto property = (iw_propery)(header & ~(uint32_t)iw_type::TypeMask);
					const auto type = (iw_type)(header & (uint32_t)iw_type::TypeMask);

					p += sizeof(uint32_t);

					switch (type)
					{
					case iw_type::NodeStart:
						break;
					case iw_type::NodeEnd:
						break;
					case iw_type::String:
					{
						auto len = *(const uint32_t*)p;
						p += sizeof(uint32_t);
						std::string str((const char *)p, (const char *)p + len);
						p += len;

						std::cout << "   " << str.c_str() << std::endl;
					}
						break;
					case iw_type::Integer:
						if (property == iw_propery::PageLeft) pageLeft = *(const uint32_t*)p;
						if (property == iw_propery::PageTop) pageTop = *(const uint32_t*)p;
						if (property == iw_propery::PageRight) pageRight = *(const uint32_t*)p;
						if (property == iw_propery::PageBottom) pageBottom = *(const uint32_t*)p;
						if (property == iw_propery::Format) format = *(const iw_format*)p;
						p += sizeof(uint32_t);
						break;
					case iw_type::Date:
						p += sizeof(uint32_t);
						p += sizeof(uint32_t);
						break;
					case iw_type::Blob:
					{
						auto len = *(const uint32_t*)p;
						p += sizeof(uint32_t);						

						std::wostringstream os;
						os << blob_count++;
						os << L".jpg";

						auto name = os.str();
						std::cout << "   writing blob " << name.c_str() << " (" << len << " bytes)" << std::endl;


						auto save_format = 0;
						auto bpp = 16;

						if (format == iw_format::PF555)
						{
							save_format = PixelFormat16bppRGB555;
						}
						else if (format == iw_format::PF24) 
						{
							save_format = PixelFormat24bppRGB; bpp = 24;
						}
						else if (format == iw_format::PF32)
						{
							save_format = PixelFormat32bppRGB;
							bpp = 32;
						}
						else if (format == iw_format::PF32Alpha)
						{
							save_format = PixelFormat32bppARGB;
							bpp = 32;
						}
						else
						{
							std::cout << "   unknown format " << std::endl;
						}

						auto width = pageRight - pageLeft;						
						auto bpp_width = width * bpp;
						auto stride = ((bpp_width + ((bpp_width % 8) ? 8 : 0)) / 8 + 3) & ~3;

						Gdiplus::Bitmap bitmap(width, pageBottom - pageTop, stride, save_format, (uint8_t*)p);
						bitmap.RotateFlip(Gdiplus::Rotate180FlipX);

						CLSID   encoderClsid;
						GetEncoderClsid(L"image/png", &encoderClsid);
						bitmap.Save(name.c_str(), &encoderClsid, NULL);					

						

						/*std::fstream f(name, std::ios::out | std::ios::binary);
						f.write((const char *)p, len);
						f.close();*/
						
						p += len;
					}
						break;
					case iw_type::Exit:
						break;
					}
				}
			}

			sqlite3_finalize(stmt);
			stmt = 0;
		}
	}	

	sqlite3_close(db);
	db = nullptr;

	Gdiplus::GdiplusShutdown(gdiplusToken);

    return 0;
}

