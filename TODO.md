TO DO
=====

Provide a way to handle files where faces need triangulation, but we haven't
read the vertex data yet:
* Record the byte offset in the file of each element, as we reach it.
* Allow jumping directly to the start of any element with a known offset.
* Calculate offsets up to the first non-fixed-size element at startup.

Flesh out the API docs.

Automated tests to ensure we can parse all file and data types correctly.