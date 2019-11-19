TO DO
=====

Provide a way to handle files where faces need triangulation, but we haven't
read the vertex data yet:
* Record the byte offset in the file of each element, as we reach it.
* Allow jumping directly to the start of any element with a known offset.
* Calculate offsets up to the first non-fixed-size element at startup.

Try out doing the endian swaps when we extract data, instead of when we load
it.
* We might avoid a lot of endian swaps that way.
* On the other hand, we might end up doing more if the same property is read 
  multiple times.

Add helper methods for checking standard element names.
