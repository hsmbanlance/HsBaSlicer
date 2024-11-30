#pragma once
#ifndef HSBA_SLICER_DLLEXPORT_H
#define HSBA_SLICER_DLLEXPORT_H

#ifdef _WIN32
#ifdef HSBA_SLICER_EXPORTS
#define HSBA_SLICER_API __declspec(dllexport)
#else
#define HSBA_SLICER_API __declspec(dllimport)
#endif
#else
#define HSBA_SLICER_API
#endif

#endif // !HSBA_SLICER_DLLEXPORT_H
