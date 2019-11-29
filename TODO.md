TO DO
=====

Try mmapping the whole file, so we never have to worry about refilling the
input buffer.
* Don't need to worry about copying values into internal storage that way
  either, can copy them directly into user-provided buffers as part of the
  extraction step instead.


Provide a way to handle files where faces need triangulation, but we haven't
read the vertex data yet:
* Record the byte offset in the file of each element, as we reach it.
* Allow jumping directly to the start of any element with a known offset.
* Calculate offsets up to the first non-fixed-size element at startup.


Expand the docs.


Set up continuous integration 
- Github Actions?
- Travis?


Automated tests to ensure we can parse all file and data types correctly.


Option to generate graphs of the miniply-perf results as SVG files?


Send a pull request adding miniply to the
[ply_io_benchmark](https://github.com/mhalber/ply_io_benchmark) suite. 
