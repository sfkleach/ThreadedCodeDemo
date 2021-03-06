# ThreadedCodeDemo

This repo demonstrates a variety of implementations of threaded-code in C++ and Rust. As this is 
intended for a online presentation, I chose a very simple programming language to demonstrate the 
techniques of direct and subroutine threaded code. This is the elegant and notorious Brainfuck 
language.

I include a direct-threaded implementation in C++, exploiting the unique jump-to-label feature of GNU C/C++. 
Based on my previous experiments, I was pretty confident this would have performance quite close to the 
baseline, which appears to be the case. 

And then I include a subroutine-threaded implementation in C++ and also in Rust. I crudely benchmark the 
performance of these using the `bsort.bf` program. This came from the fabulous brainfluff site and was
written by Daniel Cristofani (licensed under Creative Commons). It performs a bubble-sort on its input, 
which is gratifyingly expensive, and makes running timing tests easy and giving stable results. Also, 
the overheads of the I/O do not dominate.

The baseline is provided by bsort.c, which is a direct-to-C implementation of `bsort.bf`. Rather neatly
I generated this from the `bsort.bf` code using `dbf2c.bf`, another of Daniel's incredible programs, which 
translates Brainfuck source into C. This I compiled into C (with -O) to give a rough sense of how much
overhead the different interpreters have - you can see the output as `bsort.c`.

Here's a short table of results, sorting the `Makefile` as our example text, on my laptop.
```
Baseline         100%     989ms   (100% by definition)
Direct           114%    1128ms
Subroutine C++   355%    3511ms
Subroutine Rust  333%    3297ms
```

As I have been around this loop before with Ginger, these were roughly the results I was expecting. I have
seen various comments that researchers find very little difference between direct-and-subroutine threading.
However I have never duplicated that - I wonder if the difference comes from the fact that I am restricting 
myself to portable implementations. If anyone knows, drop me a line :)

# Licenses

The C++, Rust and Makefile programs are licensed under the GPL 3.0, which is included as part of this archive. 
Two files are specifically not covered by this license and must be additionally attributed.

The programs `head.bf`, `bsort.bf` and `dbf2c.bf` are copyright (c) 2016 by Daniel Cristofani and included in this archive
for convenience only. They are licensed under 
[a Creative Commons Attribution-ShareAlike 4.0 International License](https://creativecommons.org/licenses/by-sa/4.0/). 
I have changed the file-extension used from `.b` to `.bf` to avoid naming clashes. The originals can be found
[on Daniel's website](http://www.brainfuck.org/).

The program `hello.bf` appears on many web sites, including Wikipedia, without attribution and at this point I 
believe it is in the public domain.
