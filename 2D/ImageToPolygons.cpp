#include "ImageToPolygons.hpp"

#include <vector>
#include <string>
#include <fstream>
#include <queue>
#include <cstdint>
#include <algorithm>
#include <cstring>
#include <png.h>
#include <jpeglib.h>
#include <setjmp.h>

namespace HsBa::Slicer
{
    namespace
    {
        static std::string ToLower(const std::string &s)
        {
            std::string out = s;
            for (auto &c : out) c = char(std::tolower((unsigned char)c));
            return out;
        }

        bool LoadBMPGray(const std::string &path, std::vector<uint8_t> &out, int &w, int &h)
        {
            std::ifstream ifs(path, std::ios::binary);
            if (!ifs) return false;
            uint16_t bfType;
            ifs.read(reinterpret_cast<char*>(&bfType), sizeof(bfType));
            if (bfType != 0x4D42) return false;

            ifs.seekg(8, std::ios::cur);
            uint32_t bfOffBits;
            ifs.read(reinterpret_cast<char*>(&bfOffBits), sizeof(bfOffBits));

            uint32_t biSize;
            ifs.read(reinterpret_cast<char*>(&biSize), sizeof(biSize));
            if (biSize < 40) return false;

            int32_t biWidth, biHeight;
            ifs.read(reinterpret_cast<char*>(&biWidth), sizeof(biWidth));
            ifs.read(reinterpret_cast<char*>(&biHeight), sizeof(biHeight));
            w = biWidth; h = std::abs(biHeight);

            uint16_t biPlanes, biBitCount;
            ifs.read(reinterpret_cast<char*>(&biPlanes), sizeof(biPlanes));
            ifs.read(reinterpret_cast<char*>(&biBitCount), sizeof(biBitCount));

            uint32_t biCompression;
            ifs.read(reinterpret_cast<char*>(&biCompression), sizeof(biCompression));
            if (biCompression != 0) return false; // only uncompressed

            if (biBitCount != 24 && biBitCount != 8) return false;

            // skip rest of DIB header to pixel data offset
            ifs.seekg(bfOffBits, std::ios::beg);

            out.assign(w * h, 0);
            if (biBitCount == 24)
            {
                int rowBytes = ((w * 3 + 3) / 4) * 4;
                std::vector<uint8_t> row(rowBytes);
                // BMP stores bottom-up
                for (int y = 0; y < h; ++y)
                {
                    int rowIdx = h - 1 - y;
                    ifs.read(reinterpret_cast<char*>(row.data()), rowBytes);
                    for (int x = 0; x < w; ++x)
                    {
                        uint8_t b = row[x*3 + 0];
                        uint8_t g = row[x*3 + 1];
                        uint8_t r = row[x*3 + 2];
                        uint8_t gray = static_cast<uint8_t>((int(r) + int(g) + int(b)) / 3);
                        out[rowIdx * w + x] = gray;
                    }
                }
            }
            else // 8-bit
            {
                // assume palette present: skip palette (256 * 4 bytes)
                std::vector<uint8_t> palette(256 * 4);
                ifs.read(reinterpret_cast<char*>(palette.data()), palette.size());
                int rowBytes = ((w + 3) / 4) * 4;
                std::vector<uint8_t> row(rowBytes);
                for (int y = 0; y < h; ++y)
                {
                    int rowIdx = h - 1 - y;
                    ifs.read(reinterpret_cast<char*>(row.data()), rowBytes);
                    for (int x = 0; x < w; ++x)
                    {
                        out[rowIdx * w + x] = row[x];
                    }
                }
            }

            return true;
        }

        bool LoadPNGGray(const std::string &path, std::vector<uint8_t> &out, int &w, int &h)
        {
            FILE *fp = std::fopen(path.c_str(), "rb");
            if (!fp) return false;
            png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
            if (!png) { fclose(fp); return false; }
            png_infop info = png_create_info_struct(png);
            if (!info) { png_destroy_read_struct(&png, nullptr, nullptr); fclose(fp); return false; }
            if (setjmp(png_jmpbuf(png))) { png_destroy_read_struct(&png, &info, nullptr); fclose(fp); return false; }
            png_init_io(png, fp);
            png_read_info(png, info);
            png_uint_32 width = png_get_image_width(png, info);
            png_uint_32 height = png_get_image_height(png, info);
            int color_type = png_get_color_type(png, info);
            int bit_depth = png_get_bit_depth(png, info);

            // expand paletted images to RGB
            if (color_type == PNG_COLOR_TYPE_PALETTE)
                png_set_palette_to_rgb(png);
            if (png_get_valid(png, info, PNG_INFO_tRNS))
                png_set_tRNS_to_alpha(png);
            if (bit_depth == 16)
                png_set_strip_16(png);
            if (bit_depth < 8)
                png_set_packing(png);
            if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
                png_set_expand_gray_1_2_4_to_8(png);

            png_read_update_info(png, info);
            png_size_t rowbytes = png_get_rowbytes(png, info);
            std::vector<uint8_t> row(rowbytes);
            out.assign(width * height, 0);
            for (unsigned y = 0; y < height; ++y)
            {
                png_read_row(png, row.data(), nullptr);
                for (unsigned x = 0; x < width; ++x)
                {
                    uint8_t gray = 0;
                    if (png_get_channels(png, info) == 1)
                    {
                        gray = row[x];
                    }
                    else
                    {
                        int idx = x * 3;
                        uint8_t r = row[idx + 0];
                        uint8_t g = row[idx + 1];
                        uint8_t b = row[idx + 2];
                        gray = static_cast<uint8_t>((int(r) + int(g) + int(b)) / 3);
                    }
                    out[y * width + x] = gray;
                }
            }

            png_destroy_read_struct(&png, &info, nullptr);
            fclose(fp);
            w = (int)width; h = (int)height;
            return true;
        }

        bool LoadJpegGray(const std::string &path, std::vector<uint8_t> &out, int &w, int &h)
        {
            FILE *fp = std::fopen(path.c_str(), "rb");
            if (!fp) return false;
            struct jpeg_decompress_struct cinfo;
            struct jpeg_error_mgr jerr;
            cinfo.err = jpeg_std_error(&jerr);
            jpeg_create_decompress(&cinfo);
            jpeg_stdio_src(&cinfo, fp);
            jpeg_read_header(&cinfo, TRUE);
            jpeg_start_decompress(&cinfo);
            w = cinfo.output_width; h = cinfo.output_height;
            int row_stride = w * cinfo.output_components;
            out.assign(w * h, 0);
            std::vector<JSAMPLE> buffer(row_stride);
            while (cinfo.output_scanline < cinfo.output_height)
            {
                JSAMPROW rowptr = &buffer[0];
                jpeg_read_scanlines(&cinfo, &rowptr, 1);
                int y = cinfo.output_scanline - 1;
                for (int x = 0; x < w; ++x)
                {
                    if (cinfo.output_components == 1)
                        out[y * w + x] = buffer[x];
                    else
                    {
                        int idx = x * cinfo.output_components;
                        uint8_t r = buffer[idx + 0];
                        uint8_t g = buffer[idx + 1];
                        uint8_t b = buffer[idx + 2];
                        out[y * w + x] = static_cast<uint8_t>((int(r) + int(g) + int(b)) / 3);
                    }
                }
            }
            jpeg_finish_decompress(&cinfo);
            jpeg_destroy_decompress(&cinfo);
            fclose(fp);
            return true;
        }

        bool LoadImageGray(const std::string &path, std::vector<uint8_t> &out, int &w, int &h)
        {
            std::string low = ToLower(path);
            if (low.size() >= 4 && low.substr(low.size()-4) == ".bmp")
                return LoadBMPGray(path, out, w, h);
            if (low.size() >= 4 && low.substr(low.size()-4) == ".png")
                return LoadPNGGray(path, out, w, h);
            if (low.size() >= 4 && (low.substr(low.size()-4) == ".jpg" || low.substr(low.size()-5) == ".jpeg"))
                return LoadJpegGray(path, out, w, h);
            return false;
        }

        // 将灰度数据写出为 PNG（8-bit 灰度）
        bool SavePNGGray(const std::string &path, const std::vector<uint8_t> &img, int w, int h)
        {
            FILE *fp = std::fopen(path.c_str(), "wb");
            if (!fp) return false;
            png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
            if (!png) { fclose(fp); return false; }
            png_infop info = png_create_info_struct(png);
            if (!info) { png_destroy_write_struct(&png, nullptr); fclose(fp); return false; }
            if (setjmp(png_jmpbuf(png))) { png_destroy_write_struct(&png, &info); fclose(fp); return false; }
            png_init_io(png, fp);
            png_set_IHDR(png, info, w, h, 8, PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
                PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
            png_write_info(png, info);
            std::vector<png_bytep> rows(h);
            std::vector<uint8_t> buf(w * h);
            std::memcpy(buf.data(), img.data(), w * h);
            for (int y = 0; y < h; ++y) rows[y] = buf.data() + y * w;
            png_write_image(png, rows.data());
            png_write_end(png, info);
            png_destroy_write_struct(&png, &info);
            fclose(fp);
            return true;
        }

        PolygonsD ExtractContoursFromBinary(const std::vector<uint8_t>& img, int w, int h, double pixelSize)
        {
            PolygonsD res;
            std::vector<char> seen(w * h, 0);

            auto inb = [&](int x, int y){ return x >= 0 && x < w && y >= 0 && y < h; };

            for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                int idx = y * w + x;
                if (img[idx] == 0 || seen[idx]) continue;
                std::vector<int> comp;
                std::queue<int> q;
                q.push(idx); seen[idx] = 1;
                while (!q.empty())
                {
                    int cur = q.front(); q.pop();
                    comp.push_back(cur);
                    int cx = cur % w, cy = cur / w;
                    for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        if (std::abs(dx) + std::abs(dy) != 1) continue; // 4-neighbor
                        int nx = cx + dx, ny = cy + dy;
                        if (!inb(nx, ny)) continue;
                        int nidx = ny * w + nx;
                        if (!seen[nidx] && img[nidx]) { seen[nidx] = 1; q.push(nidx); }
                    }
                }

                std::vector<std::pair<int,int>> border;
                for (int v : comp)
                {
                    int cx = v % w, cy = v / w;
                    bool isBorder = false;
                    for (int k = 0; k < 4 && !isBorder; ++k)
                    {
                        int nx = cx + (k==0?-1:k==1?1:0);
                        int ny = cy + (k==2?-1:k==3?1:0);
                        if (!inb(nx, ny) || img[ny*w + nx] == 0) isBorder = true;
                    }
                    if (isBorder) border.emplace_back(cx, cy);
                }

                if (border.empty()) continue;
                double ccx = 0, ccy = 0;
                for (auto &p : border) { ccx += p.first; ccy += p.second; }
                ccx /= border.size(); ccy /= border.size();
                std::sort(border.begin(), border.end(), [&](const auto &a, const auto &b){
                    double aa = std::atan2(a.second - ccy, a.first - ccx);
                    double bb = std::atan2(b.second - ccy, b.first - ccx);
                    return aa < bb;
                });

                PolygonD poly;
                poly.reserve(border.size());
                for (auto &p : border)
                {
                    poly.emplace_back(Point2D{ p.first * pixelSize, p.second * pixelSize });
                }

                PolygonsD simp = MakeSimple(poly);
                for (auto &sp : simp) res.emplace_back(sp);
            }

            return res;
        }
    }

    PolygonsD FromImage(const std::string& path, int threshold, double pixelSize)
    {
        std::vector<uint8_t> img;
        int w=0, h=0;
        if (!LoadImageGray(path, img, w, h)) return {};

        std::vector<uint8_t> bin(w*h);
        for (int i = 0; i < w*h; ++i) bin[i] = (img[i] > threshold) ? 255 : 0;

        return ExtractContoursFromBinary(bin, w, h, pixelSize);
    }

    // rasterize polygons to grayscale image and save as PNG
    bool ToImage(const PolygonsD& polys, int width, int height, double pixelSize, const std::string& outPath,
        uint8_t foreground, uint8_t background)
    {
        if (width <= 0 || height <= 0) return false;
        std::vector<uint8_t> img(width * height, background);

        // For each polygon (fill using even-odd rule), do scanline fill
        for (const auto &poly : polys)
        {
            if (poly.empty()) continue;
            // compute y range in pixel coordinates
            double miny = poly[0].y, maxy = poly[0].y;
            for (const auto &pt : poly) { miny = std::min(miny, pt.y); maxy = std::max(maxy, pt.y); }
            int y0 = std::max(0, (int)std::floor(miny / pixelSize));
            int y1 = std::min(height-1, (int)std::ceil(maxy / pixelSize));

            for (int py = y0; py <= y1; ++py)
            {
                double yworld = py * pixelSize + pixelSize * 0.5; // sample at pixel center
                std::vector<double> xs;
                for (size_t i = 0; i < poly.size(); ++i)
                {
                    const auto &a = poly[i];
                    const auto &b = poly[(i+1) % poly.size()];
                    if ((a.y <= yworld && b.y > yworld) || (b.y <= yworld && a.y > yworld))
                    {
                        double t = (yworld - a.y) / (b.y - a.y);
                        double x = a.x + t * (b.x - a.x);
                        xs.push_back(x);
                    }
                }
                if (xs.empty()) continue;
                std::sort(xs.begin(), xs.end());
                for (size_t k = 0; k + 1 < xs.size(); k += 2)
                {
                    double xl = xs[k], xr = xs[k+1];
                    int ix0 = std::max(0, (int)std::floor(xl / pixelSize));
                    int ix1 = std::min(width-1, (int)std::floor(xr / pixelSize));
                    for (int px = ix0; px <= ix1; ++px)
                    {
                        img[py * width + px] = foreground;
                    }
                }
            }
        }

        return SavePNGGray(outPath, img, width, height);
    }

} // namespace HsBa::Slicer
