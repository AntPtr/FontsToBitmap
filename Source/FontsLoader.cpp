//Utility code for converting font files to bitmap using windows gdi api
//or True Type library from Sean Barrett

//Switch to 1 to use Windows API or 0 to truetype library
#if COMPILER_MSVC
#include <intrin.h>
#pragma intrinsic(_BitScanForward)
#endif

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef size_t memory_index;

typedef int32 bool32;

typedef float real32;
typedef double real64;


#define USE_FONT_FROM_WINDOWS 1

#if USE_FONT_FROM_WINDOWS
#include <windows.h>
#else
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#endif

#include <memory.h>

//Bitmap simple struct
struct loaded_bitmap
{
  int32 Width;
  int32 Height;
  void *Memory;
  int32 Pitch;
};


//Pass font name only when you want to use Windows api version
internal loaded_bitmap LoadGlyphBitmap(char *FileName, uint32 CodePoint, char *FontName = 0)
{
  loaded_bitmap Result = {};
 
#if USE_FONT_FROM_WINDOWS
  static HDC DeviceContext = 0;
  static VOID* Bits = 0;
  int MaxWidth = 1024;
  int MaxHeight = 1024;

  if(!DeviceContext)
  {
    AddFontResourceExA(FileName, FR_PRIVATE, 0);
    int Height = 128;
    HFONT Font = CreateFontA(Height, 0, 0, 0, FW_DONTCARE,
			     FALSE, FALSE, FALSE, DEFAULT_CHARSET,
			     OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
			     DEFAULT_PITCH|FW_DONTCARE, FontName);

    DeviceContext = CreateCompatibleDC(0);
    
    BITMAPINFO Info = {};

    Info.bmiHeader.biSize = sizeof(Info.bmiHeader);
    Info.bmiHeader.biWidth = MaxWidth;
    Info.bmiHeader.biHeight = MaxHeight;
    Info.bmiHeader.biPlanes = 1;
    Info.bmiHeader.biBitCount = 32;
    Info.bmiHeader.biCompression = BI_RGB;
    Info.bmiHeader.biSizeImage = 0;
    Info.bmiHeader.biXPelsPerMeter = 0;
    Info.bmiHeader.biYPelsPerMeter = 0;
    Info.bmiHeader.biClrUsed = 0;
    Info.bmiHeader.biClrImportant = 0;
    HBITMAP Bitmap = CreateDIBSection(DeviceContext, &Info, DIB_RGB_COLORS, &Bits, 0, 0);

    SelectObject(DeviceContext, Bitmap);
    SelectObject(DeviceContext, Font);
    SetBkColor(DeviceContext, RGB(0, 0 ,0));

    TEXTMETRIC TextMetric;
    GetTextMetrics(DeviceContext, &TextMetric);
  }

  wchar_t CheesePoint = (wchar_t)CodePoint;
  SIZE Size;
  GetTextExtentPoint32W(DeviceContext, &CheesePoint, 1, &Size); 

  int Width = Size.cx;
  if (Width > MaxWidth)
  {
      Width = MaxWidth;
  }

  int Height = Size.cy;
  if (Height > MaxHeight)
  {
      Width = MaxHeight;
  }

  //Setting the background color to full black and the glyph to white
  SetBkMode(DeviceContext, OPAQUE);
  SetBkColor(DeviceContext, RGB(0, 0, 0));
  SetTextColor(DeviceContext, RGB(255, 255, 255));
  PatBlt(DeviceContext, 0, 0, 1024, 1024, BLACKNESS);
  TextOutW(DeviceContext, 0, 0, &CheesePoint, 1);

  //This code is for finding and cutting empty padding around the glyph
  int MinX = 10000;
  int MinY = 10000;
  int MaxX = -10000;
  int MaxY = -10000;

  uint32* Row = (uint32*)Bits + MaxWidth * (MaxHeight - 1);
  for(int Y = 0; Y < Height; ++Y)
  {
    for(int X = 0; X < Width; ++X)
    {
      uint32 *Pixel = Row;
      if(Pixel != 0)
      {
	    if(MinX > X)
	    {
	        MinX = X;
	    }           
      
	    if(MinY > Y)
        {
	        MinY = Y;
	    }

	    if(MaxX < X)
        {
	        MaxX = X;
	    }

	    if(MaxY < Y)
        {
	         MaxY = Y;
	    }
      }
      ++Pixel;
    }
    Row -= MaxWidth;
  }
  //-----------------------------------------------------------------
  if(MinX <= MaxX)
  {
    Width = (MaxX - MinX) + 1;
    Height = (MaxY - MinY) + 1;

    //Allocating the bitmap memory and copying the glyph in it
    Result.Pitch = Width * BITMAP_BYTES_PER_PIXEL;
    Result.Width = Width;
    Result.Height = Height;
    Result.Memory = malloc(Result.Height * Result.Pitch);

    uint8 *DestRow = (uint8*)Result.Memory + (Result.Height - 1) * Result.Pitch;
    uint32 *SourceRow = (uint32*)Bits + MaxWidth * (MaxHeight - 1 - MinY);
    
    for(int Y = MinY; Y < MaxY; ++Y)
    {
      uint32 *Dest = (uint32 *)DestRow;
      uint32* Source = (uint32*)SourceRow + MinX;
      
      for(int X = MinX; X < MaxX; ++X)
      {
	    uint8 Pixel = *Source;
        uint8 Alpha = 0;
        Alpha = uint8(Pixel & 0xFF);
	    *Dest++ = ((Alpha << 24)|
		            (Alpha << 16)|
		            (Alpha << 8)|
		            (Alpha << 0));
      }
      DestRow -= Result.Pitch; 
      SourceRow -= MaxWidth;
    }     
  }

  //True type version
#else
  entire_file TTFFile = ReadEntireFile(FileName);
     
  stbtt_fontinfo Font;
  stbtt_InitFont(&Font, (uint8 *)(TTFFile.Contents), stbtt_GetFontOffsetForIndex((uint8 *)(TTFFile.Contents), 0));

  int Width, Height, XOffset, YOffset;
  uint8 *MonoBitmap = stbtt_GetCodepointBitmap(&Font, 0,stbtt_ScaleForPixelHeight(&Font, 120.0f), CodePoint, &Width, &Height, &XOffset, &YOffset);

  Result.Pitch = Width*BITMAP_BYTES_PER_PIXEL;
  Result.Width = Width;
  Result.Height = Height;
  Result.Memory = malloc(Result.Height*Result.Pitch);
  
  uint8 *Source = MonoBitmap;
  uint8 *DestRow = (uint8 *)Result.Memory + (Height - 1)*Result.Pitch;
  
  for(int Y = 0; Y < Height; ++Y)
  {
    uint32 *Dest = (uint32 *)DestRow;
    for(int X = 0; X < Width; ++X)
    {
      uint8 Alpha = *Source++;
      *Dest++ = ((Alpha << 24)|
		 (Alpha << 16)|
		 (Alpha << 8)|
		 (Alpha << 0));
    }
    DestRow -= Result.Pitch; 
  }
  stbtt_FreeBitmap(MonoBitmap, 0);
  free(TTFFile.Contents);
#endif  
  return Result;    
}
