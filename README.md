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

Note that miniply only supports reading, not writing.


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


History
-------

I originally wrote the code that became miniply as part of
[minipbrt](https://github.com/vilya/minipbrt). I looked at several existing
PLY parsing libraries first, but I really wanted to keep minipbrt dependency
free. I also noticed that I could reuse a lot of my existing text parsing code
if I wrote my own, so that was enough to convince me. In the end, the code I
wrote for minipbrt seemed complete enough to be worth publishing as a
standalone library too.

This version has a slightly expanded API surface commpared to the version in
minipbrt, although they're pretty similar otherwise.


Other PLY parsing libraries
---------------------------

There are several other libraries available for parsing PLY files. If miniply
doesn't meet your needs, perhaps one of these will:

* [happly](https://github.com/nmwsharp/happly)
* [rply](http://w3.impa.br/~diego/software/rply/)
* [tinyply](https://github.com/ddiakopoulos/tinyply)

Each of these libraries provides a slightly different paradigm for extracting data from the PLY files:
* *happly* parses the whole file and makes all the data available for random access by your code.
* *tinyply* requires you to specify which elements and properties you're interested in up front; parsing fills in data for those and ignores everything else. Your code then has random access to that data only.
* *rply* requires you to provide callback functions for each of the elements you're interested in and it invokes those as it parses.
* *miniply* iterates over the elements in the file, giving you the option to load data for the current element only. You only ever have access to the current element, no random access.

