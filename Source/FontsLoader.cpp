//Utility code for converting font files to bitmap using windows gdi api
//or True Type library from Sean Barrett

//Switch to 1 to use Windows API or 0 to truetype library
#define USE_FONT_FROM_WINDOWS 0

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
  if(!DeviceContext)
  {
    AddFontResourceExA(FileName, FR_PRIVATE, 0);
    int Height = 128;
    HFONT Font = CreateFontA(Height, 0, 0, 0, FW_DONTCARE,
			     FALSE, FALSE, FALSE, DEFAULT_CHARSET,
			     OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
			     DEFAULT_PITCH|FW_DONTCARE, FontName);

    DeviceContext = CreateCompatibleDC(0);
    HBITMAP Bitmap = CreateCompatibleBitmap(DeviceContext, 1024, 1024);
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
  int Height = Size.cy;

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

  for(int Y = 0; Y < Height; ++Y)
  {
    for(int X = 0; X < Width; ++X)
    {
      COLORREF Pixel = GetPixel(DeviceContext, X, Y);
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
    }
  }
  //-----------------------------------------------------------------
  if(MinX <= MaxX)
  {
    Width = (MaxX - MinX) + 1;
    Height = (MaxY - MinY) + 1;

    //Allocating the bitmap memory and copying the glyph in it
    Result.Pitch = Width*BITMAP_BYTES_PER_PIXEL;
    Result.Width = Width;
    Result.Height = Height;
    Result.Memory = malloc(Result.Height*Result.Pitch);

    uint8 *DestRow = (uint8 *)Result.Memory + (Height - 1)*Result.Pitch;
  
    for(int Y = MinY; Y < MaxY; ++Y)
    {
      uint32 *Dest = (uint32 *)DestRow;
      for(int X = MinX; X < MaxX; ++X)
      {
	COLORREF Pixel = GetPixel(DeviceContext, X, Y);
        uint8 Alpha = uint8(Pixel & 0xFF);
	*Dest++ = ((Alpha << 24)|
		   (Alpha << 16)|
		   (Alpha << 8)|
		   (Alpha << 0));
      }
      DestRow -= Result.Pitch; 
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
