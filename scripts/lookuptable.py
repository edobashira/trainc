#!/usr/bin/python
# lookuptable.py
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
Create a HMM state lookup table from context_builder state log file.
"""

import optparse
import re
import sys


class SymbolTable:
  def __init__(self):
    self.dict_ = {}
    
  def read(self, filename):
    for line in file(filename):
      if not line.strip(): continue
      sym, key = line.split()
      self.dict_[sym] = int(key)
  
  def find(self, symbol):
    return self.dict_.get(symbol, -1)
  

class HmmStateModels:
  def __init__(self):
    self.center_idx_ = []
    self.models_ = []
    self.model_syms_ = []
    self.ci_phones = set()
    self.name_re = re.compile("(.*)_([0-9]+)\.[0-9]+")
    
  def setModelSymbols(self, symbols):
    self.model_syms_ = symbols
    
  def setCiPhones(self, ci_phones):
    self.ci_phones = set(ci_phones)
    
  def setEmptyContext(self, empty_context):
    self.empty_context = set(empty_context)
    
  def parseLogFile(self, filename):
    group_re = re.compile("([0-9\-]+)={([^}]*)}")
    for line in file(filename):
      items = line.split()
      state_name = items[0]
      context = group_re.findall(line)
      assert len(context) == 3
      assert int(context[0][0]) == -1
      assert int(context[1][0]) == 0
      assert int(context[2][0]) == 1
      self._addModel(state_name, [ i[1] for i in context ])
  
  def _addModel(self, name, context):
    center, state = self.name_re.match(name).groups()
    state = int(state) - 1
    center_phones = context[1].split()
    hmm_idx = self._addHmmIndex(center_phones, state)
    model_idx = self.model_syms_.find(name)
    assert model_idx > 0
    model_idx -= 2 # .eps and .wb
    left_context = set(context[0].split())
    right_context = set(context[2].split())
    if left_context.issubset(self.ci_phones):
      left_context = self.empty_context
    if right_context.issubset(self.ci_phones):
      right_context = self.empty_context
    if set(center_phones).issubset(self.ci_phones):
      left_context = right_context = set()
    self.models_[hmm_idx][model_idx] = (left_context, right_context)
    
  def find(self, left, center, right, state):
    idx = self.center_idx_[state].get(center, [])
    found = -1
    for i in idx:
      models = self.models_[i]
      for m in models:
        # print "m", models[m][0], models[m][1], (bool)(left or right or models[m][0] or models[m][1])
        if not (left or right or models[m][0] or models[m][1]):
          assert found < 0
          found = m 
        elif left in models[m][0] and right in models[m][1]:
          assert found < 0
          found = m
    # print found, left, center, right, state
    # print idx
    return found
       
  def _addHmmIndex(self, center_phones, state):
    while len(self.center_idx_) <= state:
      self.center_idx_.append({})
    idx = len(self.models_)
    self.models_.append({})
    for c in center_phones:
      if not c in self.center_idx_[state]:
        self.center_idx_[state][c] = []
      self.center_idx_[state][c].append(idx)
    return idx
    
    
class AllophoneState:
  symbol_re = re.compile("(.*){([^+]+)\+([^}]+)}(@i)?(@f)?\.([0-9]+)")
  empty_context = ""
  
  def __init__(self, symbol):
    self.center = ""
    self.context = ["", ""]
    self.state = -1
    self.initial = False
    self.final = False
    self._parseSymbol(symbol)
    
  def _parseSymbol(self, symbol):
    m = self.symbol_re.match(symbol)
    if not m:
      raise Exception("cannot parse symbol: %s" % symbol)
    items = m.groups()
    self.center = items[0]
    self.context = ["", ""]
    for i in [1, 2]:
      if items[i] == "#":
        self.context[i-1] = self.empty_context
      else:
        self.context[i-1] = items[i]
    self.initial = items[3] == "@i"
    self.final = items[4] == "@f"
    self.state = int(items[5])
    
  def phoneSymbol(self):
    s = self.center
    if self.initial: s += "@i"
    if self.final: s += "@f"
    return s 

  
def main(opts, args):
  state_syms = SymbolTable()
  state_syms.read(opts.state_symbols)
  ci_phones = opts.ci_phones.split(",")
  models = HmmStateModels()
  models.setModelSymbols(state_syms)
  models.setCiPhones(ci_phones)
  models.setEmptyContext([opts.empty_context])
  models.parseLogFile(opts.states_log)
  AllophoneState.empty_context = opts.empty_context
  
  for line in file(opts.allophone_states):
    line = line.strip()
    if not line or line[0] == "#": continue
    a = AllophoneState(line)
    if a.center in ci_phones:
      l, c, r = "", a.center, ""
    else:
      l, c, r = a.context[0], a.phoneSymbol(), a.context[1]
    m = models.find(l, c, r, a.state)
    if m < 0:
      sys.stderr.write("[WARNING] not found: %s\n" % line)
      if a.initial or a.final:
        m = models.find(l, a.center, r, a.state)
    if m < 0:
      sys.stderr.write("[ERROR] cannot map %s\n" % line)  
    else:
      print line, m 
    
if __name__ == "__main__":
  optparser = optparse.OptionParser(\
      usage = "USAGE: %prog [OPTIONS]")
  optparser.add_option("-s", "--state_symbols", dest="state_symbols",
                       help="HMM state model symbol table")
  optparser.add_option("-l", "--states_log", dest="states_log",
                       help="context builder state log file")
  optparser.add_option("-a", "--allophone_states", dest="allophone_states",
                       help="allophone state list to process")
  optparser.add_option("-o", "--output", dest="output",
                       help="lookup table output file")
  optparser.add_option("-e", "--empty_context", dest="empty_context",
                       help="phone symbol used for empty context",
                       default="si")
  optparser.add_option("-c", "--ci_phones", dest="ci_phones",
                       help="list of ci phones, separated by ','")
  opts, args = optparser.parse_args()
  if not (opts.state_symbols and opts.states_log):
    optparser.print_usage()
    sys.exit(1)
  main(opts, args)

  
