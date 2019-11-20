miniply - A simple and fast c++11 library for loading PLY files
===============================================================

miniply is a small, fast and easy-to-use library for parsing
[PLY files](http://paulbourke.net/dataformats/ply/), written in c++11. The
entire parser is a single header and cpp file which you can copy into your own
project.


Features
--------

- *Small*: just a single .h and .cpp file which you can copy into your project.
* *Fast*: loads all 8929 PLY files from the [pbrt-v3-scenes repository](https://www.pbrt.org/scenes-v3.html)
  in under 9 seconds total - an average parsing time of less than 1 millisecond per file!
- *Complete*: parses ASCII, binary little-endian and binary big-endian
  versions of the file format (binary loading assumes you're running on a 
  little-endian CPU).
- Provides helper methods for getting standard mesh properties (position and 
  other vertex attributes; indices for faces).
- Can optionally triangulate polygons with more than 3 vertices.
- MIT licensed.

Note that miniply does not support *writing* PLY files, only reading them.


Getting started
---------------

* Copy `miniply.h` and `miniply.cpp` into your project.
* Add `#include <miniply.h>` wherever necessary.

The CMake file that you see in this repo is purely for building the `miniply-info`
and `miniply-perf` command line tools in the `extra` folder; it isn't required if
you're just using the library in your own project.


General use
-----------

The general usage model for this library is:

1. Construct a `PLYReader` object. This will open the PLY file and read the header.
2. Call `reader.valid()` to check that the file was opened successfully.
3. Iterate over the elements using something like 
   `for (; reader.has_element(); reader.next_element()) { ... }`, calling 
   `reader.element_is()` to check whether the current element is one that you want
   data from.
4. For any elements that you're interested in:
   * Call `reader.load_element()`.
   * Use some combination of the `extract_columns()`, `extract_list_column()` and 
     `extract_triangles()` methods to get the data you're interested in.

You can only iterate forwards over the elements in the file (at present). You cannot
jump back to an earlier element. 

You can skip forward to the next element simply by calling `next_element()` without 
having called `load_element()` yet. This will be very efficient if the current element
is fixed-size. If the current element contains any list properties then we will have to
scan through all of the element data to find where it finishes and the next element 
starts, which is not as efficient (although the library will do it's best!).


Loading header info from a PLY file
-----------------------------------

This function (taken directly from 
[extra/miniply-info.cpp](https://github.com/vilya/miniply/blob/master/extra/miniply-info.cpp)) 
shows how you can get the header from a .ply file:

```cpp
bool print_ply_header(const char* filename)
{
  miniply::PLYReader reader(filename);
  if (!reader.valid()) {
    fprintf(stderr, "Failed to open %s\n", filename);
    return false;
  }

  printf("ply\n");
  printf("format %s %d.%d\n", kFileTypes[int(reader.file_type())],
         reader.version_major(), reader.version_minor());
  for (uint32_t i = 0, endI = reader.num_elements(); i < endI; i++) {
    const miniply::PLYElement* elem = reader.get_element(i);
    printf("element %s %u\n", elem->name.c_str(), elem->count);
    for (const miniply::PLYProperty& prop : elem->properties) {
      if (prop.countType != miniply::PLYPropertyType::None) {
        printf("property list %s %s %s\n", kPropertyTypes[uint32_t(prop.countType)],
               kPropertyTypes[uint32_t(prop.type)], prop.name.c_str());
      }
      else {
        printf("property %s %s\n", kPropertyTypes[uint32_t(prop.type)], prop.name.c_str());
      }
    }
  }
  printf("end_header\n");

  return true;
}
```


Loading a triangle mesh
-----------------------

Polygons in PLY files are ordinarily stored in a list property, meaning that each 
polygon is a variable-length list of vertex indices. If the mesh representation in
your program is triangles-only, you will need to triangulate the faces. `miniply` 
has built-in support for this and this example code, taken from 
[extra/miniply-perf](https://github.com/vilya/miniply/blob/master/extra/miniply-perf.cpp)
with only minor simplifications, shows how to use it:

```cpp
// Very basic triangle mesh struct, for example purposes
struct TriMesh {
  // Per-vertex data
  float* pos     = nullptr; // has 3 * numVerts elements.
  float* normal  = nullptr; // if non-null, has 3 * numVerts elements.
  float* uv      = nullptr; // if non-null, has 2 * numVerts elements.
  uint32_t numVerts   = 0;

  // Per-index data
  int* indices   = nullptr;
  uint32_t numIndices = 0; // number of indices = 3 times the number of triangles.
};


TriMesh* load_trimesh_from_ply(const char* filename)
{
  miniply::PLYReader reader(filename);
  if (!reader.valid()) {
    return nullptr;
  }

  TriMesh* trimesh = new TriMesh();
  bool gotVerts = false;
  bool gotFaces = false;
  while (reader.has_element() && (!gotVerts || !gotFaces)) {
    if (!gotVerts && reader.element_is(miniply::kPLYVertexElement)) {
      if (!reader.load_element()) {
        break;
      }
      uint32_t propIdxs[3];
      if (!reader.find_pos(propIdxs)) {
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
```


History
-------

I originally wrote the code that became miniply as part of
[minipbrt](https://github.com/vilya/minipbrt). I looked at several existing
PLY parsing libraries first, but (a) I really wanted to keep minipbrt dependency
free; and (b) I already had a lot of parsing code lying around just begging to be
reused. In the end, the code I wrote for minipbrt seemed complete enough to be 
worth publishing as a standalone library too. Since then I've refined the API,
improved performance and reduced memory usage. I hope you like the result. :-)


Performance
-----------

The [ply-parsing-perf](https://github.com/vilya/ply-parsing-perf/) repo has a
detailed performance comparison between miniply and a number of other ply parsing
libraries. 

Overall `miniply` is between 2 and 8 times faster than all the other parsers for 
that test workload (the test creates a simple poly mesh from each ply file).


Other PLY parsing libraries
---------------------------

There are several other C/C++ libraries available for parsing PLY files. If
miniply doesn't meet your needs, perhaps one of these will:

* [Happly](https://github.com/nmwsharp/happly)
* [tinyply](https://github.com/ddiakopoulos/tinyply)
* [RPly](http://w3.impa.br/~diego/software/rply/)

Each of these libraries provides a slightly different paradigm for extracting
data from the PLY files:
* *Happly* parses the whole file and makes all the data available for random 
  access by your code. This is probably the easiest to use, but can require a
  lot of work to translate the data into your own structures.
* *tinyply* parses the file header so that it knows what data is available,
  then asks you to specify which elements and properties you want. Loading
  fills in the data for those and ignores everything else. Your code then has
  random access to that data only.
* *RPly* requires you to provide callback functions for each of the elements 
  you're interested in and it invokes those as it parses. You can provide
  two context parameters (one void pointer, one long int) for each callback
  which can help you determine where to store the data.

All of the above provide the ability to write PLY files as well, which miniply
does not do.


Feedback, suggestions and bug reports
-------------------------------------

GitHub issues: https://github.com/vilya/miniply/issues

If you're using miniply and find it useful, drop me an email - I'd love to know about it!
