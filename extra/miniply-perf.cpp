// Copyright 2019 Vilya Harvey
#include "miniply.h"

#include <chrono>
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
  float* pos          = nullptr; // has 3*numVerts elements.
  float* normal       = nullptr; // if non-null, has 3 * numVerts elements.
  float* uv           = nullptr; // if non-null, has 2 * numVerts elements.
  uint32_t numVerts   = 0;

  // Per-index data
  int* indices        = nullptr; // has numIndices elements.
  uint32_t numIndices = 0; // number of indices = 3 times the number of faces.

  ~TriMesh() {
    delete[] pos;
    delete[] normal;
    delete[] uv;
    delete[] indices;
  }

  bool all_indices_valid() const {
    for (uint32_t i = 0; i < numIndices; i++) {
      if (indices[i] < 0 || uint32_t(indices[i]) >= numVerts) {
        return false;
      }
    }
    return true;
  }
};


static TriMesh* parse_file_with_miniply(const char* filename, bool assumeTriangles)
{
  miniply::PLYReader reader(filename);
  if (!reader.valid()) {
    return nullptr;
  }

  uint32_t faceIdxs[3];
  if (assumeTriangles) {
    miniply::PLYElement* faceElem = reader.get_element(reader.find_element(miniply::kPLYFaceElement));
    if (faceElem == nullptr) {
      return nullptr;
    }
    assumeTriangles = faceElem->convert_list_to_fixed_size(faceElem->find_property("vertex_indices"), 3, faceIdxs);
  }

  uint32_t propIdxs[3];
  bool gotVerts = false, gotFaces = false;

  TriMesh* trimesh = new TriMesh();
  while (reader.has_element() && (!gotVerts || !gotFaces)) {
    if (reader.element_is(miniply::kPLYVertexElement)) {
      if (!reader.load_element() || !reader.find_pos(propIdxs)) {
        break;
      }
      trimesh->numVerts = reader.num_rows();
      trimesh->pos = new float[trimesh->numVerts * 3];
      reader.extract_properties(propIdxs, 3, miniply::PLYPropertyType::Float, trimesh->pos);
      if (reader.find_normal(propIdxs)) {
        trimesh->normal = new float[trimesh->numVerts * 3];
        reader.extract_properties(propIdxs, 3, miniply::PLYPropertyType::Float, trimesh->normal);
      }
      if (reader.find_texcoord(propIdxs)) {
        trimesh->uv = new float[trimesh->numVerts * 2];
        reader.extract_properties(propIdxs, 2, miniply::PLYPropertyType::Float, trimesh->uv);
      }
      gotVerts = true;
    }
    else if (!gotFaces && reader.element_is(miniply::kPLYFaceElement)) {
      if (!reader.load_element()) {
        break;
      }
      if (assumeTriangles) {
        trimesh->numIndices = reader.num_rows() * 3;
        trimesh->indices = new int[trimesh->numIndices];
        reader.extract_properties(faceIdxs, 3, miniply::PLYPropertyType::Int, trimesh->indices);
      }
      else {
        uint32_t propIdx;
        if (!reader.find_indices(&propIdx)) {
          break;
        }
        bool polys = reader.requires_triangulation(propIdx);
        if (polys && !gotVerts) {
          fprintf(stderr, "Error: face data needing triangulation found before vertex data.\n");
          break;
        }
        if (polys) {
          trimesh->numIndices = reader.num_triangles(propIdx) * 3;
          trimesh->indices = new int[trimesh->numIndices];
          reader.extract_triangles(propIdx, trimesh->pos, trimesh->numVerts, miniply::PLYPropertyType::Int, trimesh->indices);
        }
        else {
          trimesh->numIndices = reader.num_rows() * 3;
          trimesh->indices = new int[trimesh->numIndices];
          reader.extract_list_property(propIdx, miniply::PLYPropertyType::Int, trimesh->indices);
        }
      }
      gotFaces = true;
    }
    reader.next_element();
  }

  if (!gotVerts || !gotFaces || !trimesh->all_indices_valid()) {
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

  bool assumeTriangles = false;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--assume-triangles") == 0) {
      assumeTriangles = true;
      break;
    }
  }

  std::vector<std::string> filenames;
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
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

    TriMesh* trimesh = parse_file_with_miniply(filename.c_str(), assumeTriangles);
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
