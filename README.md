miniply - A simple and fast c++11 library for loading PLY files
===============================================================

Features
--------

* Supports ASCII, Binary Little-Endian and Binary Big-Endian formats
  * Assumes you're running on a little-endian CPU.

* Provides helper methods for extract common pieces of data such as vertex
  positions and triangle indices.

* Includes code to triangulate polygons with more than three vertices.

* *Fast*: loads all 8929 PLY files from the pbrt-v3-scenes repository in under 9
  seconds total.


Getting started
---------------

The basic usage model for this library is:

1. Construct a PLYReader object
2. Iterate over the elements, processing them as they occur.
3. For any elements that you're interested in:
   * Call `load_element()`
   * Use the various `extract_X` methods to get the data you're interested in.


Constructing a PLYReader will open the PLY file and read the header.

You can only iterate forward over the elements in the file (at present). You
cannot jump back to an earlier element. You can skip forward to the next
element, but we may have to read the data in the current element to figure out
where it ends and the next element begins.
