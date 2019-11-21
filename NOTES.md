Notes
=====

I considered doing endianness swaps as part of the extract calls, instead of
at data load time. I've put it off for now because (a) my personal use cases
so far almost always use all properties in each element anyway, so I'd still
end up converting all the data anyway; and (b) big-endian files are pretty
rare in practice. If you have a use case where this might help, please let me
know!

* I have a use case for this now. The ply_io_benchmark tests only extract
  vertex positions, ignoring any additional vertex attributes. Doing the
  conversion on demand only will help improve miniply's performance in the
  benchmarks! (And also for that use case too, of course. But... benchmarks!)

Reading variable-size properties (aka lists) is pretty slow. If we know in
advance that all rows for a list property always have the same size, we can
improve performance *massively* by replacing the list property with a set of
fixed-size properties. That's what the
`PLYElement::convert_list_to_fixed_size()` method is for.