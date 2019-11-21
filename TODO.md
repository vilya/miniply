TO DO
=====

Add a way to load a list property where we know up front what size every 
list will have. 
* Common case to know in advance that a file contains a triangle mesh or
  quad mesh, for example. 
* Should be able to calculate the byte size for the whole element up front 
  in this case and load it with a single fread call. 
* Extracting can be much more efficient in this case too, equivalent to 
  extracting a set of contiguous non-list properties. 

For list properties where the list sizes aren't known in advance, there 
are still some optimisations we can do:
* Avoid calling std::vector::resize repeatedly, profiling shows its very 
  expensive! Try resizing up front to (say) 3 elements per row & do our 
  own tracking of whether to grow it.
* Try scanning the element once to read the list counts, then rewinding to
  the start of the element and allocating all necessary space before parsing 
  it for real.
* Use a plain array instead of std::vector
  * Use malloc & realloc instead of new. 
* Use a paged array to hold the list data instead of a std::vector.
  * pool the pages so they can be reused for later elements.

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

Send a pull request adding miniply to the ply_io_benchmark suite. 