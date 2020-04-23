miniply
===============================================================

<p align="center">
<img src="https://github.com/vilya/miniply/blob/master/miniPLY3d_logo-cropped.png" width="744"> 
</p>

A fast and easy-to-use library for parsing 
[PLY files](http://paulbourke.net/dataformats/ply/), in a single c++11 header and
cpp file with no external dependencies, ready to drop into your project.


Features
--------

- **Fast**: loads all 8929 PLY files from the 
  [pbrt-v3-scenes repository](https://www.pbrt.org/scenes-v3.html)
  in under 9 seconds total - an average parsing time of less than 1 millisecond
  per file!
- **Small**: just a single .h and .cpp file which you can copy into your project.
- **Complete**: parses ASCII, binary little-endian and binary big-endian
  versions of the file format (binary loading assumes you're running on a 
  little-endian CPU).
- **Cross-platform**: works on Windows, macOS and Linux.
- Provides helper methods for getting **standard vertex and face properties**.
- Can **triangulate polygons** as they're loaded.
- **Fast path** for models where you know every face has the same fixed number of vertices
- **MIT license**

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
has built-in support for this:

*Note that if you know in advance that your PLY file only contains triangles, there is a much faster way to load it. See below for details.*

```cpp
// Very basic triangle mesh struct, for example purposes
struct TriMesh {
  // Per-vertex data
  float* pos     = nullptr; // has 3 * numVerts elements.
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

  uint32_t indexes[3];
  bool gotVerts = false, gotFaces = false;

  TriMesh* trimesh = new TriMesh();
  while (reader.has_element() && (!gotVerts || !gotFaces)) {
    if (reader.element_is(miniply::kPLYVertexElement) && reader.load_element() && reader.find_pos(indexes)) {
      trimesh->numVerts = reader.num_rows();
      trimesh->pos = new float[trimesh->numVerts * 3];
      reader.extract_properties(indexes, 3, miniply::PLYPropertyType::Float, trimesh->pos);
      if (reader.find_texcoord(indexes)) {
        trimesh->uv = new float[trimesh->numVerts * 2];
        reader.extract_properties(indexes, 2, miniply::PLYPropertyType::Float, trimesh->uv);
      }
      gotVerts = true;
    }
    else if (reader.element_is(miniply::kPLYFaceElement) && reader.load_element() && reader.find_indices(indexes) {
      bool polys = reader.requires_triangulation(propIdx);
      if (polys && !gotVerts) {
        fprintf(stderr, "Error: need vertex positions to triangulate faces.\n");
        break;
      }
      if (polys) {
        trimesh->numIndices = reader.num_triangles(indexes[0]) * 3;
        trimesh->indices = new int[trimesh->numIndices];
        reader.extract_triangles(indexes[0], trimesh->pos, trimesh->numVerts, miniply::PLYPropertyType::Int, trimesh->indices);
      }
      else {
        trimesh->numIndices = reader.num_rows() * 3;
        trimesh->indices = new int[trimesh->numIndices];
        reader.extract_list_property(indexes[0], miniply::PLYPropertyType::Int, trimesh->indices);
      }
      gotFaces = true;
    }
    if (gotVerts && gotFaces) {
      break;
    }
    reader.next_element();
  }

  if (!gotVerts || !gotFaces) {
    delete trimesh;
    return nullptr;
  }

  return trimesh;
}
```

For a more complete example, see 
[extra/miniply-perf.cpp](https://github.com/vilya/miniply/blob/master/extra/miniply-perf.cpp)


Loading from a PLY file known to only contain triangles
-------------------------------------------------------

Loading the vertex indices for each face from a variable length list is a bit
wasteful if you know ahread of time that your PLY file only contains triangles.
With `miniply` you can take advantage of this knowledge to get a massive
reduction in the loading time for the file.

The idea is to replace the single list property, which miniply has to treat as
variable-sized, with a set of fixed-size properties. There will be one
property corresponding to the item count for each list (which we will ignore
during  loading, because we know it will always be three), followed by three
new  properties (one for each list index). You do this by calling 
`convert_list_to_fixed_size()` on the face element at some point prior to
loading its data.

Doing this allows `miniply` to use its far more efficient code path for loading 
fixed-size elements instead. This can cut loading times by more than half!

```cpp
// Note: using the same TriMesh class as the example above, omitting it here
// for the sake of brevity.

TriMesh* load_trimesh_from_triangles_only_ply(const char* filename)
{
  miniply::PLYReader reader(filename);
  if (!reader.valid()) {
    return nullptr;
  }

  uint32_t faceIdxs[3];
  miniply::PLYElement* faceElem = reader.get_element(reader.find_element(miniply::kPLYFaceElement));
  if (faceElem == nullptr) {
    return nullptr;
  }
  faceElem->convert_list_to_fixed_size(faceElem->find_property("vertex_indices"), 3, faceIdxs);

  uint32_t indexes[3];
  bool gotVerts = false, gotFaces = false;

  TriMesh* trimesh = new TriMesh();
  while (reader.has_element() && (!gotVerts || !gotFaces)) {
    if (reader.element_is(miniply::kPLYVertexElement) && reader.load_element() && reader.find_pos(indexes)) {
      // This section is the same as the example above, not repeating it here.
    }
    else if (!gotFaces && reader.element_is(miniply::kPLYFaceElement) && reader.load_element()) {
      trimesh->numIndices = reader.num_rows() * 3;
      trimesh->indices = new int[trimesh->numIndices];
      reader.extract_properties(faceIdxs, 3, miniply::PLYPropertyType::Int, trimesh->indices);
      gotFaces = true;
    }
    if (gotVerts && gotFaces) {
      break;
    }
    reader.next_element();
  }

  if (!gotVerts || !gotFaces) {
    delete trimesh;
    return nullptr;
  }

  return trimesh;
}
```

To recap, the differences from the previous example are:
1. We're calling `faceElem->convert_list_to_fixed_size()` up front.
2. In the section which processes the face element, we're calling 
   `reader.extract_properties()` to get the index data instead of 
   `reader.extract_triangles()` or `reader.extrat_list_property()`.


Loading triangle strips
-----------------------

Some PLY files represent faces as a set of triangle strips, stored in a
list-valued property. If you want to load the triangle strip indices as-is,
you can do that in the same was as you would read any other list-valued
property:

```cpp
if (reader.element_is("tristrips") && reader.load_element()) {
  uint32_t propIdx = reader.element()->find_property("vertex_indices");
  int arraySize = reader.sum_of_list_counts(propIdx);
  int allTriStrips = new int[arraySize];
  reader.extract_list_property(propIdx, miniply::PLYPropertyType::Int, allTriStrips);

  // ... do stuff with allTriStrips ...
}
```

Here the `allTriStrips` array will contain all of the indices from all of the
lists in the property, one after the other. If there are any restart markers
(see below) then these will be included in the array too.

Note that indices from one row will be adjacent to indices from the next row
in `allTriStrips`. If you want to use the row start & end points to implicitly
indicate the start and end of a triangle strip, you'll have to do that in your
own code (`PLYReader::get_list_counts` can help with this).


Triangle strip restart markers
------------------------------

Another possibility, already observed in the wild, is that each row in the PLY
file can have multiple triangle strips separated by a special marker index
which we call the restart value (loosely following OpenGL's terminology).

Miniply has built-in functionality for de-stripifying which handles marker
values. *See the next section for example code.* This functionality treats the
end of each row as an implicit restart marker, if there isn't one explicitly
present in the data. It distinguishes between three ways of laying out the 
triangle strip indices. In order from fastest to load, to slowest, they are:

1. `PLYListRestart::None`: Implicit restart markers only, no explicit restart
   markers. This means there's exactly one triangle strip per row. This is the
   most efficient form to load.

2. `PLYListRestart::Terminator`: Explicit restart markers only, no implicit
   restart markers. This allows multiple triangle strips per row. It means every
   triangle strip *must* end with a restart marker even if it's the last one in a
   row.

3. `PLYListRestart::Separator`: Both implicit and explicit restart markers
   allowed. This allows multiple triangle strips per row. It means the last
   triangle strip in a row does not  have to end with a restart marker, but all
   earlier strips in the row do.  This is a superset of the other two layouts, so
   it supports the widest range of inputs; but it's also the least efficient to
   load.

Note that miniply's restart value support assumes there is a *single* restart
value. It cannot correctly handle cases where, for example, any negative
number is supposed to indicate a restart. For cases like that (which are
hopefully rare!) you'll need to write your own de-stripifying code.


Loading and de-stripifying triangle strips
------------------------------------------

The `PLYReader` class has two methods for loading a triangle strip and
converting it to an indexed triangle set:

* `PLYReader::num_triangles_in_strips` which tells you how many triangles
  you'll end up with so that, e.g., you can allocate enough space for them all.

* `PLYReader::extract_triangle_strips` which de-stripifies the triangle strips
  into an array that you provide.

The example code here shows what you could add to the "loading a triangle
mesh" example (above) to handle triangle strips:

```cpp
// ...
else if (!gotFaces && reader.element_is("tristrips") && reader.load_element() && reader.find_indices(indexes)) {
  int restartVal = -1;
  trimesh->numIndices = reader.num_triangles_in_strips(indexes[0], PLYListRestart::Separator, PLYPropertyType::Int, &restartVal) * 3;
  trimesh->indices = new int[trimesh->numIndices];
  reader.extract_triangle_strips(indexes[0], PLYPropertyType::Int, trimesh->indices, PLYListRestart::Separator, PLYPropertyType::Int, &restartVal);
  gotFaces = true;
}
// ...
```

Note that this example uses `PLYListRestart::Separator`, which supports the
widest range of files at the expense of (some) load-time performance. If you
are only loading files where you know you can use one of the other list 
restart types, doing so will improve loading performance.


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

Overall `miniply` is between 2 and 8 times faster than all the other parsers I've 
tested, for that workload (creating a simple poly mesh from each ply file).

See also Maciej Halber's [ply_io_benchmark](https://github.com/mhalber/ply_io_benchmark) 
(thanks to Dimitri Diakopoulos for the pointer!) for more performance 
comparisons.


Other PLY parsing libraries
---------------------------

If miniply doesn't meet your needs, perhaps one of these other great PLY parsing 
libraries will?

* [Happly](https://github.com/nmwsharp/happly)
* [tinyply](https://github.com/ddiakopoulos/tinyply)
* [RPly](http://w3.impa.br/~diego/software/rply/)
* [msh_ply](https://github.com/mhalber/msh/blob/master/msh_ply.h)

In particular these all support writing as well as reading, whereas
`miniply` only supports reading.


Feedback, suggestions and bug reports
-------------------------------------

GitHub issues: https://github.com/vilya/miniply/issues

If you're using miniply and find it useful, drop me an email - I'd love to
know about it!
