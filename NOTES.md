Notes
=====

I considered doing endianness swaps as part of the extract calls, instead of
at data load time. I've put it off for now because (a) my personal use cases
so far almost always use all properties in each element anyway, so I'd still
end up converting all the data anyway; and (b) big-endian files are pretty
rare in practice. If you have a use case where this might help, please let me
know!
