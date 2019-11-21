TO DO
=====

Optimise loading of list properties, for the cases where we can't convert them
to a set of fixed size properties in advance:
* Avoid calling std::vector::resize repeatedly, profiling shows its very 
  expensive! Try resizing up front to (say) 3 elements per row & do our 
  own tracking of whether to grow it.
* Try scanning the element once to read the list counts, then rewinding to
  the start of the element and allocating all necessary space before parsing 
  it for real. Compare the speed this way with the existing way where we 
  reallocate while reading.
* Try using a stretchy-buffer instead of std::vector
  * Use malloc instead of new, so that we can use realloc to grow the array.

Provide a way to handle files where faces need triangulation, but we haven't
read the vertex data yet:
* Record the byte offset in the file of each element, as we reach it.
* Allow jumping directly to the start of any element with a known offset.
* Calculate offsets up to the first non-fixed-size element at startup.

Flesh out the API docs.

Set up continuous integration 
- Github Actions?
- Travis?

Automated tests to ensure we can parse all file and data types correctly.

Option to generate graphs of the miniply-perf results as SVG files. 

Send a pull request adding miniply to the
[ply_io_benchmark](https://github.com/mhalber/ply_io_benchmark) suite. 
