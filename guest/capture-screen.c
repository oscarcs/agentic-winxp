#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static int write_bmp(const char *path, HBITMAP bitmap, int width, int height)
{
    BITMAPFILEHEADER file_header;
    BITMAPINFOHEADER info_header;
    DWORD row_bytes;
    DWORD image_bytes;
    char *pixels;
    FILE *f;
    HDC dc;
    BITMAPINFO info;
    int ok;

    row_bytes = ((width * 3 + 3) / 4) * 4;
    image_bytes = row_bytes * height;
    pixels = (char *)malloc(image_bytes);
    if (!pixels) {
        return 1;
    }

    ZeroMemory(&info_header, sizeof(info_header));
    info_header.biSize = sizeof(info_header);
    info_header.biWidth = width;
    info_header.biHeight = height;
    info_header.biPlanes = 1;
    info_header.biBitCount = 24;
    info_header.biCompression = BI_RGB;
    info_header.biSizeImage = image_bytes;

    ZeroMemory(&info, sizeof(info));
    info.bmiHeader = info_header;

    dc = GetDC(NULL);
    ok = GetDIBits(dc, bitmap, 0, height, pixels, &info, DIB_RGB_COLORS);
    ReleaseDC(NULL, dc);
    if (!ok) {
        free(pixels);
        return 1;
    }

    ZeroMemory(&file_header, sizeof(file_header));
    file_header.bfType = 0x4d42;
    file_header.bfOffBits = sizeof(file_header) + sizeof(info_header);
    file_header.bfSize = file_header.bfOffBits + image_bytes;

    f = fopen(path, "wb");
    if (!f) {
        free(pixels);
        return 1;
    }
    fwrite(&file_header, sizeof(file_header), 1, f);
    fwrite(&info_header, sizeof(info_header), 1, f);
    fwrite(pixels, image_bytes, 1, f);
    fclose(f);
    free(pixels);
    return 0;
}

int main(int argc, char **argv)
{
    const char *path = "xp-screen.bmp";
    int width;
    int height;
    HDC screen_dc;
    HDC mem_dc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    int result;

    if (argc >= 2) {
        path = argv[1];
    }

    width = GetSystemMetrics(SM_CXSCREEN);
    height = GetSystemMetrics(SM_CYSCREEN);
    screen_dc = GetDC(NULL);
    mem_dc = CreateCompatibleDC(screen_dc);
    bitmap = CreateCompatibleBitmap(screen_dc, width, height);
    old_bitmap = (HBITMAP)SelectObject(mem_dc, bitmap);

    BitBlt(mem_dc, 0, 0, width, height, screen_dc, 0, 0, SRCCOPY);

    SelectObject(mem_dc, old_bitmap);
    DeleteDC(mem_dc);
    ReleaseDC(NULL, screen_dc);

    result = write_bmp(path, bitmap, width, height);
    DeleteObject(bitmap);

    if (result != 0) {
        printf("capture failed\n");
        return 1;
    }
    printf("captured %dx%d to %s\n", width, height, path);
    return 0;
}
