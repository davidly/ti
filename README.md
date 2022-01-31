# ti

Timing Info is a Windows command-line app that shows resource information about a process. 
TI spawns a process then awaits its completion. 
It's like a million other similar tools yet slightly different.

Usage: ti app [arg1] [arg2] [...]
  Timing Info starts a process then displays information about that process after it exits

Sample output for a 32-core machine:

    process stats for C:\ntbin\ff.exe
    peak working set:       20,631,552
    final working set:          28,672
    kernel CPU:                 36,468
    user CPU:                   13,203
    total CPU:                  49,671
    core efficiency:            23.89%
    average cores used:           7.65
    elapsed time:                6,497

Build using a standard Visual Studio VC 64 or 32 bit command prompt:

    cl /nologo ti.cxx /EHac /Zi /D_AMD64_ /link /OPT:REF
    
or

    cl /nologo ti.cxx /EHac /Zi /Fe: /link /OPT:REF
