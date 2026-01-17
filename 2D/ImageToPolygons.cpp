#include "ImageToPolygons.hpp"

#include <vector>
#include <string>
#include <fstream>
#include <queue>
#include <cstdint>
#include <algorithm>
#include <cstring>
#include <filesystem>
// use OpenCV for image IO and simple raster operations
#include <opencv2/opencv.hpp>
#include <lua.hpp>

#include "base/error.hpp"
#include "LuaAdapter.hpp"
#include "utils/LuaNewObject.hpp"

namespace HsBa::Slicer
{
    namespace
    {
        inline constexpr int SVG_PERCENT_VALUE = 100;
        static std::string ToLower(const std::string &s)
        {
            std::string out = s;
            for (auto &c : out) c = char(std::tolower((unsigned char)c));
            return out;
        }

        bool LoadImageGray(const std::string &path, std::vector<uint8_t> &out, int &w, int &h)
        {
            try {
                cv::Mat img = cv::imread(path, cv::IMREAD_UNCHANGED);
                if (img.empty()) return false;
                cv::Mat gray;
                if (img.channels() == 1) gray = img;
                else cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
                w = gray.cols; h = gray.rows;
                out.assign(w * h, 0);
                for (int y = 0; y < h; ++y)
                {
                    const uint8_t* rowptr = gray.ptr<uint8_t>(y);
                    for (int x = 0; x < w; ++x) out[y * w + x] = rowptr[x];
                }
                return true;
            }
            catch(const cv::Exception&) 
            {
                 return false; 
            }
        }

        bool SavePNGGray(const std::string &path, const std::vector<uint8_t> &img, int w, int h)
        {
            try {
                cv::Mat m(h, w, CV_8UC1);
                std::memcpy(m.data, img.data(), w * h);
                return cv::imwrite(path, m);
            }
            catch (const cv::Exception&)
            {
                return false;
            }
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
        std::vector<PolygonsD> layers = FromImageMulti(path, std::vector<int>{threshold}, pixelSize);
        if (layers.empty()) return {};
        return layers.front();
    }

    // rasterize polygons to grayscale image and save as PNG
    bool ToImage(const PolygonsD& polys, int width, int height, double pixelSize, const std::string& outPath,
        uint8_t foreground, uint8_t background)
    {
        if (width <= 0 || height <= 0) return false;

        std::string low = ToLower(outPath);
        if (low.size() >= 4 && low.substr(low.size() - 4) == ".svg")
        {
            // write simple SVG vector file: each polygon as a <polygon> element
            std::ofstream ofs(outPath);
            if (!ofs) return false;
            ofs << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
            ofs << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height << "\">\n";
            // background
            ofs << "<rect width=\"" << SVG_PERCENT_VALUE << "%\" height=\"" << SVG_PERCENT_VALUE << "%\" fill=\"rgb(" << int(background) << "," << int(background) << "," << int(background) << ")\"/>\n";
            for (const auto &poly : polys)
            {
                if (poly.empty()) continue;
                // build point list in pixel coordinates
                std::ostringstream pts;
                for (const auto &pt : poly)
                {
                    double px = pt.x / pixelSize + 0.5; // center sample
                    double py = pt.y / pixelSize + 0.5;
                    pts << px << "," << py << " ";
                }
                ofs << "<polygon points=\"" << pts.str() << "\" fill=\"rgb(" << int(foreground) << "," << int(foreground) << "," << int(foreground) << ")\" stroke=\"none\" />\n";
            }
            ofs << "</svg>\n";
            return true;
        }
        else
        {
            // rasterize with OpenCV
            cv::Mat img(height, width, CV_8UC1, cv::Scalar(background));
            for (const auto &poly : polys)
            {
                if (poly.empty()) continue;
                // build vector of points in pixel coordinates
                std::vector<std::vector<cv::Point>> contours;
                contours.emplace_back();
                for (const auto &pt : poly)
                {
                    int px = (int)std::round(pt.x / pixelSize);
                    int py = (int)std::round(pt.y / pixelSize);
                    contours.back().push_back(cv::Point(px, py));
                }
                // fill polygon
                cv::fillPoly(img, contours, cv::Scalar(foreground));
            }
            return cv::imwrite(outPath, img);
        }
    }

    std::vector<PolygonsD> FromImageMulti(const std::string& path, const std::vector<int>& thresholds, double pixelSize)
    {
        std::vector<PolygonsD> res;
        std::vector<uint8_t> img;
        int w=0, h=0;
        if (!LoadImageGray(path, img, w, h)) return res;
        if (w <= 0 || h <= 0) return res;

        // For each threshold create a binary image and extract contours
        for (int thr : thresholds)
        {
            std::vector<uint8_t> bin(w*h);
            for (int i = 0; i < w*h; ++i) bin[i] = (img[i] > thr) ? MAX_GRAY_VALUE : 0;
            PolygonsD layer = ExtractContoursFromBinary(bin, w, h, pixelSize);
            res.emplace_back(std::move(layer));
        }
        return res;
    }

    bool LuaToImage(const PolygonsD& poly, const std::string& scriptPath, const std::string& outPath, const std::string& functionName)
    {
        auto L = MakeUniqueLuaState();
        if (!L) throw RuntimeError("Failed to create Lua state");
        luaL_openlibs(L.get());
        RegisterLuaPolygonOperations(L.get());
        // load script
        if (luaL_loadfile(L.get(), scriptPath.c_str()) || lua_pcall(L.get(), 0, 0, 0))
        {
            std::string err = lua_tostring(L.get(), -1);
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
            std::string err = lua_tostring(L.get(), -1);
            throw RuntimeError("Error calling Lua function: " + err);
        }
        // expect table of grayscale image
        if (!lua_istable(L.get(), -1))
        {
            throw RuntimeError("Lua function did not return a table");
        }
        std::vector<uint8_t> img;
        size_t len = lua_rawlen(L.get(), -1);
        for (size_t i = 1; i <= len; ++i)
        {
            lua_rawgeti(L.get(), -1, static_cast<int>(i));
            if (lua_isinteger(L.get(), -1))
            {
                int val = static_cast<int>(lua_tointeger(L.get(), -1));
                img.push_back(static_cast<uint8_t>(std::clamp(val, 0, static_cast<int>(MAX_GRAY_VALUE))));
            }
            else
            {
                throw RuntimeError("Lua image table contains non-integer value");
            }
            lua_pop(L.get(), 1); // pop value
        }
        // save image, use filesystem
        if(outPath.empty())
        {
            return false;
        }
        std::filesystem::path outPathFs(outPath);
        std::ofstream ofs(outPathFs);
        ofs.write(reinterpret_cast<const char*>(img.data()), img.size());
        ofs.close();
        return true;
    }

    bool LuaToImageString(const PolygonsD& poly, const std::string& script, const std::string& outPath, const std::string& functionName)
    {
        auto L = MakeUniqueLuaState();
        if (!L) throw RuntimeError("Failed to create Lua state");
        luaL_openlibs(L.get());
        RegisterLuaPolygonOperations(L.get());
        // load script and execute it (define function in global)
        if (luaL_loadstring(L.get(), script.c_str()) != LUA_OK) {
            std::string err = lua_tostring(L.get(), -1);
            throw RuntimeError("Load string failed: " + err);
        }
        if (lua_pcall(L.get(), 0, LUA_MULTRET, 0) != LUA_OK) {
            std::string err = lua_tostring(L.get(), -1);
            throw RuntimeError("Exec chunk failed: " + err);
        }
        // get function
        lua_getglobal(L.get(), functionName.c_str());
        if (!lua_isfunction(L.get(), -1)) {
            throw RuntimeError("Function '" + functionName + "' not found");
        }
        // push polygon argument
        PushPolygonsDToLua(L.get(), poly);
        // call function with 1 arg, 1 result
        if (lua_pcall(L.get(), 1, 1, 0) != LUA_OK)
        {
            std::string err = lua_tostring(L.get(), -1);
            throw RuntimeError("Error calling Lua function: " + err);
        }
        // expect table of grayscale image
        if (!lua_istable(L.get(), -1))
        {
            throw RuntimeError("Lua function did not return a table");
        }
        std::vector<uint8_t> img;
        size_t len = lua_rawlen(L.get(), -1);
        for (size_t i = 1; i <= len; ++i)
        {
            lua_rawgeti(L.get(), -1, static_cast<int>(i));
            if (lua_isinteger(L.get(), -1))
            {
                int val = static_cast<int>(lua_tointeger(L.get(), -1));
                img.push_back(static_cast<uint8_t>(std::clamp(val, 0, static_cast<int>(MAX_GRAY_VALUE))));
            }
            else
            {
                throw RuntimeError("Lua image table contains non-integer value");
            }
            lua_pop(L.get(), 1); // pop value
        }
        // save image, use filesystem
        if (outPath.empty())
        {
            return false;
        }
        std::filesystem::path outPathFs(outPath);
        std::ofstream ofs(outPathFs);
        ofs.write(reinterpret_cast<const char*>(img.data()), img.size());
        ofs.close();
        return true;
    }

} // namespace HsBa::Slicer
