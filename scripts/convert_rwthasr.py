#!/usr/bin/python
# convert_rwthasr.py
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Copyright 2010 RWTH Aachen University. All rights reserved.
# Author: rybach@cs.rwth-aachen.de (David Rybach)

"""
Convert RWTH ASR CART examples to context_builder samples (text file format).
"""

import sys
import xml.sax
import optparse
import copy
import os.path
import gzip

class PropertiesDefinition:
  def __init__(self):
    self.hmm_state = {}
    self.boundary = {}
    self.central = {}
    self.phones = {}
        
class Example:
  def __init__(self):
    self.properties = {}
    self.nobs = 0
    self.data = []
    self.nrows = 0
    self.ncol = 0

class ExampleListParser (xml.sax.ContentHandler):
    
  def __init__(self):
    self.properties = PropertiesDefinition()
    self.examples = []
    
  def startDocument(self):
    self.buffer = ""
    self.fill_buffer = False
    self.parse_map = False
    self.cur_key = None
    self.cur_value = None
    self.cur_map = None
    self.cur_key_value_handler = None
    self.cur_key_handler = None
    self.cur_map_handler = None
    self.cur_prop_element = None
    self.cur_example = None
    
  def startElement(self, name, attr):
    if name == "properties-definition":
      self.cur_key_handler = self.propertyDefHandler
    elif name == "example":
      self.cur_example = Example()
      self.cur_example.nobs = int(attr["nObservations"])
    elif name == "properties":
      self.cur_key_handler = None
      self.cur_key_value_handler = self.setExampleProperty
    elif name == "key":
      self.buffer = ""
      self.fill_buffer = True
    elif name == "value":
      if self.parse_map:
        self.cur_key = attr["id"]
      self.buffer = ""
      self.fill_buffer = True
    elif name == "value-map":
      self.parse_map = True
      self.cur_map = {}
      self.cur_key_value_handler = self.handleValueMap
    elif name == "matrix-f64":
      self.buffer = ""
      self.fill_buffer = True
      self.cur_example.ncol = int(attr["nColumns"])
      self.cur_example.nrows = int(attr["nRows"])

  def endElement(self, name):
    if name == "properties-definition":
      self.cur_key_handler = None
    elif name == "example":
      self.examples.append(self.cur_example)
      self.cur_example = None
    elif name == "properties":
      self.cur_key_value_handler = None            
    if name == "key":
      self.cur_key = self.buffer.strip()
      self.fill_buffer = False
      if self.cur_key_handler:
        self.cur_key_handler(self.cur_key)
    elif name == "value":
      self.cur_value = self.buffer.strip()
      self.fill_buffer = False
      self.cur_key_value_handler(self.cur_key, self.cur_value)
      self.cur_key = self.cur_value = None
    elif name == "value-map":
        self.cur_map_handler(self.cur_map)
        self.cur_map = None
        self.parse_map = False
        self.cur_key_value_handler = None
    elif name == "matrix-f64":
      self.cur_example.data = self.buffer.split()
      self.fill_buffer = False
  
  def characters(self, c):
    if self.fill_buffer:
      self.buffer += c
      
  def handleValueMap(self, key, value):
    assert self.cur_map is not None
    self.cur_map[key] = value
  
  def propertyDefHandler(self, key):
    if key in [ "hmm-state", "boundary", "central" ]:
      self.cur_prop_element = key.replace("-", "_")
    elif key in [ "history[0]", "future[0]" ]:
      self.cur_prop_element = key.split("[")[0]
    else:
      raise Exception("unknown property: " + key)    
    self.cur_map_handler = self.setPropertyMember
      
  def setPropertyMember(self, map):
    setattr(self.properties, self.cur_prop_element, copy.copy(self.cur_map))
    
  def setExampleProperty(self, key, value):
    assert self.cur_example is not None
    self.cur_example.properties[key] = value
    

class SymbolTable:
  def __init__(self):
    self.symbols_ = {}
    self.keys_ = {}
    
  def set(self, map, ignore):
    for key, value in map.items():
      ikey = int(key)
      if value != ignore:
        assert not ikey in self.symbols_
        self.symbols_[int(key)] = value
      else:
        assert ikey == 0
  
  def add(self, phones):
    key = max(self.symbols_.keys()) + 1
    for p in phones:
      self.symbols_[key] = p
      self.keys_[p] = key
      key += 1
      
  def find(self, symbol):
    if not symbol in self.keys_:
      return -1
    return self.keys_[symbol]
  
  def read(self, filename):
    for line in file(filename):
      if not line.strip(): continue
      symbol, key = line.split()
      key = int(key)
      if key == 0:
        # skip epsilon
        continue
      self.symbols_[key] = symbol
      self.keys_[symbol] = key
      
  def write(self, filename):
    fp = file(filename, "w")
    fp.write(".eps 0\n")
    for key, value in self.symbols_.items():
      fp.write("%s %d\n" % (value, key))
      
  def symbols(self):
    return self.symbols_.values()


class Sample:
  def __init__(self, phone, state, left_ctxt, right_ctxt):
    self.phone = phone
    self.state = state
    self.left_context = left_ctxt
    self.right_context = right_ctxt
    self.initial = False
    self.final = False
    self.weight = 0
    self.sum = []
    self.sum2 = []
    
  def setData(self, nobs, ncol, nrow, data):
    self.weight = nobs
    assert nrow == 2
    dim = ncol
    self.sum = data[0:dim]
    self.sum2 = data[dim:]
    assert len(self.sum) == dim
    assert len(self.sum2) == dim
      
class Samples:
  def __init__(self):
    self.samples = []
    self.use_word_boundary = False
    
  def setUseWordBoundary(self, use_wb):
    self.use_word_boundary = use_wb
    
  def setExample(self, example):
    sample = Sample(example.properties["central"],
                    int(example.properties["hmm-state"]),
                    [example.properties["history[0]"]],
                    [example.properties["future[0]"]])
    if self.use_word_boundary:
      if example.properties["boundary"] == "begin-of-lemma":
        sample.initial = True
      elif example.properties["boundary"] == "end-of-lemma":
        sample.final = True
      elif example.properties["boundary"] == "single-phoneme-lemma":
        sample.initial = True
        sample.final = True
    sample.setData(example.nobs, example.ncol, example.nrows, example.data)
    self.samples.append(sample)
    
  def getGlobalProperties(self):
    dim = len(self.samples[0].sum)
    nl = len(self.samples[0].left_context)
    nr = len(self.samples[0].right_context)
    for s in self.samples:
      assert len(s.sum) == dim
      assert len(s.left_context) == nl
      assert len(s.right_context) == nr
    return dim, nl, nr
    
  def write(self, filename):
    dim, num_left_context, num_right_context = self.getGlobalProperties()
    fp = file(filename, "w")
    fp.write("1 %d %d %d\n" % (dim, num_left_context, num_right_context))
    for s in self.samples:
      self.writeSample(fp, s)
    
  def writeSample(self, fp, s):
    fp.write("%s %d %s %s %f %s %s\n" % (
             s.phone, s.state,
             " ".join(s.left_context),
             " ".join(s.right_context),
             s.weight,
             " ".join(s.sum), " ".join(s.sum2)))


class SymbolConverter:
  def __init__(self):
    self.mapping = {}
    self.initial_phones = set()
    self.final_phones = set()
    self.phone_classes = {}
    self.use_word_boundary = False
    self.non_wb = set()
    
  def setSymbols(self, symbols):
    self.symbols = symbols
    for s in self.symbols.symbols():
      if "@i" in s:
        self.initial_phones.add(s)
      if "@f" in s:
        self.final_phones.add(s)
      basePhone = s.split("@")[0]
      if basePhone != s:
        if not basePhone in self.phone_classes:
          self.phone_classes[basePhone] = []
        self.phone_classes[basePhone].append(s)
    
  def setUseWordBoundary(self, use_wb, non_wb):
    self.use_word_boundary = use_wb
    self.non_wb = non_wb
    self.initial_phones.update(non_wb)
    self.final_phones.update(non_wb)
    
  def mapSymbol(self, from_sym, to_sym):
    self.mapping[from_sym] = to_sym
    
  def convertSymbols(self, arr):
    for i in range(len(arr)):
      c = arr[i]
      if c in self.mapping:
        arr[i] = self.mapping[c]
        
  def isExcluded(self, sample):
    return sample.phone in self.non_wb 
          
  def convertContextSymbols(self, samples):
    for s in samples:
      if self.use_word_boundary and not self.isExcluded(s):
        self.mapWordBoundaryPhone(s) 
      self.convertSymbols(s.left_context)
      self.convertSymbols(s.right_context)
      
  def mapWordBoundaryPhone(self, sample):
    orig_phone = sample.phone
    if sample.initial:
      sample.phone += "@i"
    if sample.final:
      sample.phone += "@f"
    assert self.symbols.find(sample.phone) > 0
      
  def writeInitialPhones(self, filename):
    file(filename, "w").write("\n".join(self.initial_phones) + "\n")
  
  def writeFinalPhones(self, filename):
    file(filename, "w").write("\n".join(self.final_phones) + "\n")
    
  def writePhoneMap(self, filename):
    fp = file(filename, "w")
    for phone, mapped in self.phone_classes.items():
      for m in mapped:
        fp.write("%s %s\n" % (m, phone))
       

class Questions:
  
  def __init__(self):
    self.questions = {}
    
  def setInitialFinal(self, initial, final, all):
    self.questions["is_initial"] = initial
    self.questions["is_final"] = final
    self.questions["is_within"] = []
    boundary = initial.union(final)
    for p in all:
      if not p in boundary:
        self.questions["is_within"].append(p)
    
  def read(self, filename):
    for line in file(filename, "r"):
      items = line.split()
      self.questions[items[0]] = items[1:]
    
  def addMappedPhoens(self, phone_map):
    for q in self.questions:
      add = []
      for p in self.questions[q]:
        if p in phone_map:
          add += phone_map[p]
      self.questions[q] += add

  def write(self, filename):
    fp = file(filename, "w")
    for q in self.questions:
      fp.write("%s %s\n" % (q, " ".join(self.questions[q])))
            
    
def main(opts, args):
  outputdir = args[1]
  symbols = SymbolTable()
  symbols.read(opts.phonesymbols)
  parser = xml.sax.make_parser()
  handler = ExampleListParser()
  parser.setContentHandler(handler)
  sys.stderr.write("parsing %s\n" % args[0])
  if args[0].endswith(".gz"):
    fp = gzip.open(args[0], "rb")
  else:
    fp = file(args[0], "r")
  parser.parse(fp)
  props = handler.properties
  examples = handler.examples
  sys.stderr.write("read %d examples\n" % len(examples))
  samples = Samples()
  samples.setUseWordBoundary(opts.wordboundary)
  sys.stderr.write("converting to samples...\n")
  for e in examples:
    samples.setExample(e)
  converter = SymbolConverter()
  converter.setSymbols(symbols)
  converter.setUseWordBoundary(opts.wordboundary, set(opts.ci_phones))
  converter.mapSymbol(opts.empty_context, opts.boundary)
  sys.stderr.write("converting context symbols\n")
  converter.convertContextSymbols(samples.samples)
  samples.write(os.path.join(outputdir, opts.samples))
  # symbols = SymbolTable()
  # symbols.set(props.central, opts.empty_context)  
  if opts.wordboundary:
    sys.stderr.write("adding word boundary symbols\n")
    converter.writeInitialPhones(opts.initial_phones)
    converter.writeFinalPhones(opts.final_phones)
    converter.writePhoneMap(opts.phone_map)
    center_questions = Questions()
    center_questions.setInitialFinal(converter.initial_phones,
                                     converter.final_phones,
                                     symbols.symbols())
    center_questions.write(opts.center_questions)
    if opts.questions:
      sys.stderr.write("converting questions\n")
      questions = Questions()
      questions.read(opts.questions)
      questions.addMappedPhoens(converter.phone_classes)
      questions.write(opts.mapped_questions)
      # symbols.add(converter.initial_phones.union(converter.final_phones))
  # symbols.write(os.path.join(outputdir, opts.phonesymbols))
      
      
    

if __name__ == "__main__":
  optparser = optparse.OptionParser(\
      usage = "USAGE: %prog [OPTIONS] <input-file> <output-dir>")
  optparser.add_option("-b", "--boundary", dest="boundary",
                       default="si",
                       help="replacement phone for boundary symbol")
  optparser.add_option("-e", "--empty_context", dest="empty_context",
                       default="#",
                       help="empty context in input file")
  optparser.add_option("-x", "--ci_phones", dest="ci_phones",
                       default=["si"], action="append",
                       help="list of context indepent phones")
  optparser.add_option("-p", "--phone_syms", dest="phonesymbols",
                       default="phones.sym",
                       help="phone symbol table filename")
  optparser.add_option("-s", "--samples", dest="samples",
                       default="samples.txt",
                       help="samples filename")
  optparser.add_option("-w", "--wordboundary", dest="wordboundary",
                       default=False, action="store_true",
                       help="enable word boundary information")
  optparser.add_option("-i", "--initials", dest="initial_phones",
                       default="initial_phones",
                       help="intial phone list filename")
  optparser.add_option("-f", "--finals", dest="final_phones",
                       default="final_phones",
                       help="final phone list filename")
  optparser.add_option("-m", "--phone_map", dest="phone_map",
                       default="phone_map",
                       help="word boundary to normal phone map filename")
  optparser.add_option("-q", "--questions", dest="questions",
                       default="",
                       help="original question file")
  optparser.add_option("-n", "--mapped_questions", dest="mapped_questions",
                       default="mapped_questions.txt",
                       help="mapped questions filename")
  optparser.add_option("-c", "--center_questions", dest="center_questions",
                       default="center_questions.txt",
                       help="central questions filename") 
                       
                       
  opts, args = optparser.parse_args()
  if len(args) < 2:
    optparser.print_usage()
    sys.exit(1)
  main(opts, args)
  
      
    
                                       