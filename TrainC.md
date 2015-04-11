# Introduction #

TrainC build compact context dependency transducers for WFST-based speech recognition directly from acoustic training data.

Instead of the conventional phonetic decision-tree growing
followed by finite state transducer compilation, this approach incorporates the phonetic context splitting directly into the transducer construction. The objective function of the split optimization is augmented with a regularization term that measures the number of transducer states introduced by a split.

A detailed description of the approach as well as experimental results can be found in D. Rybach and M. Riley. [Direct Construction of Compact Context-Dependency Transducers From Data](http://www-i6.informatik.rwth-aachen.de/publications/download/671/Rybach-Riley--DirectConstructionofCompactContext-DependencyTransducersFromData--2010.pdf). In [Interspeech](http://www.interspeech2010.org), pages 218-221, Makuhari, Japan, September 2010

# Details #
  * [Options](Options.md)
  * [Input File Formats](InputFormats.md)
  * [Output File Formats](OutputFormats.md)
  * [Examples](Examples.md)