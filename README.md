miniply - A simple and fast c++11 library for loading PLY files
===============================================================

miniply is a small, fast and easy-to-use library for parsing
[PLY](http://paulbourke.net/dataformats/ply/). files, written in c++11. The
entire parser is a single header and cpp file which you can copy into your own
project.

Features
--------

- *Small*: just a single .h and .cpp file which you can copy into your project.
* *Fast*: loads all 8929 PLY files from the pbrt-v3-scenes repository in under 9
  seconds total - an average parsing time of less than 1 millisecond per file!
- *Complete*: parses ASCII, binary little-endian and binary big-endian
  versions of the file format (binary loading assumes you're running on a 
  little-endian CPU).
- Provides helper methods for getting standard mesh properties (position and 
  other vertex attributes; indices for faces).
- Can optionally triangulate polygons with more than 3 vertices.

Note that miniply does not support writing PLY files, only reading them.


Getting started
---------------

The basic usage model for this library is:

1. Construct a PLYReader object.
2. Iterate over the elements, processing them as they occur.
3. For any elements that you're interested in:
   * Call `load_element()`
   * Use the various `extract_X` methods to get the data you're interested in.

Constructing a PLYReader will open the PLY file and read the header.

You can only iterate forwards over the elements in the file (at present). You
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

There are several other C/C++ libraries available for parsing PLY files. If
miniply doesn't meet your needs, perhaps one of these will:

* [happly](https://github.com/nmwsharp/happly)
* [tinyply](https://github.com/ddiakopoulos/tinyply)
* [rply](http://w3.impa.br/~diego/software/rply/)

Each of these libraries provides a slightly different paradigm for extracting
data from the PLY files:
* *happly* parses the whole file and makes all the data available for random 
  access by your code. This is probably the easiest to use, but can require a
  lot of work to translate the data into your own structures.
* *tinyply* parses the file header so that it knows what data is available,
  then asks you to specify which elements and properties you want. Loading
  fills in the data for those and ignores everything else. Your code then has
  random access to that data only.
* *rply* requires you to provide callback functions for each of the elements 
  you're interested in and it invokes those as it parses (I think, still 
  working on a sample using this one).

All of the above provide the ability to write PLY files as well, which miniply
does not do.
