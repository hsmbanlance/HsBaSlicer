/** @file ImageToPolygons.cpp
 * @brief Implementations of raster-to-polygon thresholding, polygon-to-image rendering and Lua-driven image generation.
 * @author HsBa
 */
#include "ImageToPolygons.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <queue>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
// use OpenCV for image IO and simple raster operations
#ifdef HAS_OPENCV
#include <opencv2/opencv.hpp>
#endif
#include <lua.hpp>
#include <png.h>
#include <turbojpeg.h>

#include "LuaAdapter.hpp"
#include "base/error.hpp"
#include "base/string_helper.hpp"
#include "utils/LuaNewObject.hpp"

namespace HsBa::Slicer
{
namespace
{
inline constexpr int SVG_PERCENT_VALUE = 100;

bool LoadImageGray(const std::string& path, std::vector<uint8_t>& out, int& w, int& h)
{
#ifdef HAS_OPENCV
    try
    {
        const cv::Mat img = cv::imread(path, cv::IMREAD_UNCHANGED);
        if (img.empty())
        {
            return false;
        }
        cv::Mat gray;
        if (img.channels() == 1)
        {
            gray = img;
        }
        else
        {
            cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
        }
        w = gray.cols;
        h = gray.rows;
        out.assign(static_cast<size_t>(w) * h, 0);
        for (int y = 0; y < h; ++y)
        {
            const uint8_t* rowptr = gray.ptr<uint8_t>(y);
            for (int x = 0; x < w; ++x)
            {
                out[y * w + x] = rowptr[x];
            }
        }
        return true;
    }
    catch (const cv::Exception&)
    {
        return false;
    }
#else
    return false;
#endif
}

bool SavePNG(const std::string& path, const std::vector<uint8_t>& img, int w, int h);

bool SavePNGGray(const std::string& path, const std::vector<uint8_t>& img, int w, int h)
{
#ifdef HAS_OPENCV
    try
    {
        const cv::Mat m(h, w, CV_8UC1);
        std::memcpy(m.data, img.data(), static_cast<size_t>(w) * h);
        return cv::imwrite(path, m);
    }
    catch (const cv::Exception&)
    {
        return false;
    }
#else
    return SavePNG(path, img, w, h);
#endif
}

PolygonsD ExtractContoursFromBinary(const std::vector<uint8_t>& img, int w, int h, double pixelSize)
{
    PolygonsD res;
    std::vector<char> seen(static_cast<size_t>(w) * h, 0);

    auto inb = [&](int x, int y) { return x >= 0 && x < w && y >= 0 && y < h; };

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            int const idx = y * w + x;
            if (img[idx] == 0 || seen[idx])
            {
                continue;
            }
            std::vector<int> comp;
            std::queue<int> q;
            q.push(idx);
            seen[idx] = 1;
            while (!q.empty())
            {
                int const cur = q.front();
                q.pop();
                comp.push_back(cur);
                int const cx = cur % w;
                int const cy = cur / w;
                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        if (std::abs(dx) + std::abs(dy) != 1)
                        {
                            continue;  // 4-neighbor
                        }
                        int const nx = cx + dx;
                        int const ny = cy + dy;
                        if (!inb(nx, ny))
                        {
                            continue;
                        }
                        int const nidx = ny * w + nx;
                        if (!seen[nidx] && img[nidx])
                        {
                            seen[nidx] = 1;
                            q.push(nidx);
                        }
                    }
                }
            }

            std::vector<std::pair<int, int>> border;
            for (int const v : comp)
            {
                int const cx = v % w;
                int const cy = v / w;
                bool const isBorder = !inb(cx - 1, cy) || img[cy * w + (cx - 1)] == 0 || !inb(cx + 1, cy) ||
                                      img[cy * w + (cx + 1)] == 0 || !inb(cx, cy - 1) || img[(cy - 1) * w + cx] == 0 ||
                                      !inb(cx, cy + 1) || img[(cy + 1) * w + cx] == 0;
                if (isBorder)
                {
                    border.emplace_back(cx, cy);
                }
            }

            if (border.empty())
            {
                continue;
            }
            double ccx = 0;
            double ccy = 0;
            for (auto& p : border)
            {
                ccx += p.first;
                ccy += p.second;
            }
            ccx /= static_cast<double>(border.size());
            ccy /= static_cast<double>(border.size());
            std::ranges::sort(border,
                              [&](const auto& a, const auto& b)
                              {
                                  double const aa = std::atan2(a.second - ccy, a.first - ccx);
                                  double const bb = std::atan2(b.second - ccy, b.first - ccx);
                                  return aa < bb;
                              });

            PolygonD poly;
            poly.reserve(border.size());
            for (auto& p : border)
            {
                poly.emplace_back(Point2D{p.first * pixelSize, p.second * pixelSize});
            }

            PolygonsD const simp = MakeSimple(poly);
            for (const auto& sp : simp)
            {
                res.emplace_back(sp);
            }
        }
    }

    return res;
}

bool pointInPolygon(const PolygonD& poly, double x, double y)
{
    if (poly.empty())
    {
        return false;
    }
    size_t const n = poly.size();
    bool inside = false;
    for (size_t i = 0, j = n - 1; i < n; j = i++)
    {
        const Point2D& a = poly[i];
        const Point2D& b = poly[j];
        if (((a.y > y) != (b.y > y)) && (x < a.x + (b.x - a.x) * (y - a.y) / (b.y - a.y)))
        {
            inside = !inside;
        }
    }
    return inside;
}

// libpng memory sink: appends produced bytes to the std::string passed as io pointer.
void PngWriteSink(png_structp png, png_bytep data, png_size_t length)
{
    auto* buf = static_cast<std::string*>(png_get_io_ptr(png));
    buf->append(reinterpret_cast<const char*>(data), length);
}

// In-memory sink has nothing to flush.
void PngFlushSink([[maybe_unused]] png_structp png)
{
}

// Encode a grayscale bitmap into a PNG byte buffer held in memory.
bool EncodePngBuffer(const std::vector<uint8_t>& img, int w, int h, std::string& out)
{
    out.clear();
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png)
    {
        return false;
    }
    png_infop info = png_create_info_struct(png);
    if (!info)
    {
        png_destroy_write_struct(&png, nullptr);
        return false;
    }
    if (setjmp(png_jmpbuf(png)))
    {
        png_destroy_write_struct(&png, &info);
        return false;
    }
    png_set_write_fn(png, &out, PngWriteSink, PngFlushSink);
    png_set_IHDR(png, info, w, h, 8, PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    for (int y = 0; y < h; ++y)
    {
        png_write_row(png, &img[static_cast<size_t>(y) * w]);
    }
    png_write_end(png, nullptr);
    png_destroy_write_struct(&png, &info);
    return true;
}

// Encode a grayscale bitmap into a JPEG byte buffer held in memory (via libjpeg-turbo).
bool EncodeJpgBuffer(const std::vector<uint8_t>& img, int w, int h, int quality, std::string& out)
{
    out.clear();
    tjhandle handle = tjInitCompress();
    if (!handle)
    {
        return false;
    }
    unsigned char* jpegBuf = nullptr;
    unsigned long jpegSize = 0;
    int const ret = tjCompress2(handle, const_cast<unsigned char*>(img.data()), w, 0, h, TJPF_GRAY, &jpegBuf, &jpegSize,
                                TJSAMP_GRAY, quality, TJFLAG_FASTDCT);
    if (ret != 0)
    {
        tjDestroy(handle);
        return false;
    }
    out.assign(reinterpret_cast<const char*>(jpegBuf), jpegSize);
    tjFree(jpegBuf);
    tjDestroy(handle);
    return true;
}

// Collect the minimum x/y corner of the polygon bounding box; has_bounds is false for an empty set.
void ComputeBounds(const PolygonsD& polys, double& min_x, double& min_y, bool& has_bounds)
{
    min_x = 0.0;
    min_y = 0.0;
    has_bounds = false;
    for (const auto& poly : polys)
    {
        for (const auto& pt : poly)
        {
            if (!has_bounds)
            {
                min_x = pt.x;
                min_y = pt.y;
                has_bounds = true;
            }
            else
            {
                min_x = std::min(min_x, pt.x);
                min_y = std::min(min_y, pt.y);
            }
        }
    }
}

// Lowercase the extension and ensure a single leading dot (accepts "png" or ".png").
std::string NormalizeExt(std::string_view ext)
{
    std::string low = Utils::to_lower(std::string(ext));
    if (!low.empty() && low.front() != '.')
    {
        low.insert(low.begin(), '.');
    }
    return low;
}

// Render polygons into a standalone SVG document string (each polygon as a <polygon> element).
bool BuildSvg(const PolygonsD& polys, int width, int height, double pixelSize, double min_x, double min_y,
              uint8_t foreground, uint8_t background, std::string& out)
{
    std::ostringstream ofs;
    ofs << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    ofs << R"(<svg xmlns="http://www.w3.org/2000/svg" width=")" << width << "\" height=\"" << height
        << "\" viewBox=\"0 0 " << width << " " << height << "\">\n";
    // background
    ofs << "<rect width=\"" << SVG_PERCENT_VALUE << "%\" height=\"" << SVG_PERCENT_VALUE << "%\" fill=\"rgb("
        << static_cast<int>(background) << "," << static_cast<int>(background) << "," << static_cast<int>(background)
        << ")\"/>\n";
    for (const auto& poly : polys)
    {
        if (poly.empty())
        {
            continue;
        }
        // build point list in pixel coordinates
        std::ostringstream pts;
        for (const auto& pt : poly)
        {
            double const px = (pt.x - min_x) / pixelSize + 0.5;  // center sample
            double const py = (pt.y - min_y) / pixelSize + 0.5;
            pts << px << "," << py << " ";
        }
        ofs << "<polygon points=\"" << pts.str() << "\" fill=\"rgb(" << static_cast<int>(foreground) << ","
            << static_cast<int>(foreground) << "," << static_cast<int>(foreground) << ")\" stroke=\"none\" />\n";
    }
    ofs << "</svg>\n";
    out = ofs.str();
    return true;
}

// Write a raw byte buffer to disk (binary), returning false if the file cannot be opened/written.
bool WriteAllBytes(const std::string& path, const std::string_view data)
{
    std::ofstream ofs(std::filesystem::path(path), std::ios::binary);
    if (!ofs)
    {
        return false;
    }
    ofs.write(data.data(), static_cast<std::streamsize>(data.size()));
    ofs.close();
    return static_cast<bool>(ofs);
}

bool SavePNG(const std::string& path, const std::vector<uint8_t>& img, int w, int h)
{
    std::string buffer;
    if (!EncodePngBuffer(img, w, h, buffer))
    {
        return false;
    }
    return WriteAllBytes(path, buffer);
}

bool SaveJPG(const std::string& path, const std::vector<uint8_t>& img, int w, int h, int quality = 90)
{
    std::string buffer;
    if (!EncodeJpgBuffer(img, w, h, quality, buffer))
    {
        return false;
    }
    return WriteAllBytes(path, buffer);
}

void RasterizePolygons(const PolygonsD& polys, std::vector<uint8_t>& img, int width, int height, double pixelSize,
                       double origin_x, double origin_y, uint8_t foreground)
{
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            double const px = origin_x + (x + 0.5) * pixelSize;
            double const py = origin_y + (y + 0.5) * pixelSize;
            for (const auto& poly : polys)
            {
                if (pointInPolygon(poly, px, py))
                {
                    img[y * width + x] = foreground;
                    break;
                }
            }
        }
    }
}
}  // namespace

PolygonsD FromImage(const std::string& path, int threshold, double pixelSize)
{
    std::vector<PolygonsD> layers = FromImageMulti(path, std::vector<int>{threshold}, pixelSize);
    if (layers.empty())
    {
        return {};
    }
    return layers.front();
}

// rasterize polygons into an in-memory encoded buffer (PNG/JPEG/SVG chosen by ext)
bool ToImageBuffer(const PolygonsD& polys, int width, int height, double pixelSize, std::string_view ext,
                   std::string& outBuffer, uint8_t foreground, uint8_t background)
{
    outBuffer.clear();
    if (width <= 0 || height <= 0)
    {
        return false;
    }

    double min_x = 0.0;
    double min_y = 0.0;
    bool has_bounds = false;
    ComputeBounds(polys, min_x, min_y, has_bounds);
    if (!has_bounds)
    {
        return false;
    }

    std::string const low = NormalizeExt(ext);
    if (low == ".svg")
    {
        return BuildSvg(polys, width, height, pixelSize, min_x, min_y, foreground, background, outBuffer);
    }

#ifdef HAS_OPENCV
    try
    {
        // rasterize with OpenCV
        cv::Mat img(height, width, CV_8UC1, cv::Scalar(background));
        for (const auto& poly : polys)
        {
            if (poly.empty())
            {
                continue;
            }
            // build vector of points in pixel coordinates
            std::vector<std::vector<cv::Point>> contours;
            contours.emplace_back();
            for (const auto& pt : poly)
            {
                int const px = static_cast<int>(std::round((pt.x - min_x) / pixelSize));
                int const py = static_cast<int>(std::round((pt.y - min_y) / pixelSize));
                contours.back().push_back(cv::Point(px, py));
            }
            // fill polygon
            cv::fillPoly(img, contours, cv::Scalar(foreground));
        }
        std::string const fmt = low.empty() ? std::string(".png") : low;
        std::vector<uchar> buf;
        if (!cv::imencode(fmt, img, buf))
        {
            return false;
        }
        outBuffer.assign(reinterpret_cast<const char*>(buf.data()), buf.size());
        return true;
    }
    catch (const cv::Exception&)
    {
        return false;
    }
#else
    // rasterize manually
    std::vector<uint8_t> gray(static_cast<size_t>(width) * height, background);
    RasterizePolygons(polys, gray, width, height, pixelSize, min_x, min_y, foreground);
    if (low == ".jpg" || low == ".jpeg")
    {
        return EncodeJpgBuffer(gray, width, height, 90, outBuffer);
    }
    return EncodePngBuffer(gray, width, height, outBuffer);
#endif
}

// rasterize polygons to an image file; format chosen from the outPath extension
bool ToImage(const PolygonsD& polys, int width, int height, double pixelSize, const std::string& outPath,
             uint8_t foreground, uint8_t background)
{
    if (outPath.empty())
    {
        return false;
    }
    std::string const ext = std::filesystem::path(outPath).extension().string();
    std::string buffer;
    if (!ToImageBuffer(polys, width, height, pixelSize, ext, buffer, foreground, background))
    {
        return false;
    }
    return WriteAllBytes(outPath, buffer);
}

std::vector<PolygonsD> FromImageMulti(const std::string& path, const std::vector<int>& thresholds, double pixelSize)
{
    std::vector<PolygonsD> res;
    std::vector<uint8_t> img;
    int w = 0;
    int h = 0;
    if (!LoadImageGray(path, img, w, h))
    {
        return res;
    }
    if (w <= 0 || h <= 0)
    {
        return res;
    }

    // For each threshold create a binary image and extract contours
    for (int const thr : thresholds)
    {
        std::vector<uint8_t> bin(static_cast<size_t>(w) * h);
        for (int i = 0; i < w * h; ++i)
        {
            bin[i] = (std::cmp_greater(img[i], thr)) ? MAX_GRAY_VALUE : 0;
        }
        PolygonsD layer = ExtractContoursFromBinary(bin, w, h, pixelSize);
        res.emplace_back(std::move(layer));
    }
    return res;
}

bool LuaToImage(const PolygonsD& poly, const std::string& scriptPath, const std::string& outPath,
                const std::string& functionName, const std::function<void(lua_State*)>& lua_reg)
{
    auto L = MakeUniqueLuaState();
    if (!L)
    {
        throw RuntimeError("Failed to create Lua state");
    }
    luaL_openlibs(L.get());
    RegisterLuaPolygonOperations(L.get());
    if (lua_reg)
    {
        lua_reg(L.get());
    }
    // load script
    if (luaL_loadfile(L.get(), scriptPath.c_str()) || lua_pcall(L.get(), 0, 0, 0))
    {
        std::string const err = lua_tostring(L.get(), -1);
        throw RuntimeError("Failed to load Lua script: " + err);
    }
    // get function
    lua_getglobal(L.get(), functionName.c_str());
    if (!lua_isfunction(L.get(), -1))
    {
        throw RuntimeError("Lua function not found: " + functionName);
    }
    // push polygon argument
    PushPolygonsDToLua(L.get(), poly);
    // call function with 1 arg, 1 result
    if (lua_pcall(L.get(), 1, 1, 0) != LUA_OK)
    {
        std::string const err = lua_tostring(L.get(), -1);
        throw RuntimeError("Error calling Lua function: " + err);
    }
    // expect table of grayscale image
    if (!lua_istable(L.get(), -1))
    {
        throw RuntimeError("Lua function did not return a table");
    }
    std::vector<uint8_t> img;
    size_t const len = lua_rawlen(L.get(), -1);
    for (size_t i = 1; i <= len; ++i)
    {
        lua_rawgeti(L.get(), -1, static_cast<int>(i));
        if (lua_isinteger(L.get(), -1))
        {
            int const val = static_cast<int>(lua_tointeger(L.get(), -1));
            img.push_back(static_cast<uint8_t>(std::clamp(val, 0, static_cast<int>(MAX_GRAY_VALUE))));
        }
        else
        {
            throw RuntimeError("Lua image table contains non-integer value");
        }
        lua_pop(L.get(), 1);  // pop value
    }
    // save image, use filesystem
    if (outPath.empty())
    {
        return false;
    }
    std::filesystem::path const outPathFs(outPath);
    std::ofstream ofs(outPathFs);
    ofs.write(reinterpret_cast<const char*>(img.data()), static_cast<std::streamsize>(img.size()));
    ofs.close();
    return true;
}

bool LuaToImageString(const PolygonsD& poly, const std::string& script, const std::string& outPath,
                      const std::string& functionName, const std::function<void(lua_State*)>& lua_reg)
{
    auto L = MakeUniqueLuaState();
    if (!L)
    {
        throw RuntimeError("Failed to create Lua state");
    }
    luaL_openlibs(L.get());
    RegisterLuaPolygonOperations(L.get());
    if (lua_reg)
    {
        lua_reg(L.get());
    }
    // load script and execute it (define function in global)
    if (luaL_loadstring(L.get(), script.c_str()) != LUA_OK)
    {
        std::string const err = lua_tostring(L.get(), -1);
        throw RuntimeError("Load string failed: " + err);
    }
    if (lua_pcall(L.get(), 0, LUA_MULTRET, 0) != LUA_OK)
    {
        std::string const err = lua_tostring(L.get(), -1);
        throw RuntimeError("Exec chunk failed: " + err);
    }
    // get function
    lua_getglobal(L.get(), functionName.c_str());
    if (!lua_isfunction(L.get(), -1))
    {
        throw RuntimeError("Function '" + functionName + "' not found");
    }
    // push polygon argument
    PushPolygonsDToLua(L.get(), poly);
    // call function with 1 arg, 1 result
    if (lua_pcall(L.get(), 1, 1, 0) != LUA_OK)
    {
        std::string const err = lua_tostring(L.get(), -1);
        throw RuntimeError("Error calling Lua function: " + err);
    }
    // expect table of grayscale image
    if (!lua_istable(L.get(), -1))
    {
        throw RuntimeError("Lua function did not return a table");
    }
    std::vector<uint8_t> img;
    size_t const len = lua_rawlen(L.get(), -1);
    for (size_t i = 1; i <= len; ++i)
    {
        lua_rawgeti(L.get(), -1, static_cast<int>(i));
        if (lua_isinteger(L.get(), -1))
        {
            int const val = static_cast<int>(lua_tointeger(L.get(), -1));
            img.push_back(static_cast<uint8_t>(std::clamp(val, 0, static_cast<int>(MAX_GRAY_VALUE))));
        }
        else
        {
            throw RuntimeError("Lua image table contains non-integer value");
        }
        lua_pop(L.get(), 1);  // pop value
    }
    // save image, use filesystem
    if (outPath.empty())
    {
        return false;
    }
    std::filesystem::path const outPathFs(outPath);
    std::ofstream ofs(outPathFs);
    ofs.write(reinterpret_cast<const char*>(img.data()), static_cast<std::streamsize>(img.size()));
    ofs.close();
    return true;
}

}  // namespace HsBa::Slicer
