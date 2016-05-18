// imagewalker-db-dump.cpp : Defines the entry point for the console application.
//

#include "targetver.h"

#include <iostream>
#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <algorithm>

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


CLSID find_encoder(const WCHAR* format)
{
	UINT num = 0;          // number of image encoders
	UINT size = 0;         // size of the image encoder array in bytes
	auto result = GUID_NULL;

	Gdiplus::GetImageEncodersSize(&num, &size);

	if (size > 0)
	{
		auto pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));

		if (pImageCodecInfo)
		{
			Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);

			for (UINT j = 0; j < num; ++j)
			{
				if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
				{
					result = pImageCodecInfo[j].Clsid;
					break;
				}
			}

			free(pImageCodecInfo);
		}
	}
	return result;
}

std::wstring save_name_from_path(const wchar_t *name)
{
	std::wstring result = name;
	std::replace(result.begin(), result.end(), '\\', '_');
	std::replace(result.begin(), result.end(), '/', '_');
	std::replace(result.begin(), result.end(), '.', '_');
	std::replace(result.begin(), result.end(), ':', '_');
	std::replace(result.begin(), result.end(), ' ', '_');
	return result + L".png";
}

const char *prop_to_text(iw_propery prop)
{
	static std::map<iw_propery, const char *> mapping = {
		{ iw_propery::Flags, "Flags" },
		{ iw_propery::TimeDelay, "TimeDelay" },
		{ iw_propery::BackGround, "BackGround" },
		{ iw_propery::Transparent, "Transparent" },
		{ iw_propery::PageLeft, "PageLeft" },
		{ iw_propery::PageTop, "PageTop" },
		{ iw_propery::PageRight, "PageRight" },
		{ iw_propery::PageBottom, "PageBottom" },
		{ iw_propery::Format, "Format" },
		{ iw_propery::ImageData, "ImageData" },
		{ iw_propery::XPelsPerMeter, "XPelsPerMeter" },
		{ iw_propery::YPelsPerMeter, "YPelsPerMeter" },
		{ iw_propery::Statistics, "Statistics" },
		{ iw_propery::LoaderName, "LoaderName" },
		{ iw_propery::Errors, "Errors" },
		{ iw_propery::Warnings, "Warnings" },
		{ iw_propery::OriginalImageX, "OriginalImageX" },
		{ iw_propery::OriginalImageY, "OriginalImageY" },
		{ iw_propery::OriginalBpp, "OriginalBpp" },
		{ iw_propery::Pages, "Pages" },
		{ iw_propery::MetaData, "MetaData" },
		{ iw_propery::Type, "Type" },
		{ iw_propery::SubType, "SubType" },
		{ iw_propery::Orientation, "Orientation" },
		{ iw_propery::TimeTaken, "TimeTaken" },
		{ iw_propery::IsoSpeed, "IsoSpeed" },
		{ iw_propery::WhiteBalance, "WhiteBalance" },
		{ iw_propery::ApertureN, "ApertureN" },
		{ iw_propery::ApertureD, "ApertureD" },
		{ iw_propery::ExposureTimeN, "ExposureTimeN" },
		{ iw_propery::ExposureTimeD, "ExposureTimeD" },
		{ iw_propery::FocalLengthN, "FocalLengthN" },
		{ iw_propery::FocalLengthD, "FocalLengthD" },
		{ iw_propery::DateTaken, "DateTaken" },
		{ iw_propery::ExposureTime35mm, "ExposureTime35mm" },
		{ iw_propery::Title, "Title" },
		{ iw_propery::Tags, "Tags" },
		{ iw_propery::Description, "Description" },
		{ iw_propery::ObjectName, "ObjectName" } };

	auto found = mapping.find(prop);
	return found != mapping.end() ? found->second : "Unknown property";
}

void save_thumbnail(const std::wstring &file_name, const uint32_t width, const uint32_t height, const iw_format format, const uint8_t *pixels)
{
	if (width == 0 || height == 0)
	{
		std::cout << "empty thumbnail" << std::endl;
	}	

	auto save_format = 0;
	auto bpp = 16;

	if (format == iw_format::PF555)
	{
		save_format = PixelFormat16bppRGB555;
	}
	else if (format == iw_format::PF24)
	{
		save_format = PixelFormat24bppRGB;
		bpp = 24;
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
		std::cout << "unknown format " << std::endl;
		return;
	}

	std::wcout << L"writing thumbnail " << file_name.c_str() << std::endl;

	auto bpp_width = width * bpp;
	auto stride = ((bpp_width + ((bpp_width % 8) ? 8 : 0)) / 8 + 3) & ~3;

	Gdiplus::Bitmap bitmap(width, height, stride, save_format, (uint8_t*)pixels);
	bitmap.RotateFlip(Gdiplus::Rotate180FlipX);

	auto encoder_id = find_encoder(L"image/png");
	bitmap.Save(file_name.c_str(), &encoder_id, NULL);
}

void extract_blob(const wchar_t *name, const uint8_t *blob_data, const int blob_len)
{
	std::wcout << "Name: " << name << std::endl;

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
			std::string val((const char *)p, (const char *)p + len);
			p += len;

			std::cout << prop_to_text(property) << ": " << val.c_str() << std::endl;
		}
		break;

		case iw_type::Integer:
		{
			auto val = *(const uint32_t*)p;
			std::cout << prop_to_text(property) << ": " << val << std::endl;
			if (property == iw_propery::PageLeft) pageLeft = val;
			if (property == iw_propery::PageTop) pageTop = val;
			if (property == iw_propery::PageRight) pageRight = val;
			if (property == iw_propery::PageBottom) pageBottom = val;
			if (property == iw_propery::Format) format = (iw_format)val;
			p += sizeof(uint32_t);
		}
			break;

		case iw_type::Date:
			p += sizeof(uint32_t);
			p += sizeof(uint32_t);
			break;

		case iw_type::Blob:
		{
			auto len = *(const uint32_t*)p;
			p += sizeof(uint32_t);
			std::cout << prop_to_text(property) << ": ";
			save_thumbnail(save_name_from_path(name), pageRight - pageLeft, pageBottom - pageTop, format, p);
			p += len;
		}
		break;
		case iw_type::Exit:
			break;
		}
	}

	std::wcout << std::endl;
}

int main(int argc, char **argv)
{
	std::cout << "ImageWalker database dump." << std::endl;

	if (argc != 2)
	{
		std::cout << "Usage:  imagewalker-db-dump <database file name>" << std::endl;
		std::cout << "Cached metadata will be output to the console. Thumbnails will be written to the current working folder." << std::endl;
	}
	else
	{
		const auto db_file = argv[1]; // "C:\\Users\\Zac\\AppData\\Local\\ImageWalker\\ImageWalkerDatabase.DB3";
		std::cout << "Loading database: " << db_file << std::endl;

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
			auto blob_count = 0;

			sqlite3_stmt *stmt = nullptr;
			auto returnCode = sqlite3_prepare(db, "select fileName, imageData from thumbnail", -1, &stmt, nullptr);

			if (returnCode == SQLITE_OK)
			{
				while (sqlite3_step(stmt) == SQLITE_ROW)
				{
					auto name = (const wchar_t *)sqlite3_column_text16(stmt, 0);
					auto blob_data = (const uint8_t*)sqlite3_column_blob(stmt, 1);
					auto blob_len = sqlite3_column_bytes(stmt, 1);

					extract_blob(name, blob_data, blob_len);
					blob_count += 1;
				}

				sqlite3_finalize(stmt);
				stmt = 0;

				std::cout << std::endl  << "Completed extracting " << blob_count << " blobs" << std::endl;
			}
		}

		sqlite3_close(db);
		db = nullptr;

		Gdiplus::GdiplusShutdown(gdiplusToken);
	}

	return 0;
}

