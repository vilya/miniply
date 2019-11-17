// Copyright 2019 Vilya Harvey
#include "miniply.h"

// Other PLY parsers to compare with miniply:
#include <happly.h>

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
  uint32_t numVerts   = 0;

  // Per-index data
  int* indices   = nullptr;
  uint32_t numIndices = 0; // number of indices = 3 times the number of faces.

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
  uint32_t numIndices = reader.count_triangles("vertex_indices") * 3;
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


static TriMesh* parse_file_with_miniply(const char* filename)
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


static TriMesh* parse_file_with_happly(const char* filename)
{
  happly::PLYData plyIn(filename);
  if (!plyIn.hasElement("vertex") || !plyIn.hasElement("face")) {
    return nullptr;
  }

  TriMesh* trimesh = new TriMesh();

  // Load vertex data.
  {
    happly::Element& elem = plyIn.getElement("vertex");
    trimesh->numVerts = uint32_t(elem.count);

    trimesh->pos = new float[trimesh->numVerts * 3];
    std::vector<float> xvals = elem.getProperty<float>("x");
    std::vector<float> yvals = elem.getProperty<float>("y");
    std::vector<float> zvals = elem.getProperty<float>("z");
    for (uint32_t i = 0; i < trimesh->numVerts; i++) {
      trimesh->pos[3 * i    ] = xvals[i];
      trimesh->pos[3 * i + 1] = yvals[i];
      trimesh->pos[3 * i + 2] = zvals[i];
    }

    if (elem.hasProperty("nx") && elem.hasProperty("ny") && elem.hasProperty("nz")) {
      trimesh->normal = new float[trimesh->numVerts * 3];
      xvals = elem.getProperty<float>("nx");
      yvals = elem.getProperty<float>("ny");
      zvals = elem.getProperty<float>("nz");
      for (uint32_t i = 0; i < trimesh->numVerts; i++) {
        trimesh->normal[3 * i    ] = xvals[i];
        trimesh->normal[3 * i + 1] = yvals[i];
        trimesh->normal[3 * i + 2] = zvals[i];
      }
    }

    bool hasUV = false;
    if (elem.hasProperty("u") && elem.hasProperty("v")) {
      xvals = elem.getProperty<float>("u");
      yvals = elem.getProperty<float>("v");
      hasUV = true;
    }
    else if (elem.hasProperty("s") && elem.hasProperty("t")) {
      xvals = elem.getProperty<float>("s");
      yvals = elem.getProperty<float>("t");
      hasUV = true;
    }
    else if (elem.hasProperty("texture_u") && elem.hasProperty("texture_v")) {
      xvals = elem.getProperty<float>("texture_u");
      yvals = elem.getProperty<float>("texture_v");
      hasUV = true;
    }
    else if (elem.hasProperty("texture_s") && elem.hasProperty("texture_t")) {
      xvals = elem.getProperty<float>("texture_s");
      yvals = elem.getProperty<float>("texture_t");
      hasUV = true;
    }
    if (hasUV) {
      trimesh->uv = new float[trimesh->numVerts * 2];
      for (uint32_t i = 0; i < trimesh->numVerts; i++) {
        trimesh->uv[2 * i    ] = xvals[i];
        trimesh->uv[2 * i + 1] = yvals[i];
      }
    }
  }

  // Load index data.
  {
    std::vector<std::vector<int>> faces = plyIn.getFaceIndices<int>();

    uint32_t numTriangles = 0;
    for (const std::vector<int>& face : faces) {
      if (face.size() < 3) {
        continue;
      }
      numTriangles += uint32_t(face.size() - 2);
    }

    trimesh->numIndices = numTriangles * 3;
    trimesh->indices = new int[trimesh->numIndices];

    int* dst = trimesh->indices;
    for (const std::vector<int>& face : faces) {
      if (face.size() < 3) {
        continue;
      }
      uint32_t faceTris = miniply::triangulate_polygon(uint32_t(face.size()), trimesh->pos, trimesh->numVerts, face.data(), dst);
      dst += (faceTris * 3);
    }
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
  bool useHapply = false;

  const int kFilenameBufferLen = 16 * 1024 - 1;
  char* filenameBuffer = new char[kFilenameBufferLen + 1];
  filenameBuffer[kFilenameBufferLen] = '\0';

  std::vector<std::string> filenames;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--happly") == 0) {
      useHapply = true;
      continue;
    }

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

    TriMesh* trimesh = useHapply ? parse_file_with_happly(filename.c_str()) : parse_file_with_miniply(filename.c_str());
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
