// Copyright 2019 Vilya Harvey
#include "miniply.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

//
// Timer class
//

class Timer {
public:
  Timer(bool autostart=false);

  void start();
  void stop();

  double elapsedMS() const;

private:
  std::chrono::high_resolution_clock::time_point _start;
  std::chrono::high_resolution_clock::time_point _stop;
  bool _running = false;
};


Timer::Timer(bool autostart)
{
  if (autostart) {
    start();
  }
}


void Timer::start()
{
  _start = _stop = std::chrono::high_resolution_clock::now();
  _running = true;
}


void Timer::stop()
{
  if (_running) {
    _stop = std::chrono::high_resolution_clock::now();
    _running = false;
  }
}


double Timer::elapsedMS() const
{
  std::chrono::duration<double, std::chrono::milliseconds::period> ms =
     (_running ? std::chrono::high_resolution_clock::now() : _stop) - _start;
  return ms.count();
}


//
// TriMesh type
//

// This is what we populate to test & benchmark data extraction from the PLY
// file. It's a triangle mesh, so any faces with more than three verts will
// get triangulated.
struct TriMesh {
  // Per-vertex data
  float* pos     = nullptr; // has 3*numVerts elements.
  float* normal  = nullptr; // if non-null, has 3 * numVerts elements.
  float* tangent = nullptr; // if non-null, has 3 * numVerts elements.
  float* uv      = nullptr; // if non-null, has 2 * numVerts elements.
  int numVerts   = 0;

  // Per-index data
  int* indices   = nullptr;
  int numIndices = 0; // number of indices = 3 times the number of faces.

  ~TriMesh() {
    delete[] pos;
    delete[] normal;
    delete[] tangent;
    delete[] uv;
    delete[] indices;
  }
};


static bool ply_parse_vertex_element(miniply::PLYReader& reader, TriMesh* trimesh)
{
  const miniply::PLYElement* elem = reader.element();
  trimesh->numVerts = elem->count;
  trimesh->pos = new float[elem->count * 3];
  if (!reader.extract_vec3("x", "y", "z", trimesh->pos)) {
    return false; // invalid data: vertex data MUST include a position
  }

  if (reader.has_vec3("nx", "ny", "nz")) {
    trimesh->normal = new float[elem->count * 3];
    if (!reader.extract_vec3("nx", "ny", "nz", trimesh->normal)) {
      return false; // invalid data, couldn't parse normal.
    }
  }

  bool uvsOK = true;
  if (reader.has_vec2("u", "v")) {
    trimesh->uv = new float[elem->count * 2];
    uvsOK = reader.extract_vec2("u", "v", trimesh->uv);
  }
  else if (reader.has_vec2("s", "t")) {
    trimesh->uv = new float[elem->count * 2];
    uvsOK = reader.extract_vec2("s", "t", trimesh->uv);
  }
  else if (reader.has_vec2("texture_u", "texture_v")) {
    trimesh->uv = new float[elem->count * 2];
    uvsOK = reader.extract_vec2("texture_u", "texture_v", trimesh->uv);
  }
  else if (reader.has_vec2("texture_s", "texture_t")) {
    trimesh->uv = new float[elem->count * 2];
    uvsOK = reader.extract_vec2("texture_s", "texture_t", trimesh->uv);
  }
  if (!uvsOK) {
    return false; // invalid data, couldn't parse tex coords
  }

  return true;
}


static bool ply_parse_face_element(miniply::PLYReader& reader, TriMesh* trimesh)
{
  // Find the indices property.
  bool allTris = false;
  int numIndices = reader.count_triangles("vertex_indices", &allTris) * 3;
  if (numIndices == 0) {
    return false;
  }

  trimesh->numIndices = numIndices;
  trimesh->indices = new int[numIndices];
  if (!reader.extract_triangles("vertex_indices", trimesh->pos, trimesh->numVerts, trimesh->indices)) {
    return false;
  }

  return true;
}


static TriMesh* parse_file(const char* filename)
{
  miniply::PLYReader reader(filename);
  if (!reader.valid()) {
    return nullptr;
  }

  TriMesh* trimesh = new TriMesh();
  bool gotVerts = false;
  bool gotFaces = false;
  while (reader.has_element() && (!gotVerts || !gotFaces)) {
    const miniply::PLYElement* elem = reader.element();
    if (!gotVerts && strcmp(elem->name.c_str(), "vertex") == 0) {
      bool ok = reader.load_element() && ply_parse_vertex_element(reader, trimesh);
      if (!ok) {
        break; // failed to load data for this element.
      }
      gotVerts = true;
    }
    else if (!gotFaces && strcmp(elem->name.c_str(), "face") == 0) {
      bool ok = reader.load_element() && ply_parse_face_element(reader, trimesh);
      if (!ok) {
        break; // failed to load data for this element.
      }
      gotFaces = true;
    }
    reader.next_element();
  }

  if (!gotVerts || !gotFaces) {
    delete trimesh;
    return nullptr;
  }

  return trimesh;
}


static bool has_extension(const char* filename, const char* ext)
{
  int j = int(strlen(ext));
  int i = int(strlen(filename)) - j;
  if (i <= 0 || filename[i - 1] != '.') {
    return false;
  }
  return strcmp(filename + i, ext) == 0;
}


int main(int argc, char** argv)
{
  const int kFilenameBufferLen = 16 * 1024 - 1;
  char* filenameBuffer = new char[kFilenameBufferLen + 1];
  filenameBuffer[kFilenameBufferLen] = '\0';

  std::vector<std::string> filenames;
  for (int i = 1; i < argc; i++) {
    if (has_extension(argv[i], "txt")) {
      FILE* f = fopen(argv[i], "r");
      if (f != nullptr) {
        while (fgets(filenameBuffer, kFilenameBufferLen, f)) {
          filenames.push_back(filenameBuffer);
          while (filenames.back().back() == '\n') {
            filenames.back().pop_back();
          }
        }
        fclose(f);
      }
      else {
        fprintf(stderr, "Failed to open %s\n", argv[i]);
      }
    }
    else {
      filenames.push_back(argv[i]);
    }
  }

  if (filenames.empty()) {
    fprintf(stderr, "No input files provided.\n");
    return EXIT_SUCCESS;
  }

  int width = 0;
  for (const std::string& filename : filenames) {
    int newWidth = int(filename.size());
    if (newWidth > width) {
      width = newWidth;
    }
  }

  Timer overallTimer(true); // true ==> autostart the timer.
  int numPassed = 0;
  int numFailed = 0;
  for (const std::string& filename : filenames) {
    Timer timer(true); // true ==> autostart the timer.

    TriMesh* trimesh = parse_file(filename.c_str());
    bool ok = trimesh != nullptr;

    timer.stop();
    
    delete trimesh;

    printf("%-*s  %s  %8.3lf ms\n", width, filename.c_str(), ok ? "passed" : "FAILED", timer.elapsedMS());
    if (!ok) {
      ++numFailed;
    }
    else {
      ++numPassed;
    }
    fflush(stdout);
  }

  overallTimer.stop();
  printf("----\n");
  printf("%.3lf ms total\n", overallTimer.elapsedMS());
  printf("%d passed\n", numPassed);
  printf("%d failed\n", numFailed);
  return (numFailed > 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}
