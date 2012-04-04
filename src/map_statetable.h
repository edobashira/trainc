// map_statetable.h
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Copyright 2011 RWTH Aachen University. All Rights Reserved.
// Author: rybach@cs.rwth-aachen.de (David Rybach)
//
// \file stable table used for composition which stores the tuples in two maps

#ifndef MAP_STATE_TABLE_H_
#define MAP_STATE_TABLE_H_

#include <map>
#include <stack>
#include <vector>
#include "fst/state-table.h"
#include "debug.h"

namespace trainc {

// stable table used for fst::ComposeFst which allows to
// efficiently find all tuples for a state in the first transducer.
template<class T, class F>
class MapStateTable {
public:
  typedef T StateTuple;
  typedef typename StateTuple::StateId StateId;

  class Iterator;

  MapStateTable() : state_id_offset_(0) {}

  void SetStateIdOffset(int offset) {
    CHECK(tuple2id_.empty());
    state_id_offset_ = offset;
  }

  StateId FindState(StateId s1, StateId s2) {
    return FindState(StateTuple(s1, s2, typename StateTuple::FilterState(0)));
  }

  StateId FindState(const StateTuple &tuple) {
    if (tuple.state_id1 < tuple2id_.size()) {
      StateMap &map = tuple2id_[tuple.state_id1];
      typename StateMap::const_iterator i = map.find(tuple.state_id2);
      if (i != map.end()) {
        return i->second + state_id_offset_;
      }
    } else {
      tuple2id_.resize(tuple.state_id1 + 1);
    }
    StateId id = NextId(tuple);
    tuple2id_[tuple.state_id1].insert(std::make_pair(tuple.state_id2, id));
    return id + state_id_offset_;
  }

  const StateTuple &Tuple(StateId s) const {
    return id2tuple_[s - state_id_offset_];
  }

  StateId Size() const {
    return id2tuple_.size() - free_ids_.size();
  }

  StateId MaxId() const {
    return id2tuple_.size() - 1;
  }

  void Erase(StateId s) {
    s -= state_id_offset_;
    const StateTuple &tuple = id2tuple_[s];
    StateMap &map = tuple2id_[tuple.state_id1];
    typename StateMap::iterator i = map.find(tuple.state_id2);
    map.erase(i);
    free_ids_.push_back(s);
  }

  void EraseFirstState(StateId s1) {
    StateMap &map = tuple2id_[s1];
    for (typename StateMap::const_iterator i = map.begin(); i != map.end(); ++i) {
      free_ids_.push_back(i->second);
    }
    map.clear();
  }

  void Clear() {
    tuple2id_.clear();
    id2tuple_.clear();
    free_ids_.clear();
  }

  bool HasFirstState(StateId s1) const {
    return s1 < tuple2id_.size() && !tuple2id_[s1].empty();
  }

  Iterator TupleIdsForFirstState(StateId s1) const {
    DCHECK_LT(s1, tuple2id_.size());
    return Iterator(tuple2id_[s1].begin(), tuple2id_[s1].end(), true, state_id_offset_);
  }

  Iterator SecondStateIds(StateId s1) const {
    DCHECK_LT(s1, tuple2id_.size());
    return Iterator(tuple2id_[s1].begin(), tuple2id_[s1].end(), false, state_id_offset_);
  }

  bool Error() const { return false; }

private:
  typedef std::map<StateId, StateId> StateMap;
  typedef std::vector<std::map<StateId, StateId> > TupleMap;

  StateId NextId(const StateTuple &tuple) {
    StateId id = fst::kNoStateId;
    if (free_ids_.empty()) {
      id = id2tuple_.size();
      id2tuple_.push_back(tuple);
    } else {
      id = free_ids_.back();
      free_ids_.pop_back();
      id2tuple_[id] = tuple;
    }
    return id;
  }
  TupleMap tuple2id_;
  std::vector<StateTuple> id2tuple_;
  std::deque<StateId> free_ids_;
  int state_id_offset_;
};


template<class T, class F>
class MapStateTable<T, F>::Iterator {
  typedef typename MapStateTable<T, F>::StateId StateId;
  typedef std::map<StateId, StateId> StateMap;
public:
  Iterator(typename StateMap::const_iterator begin,
           typename StateMap::const_iterator end, bool tuple_id = true,
           int state_id_offset = 0) :
    cur_(begin), end_(end), tuple_id_(tuple_id), offset_(state_id_offset) {
  }
  bool Done() const {
    return cur_ == end_;
  }
  void Next() {
    ++cur_;
  }
  StateId Value() {
    return tuple_id_ ? cur_->second + offset_ : cur_->first;
  }
private:
  typename StateMap::const_iterator cur_, end_;
  bool tuple_id_;
  int offset_;
};


template<class A, class F>
class MapComposeStateTable :
    public MapStateTable<fst::ComposeStateTuple<typename A::StateId, F>, F> {
public:
    typedef A Arc;
    typedef typename A::StateId StateId;
    typedef F FilterState;
    typedef fst::ComposeStateTuple<StateId, FilterState> StateTuple;

    MapComposeStateTable() {}
    MapComposeStateTable(const fst::Fst<A>&, const fst::Fst<A>&) {}
    MapComposeStateTable(const MapComposeStateTable<A, F> &) {}
private:
    void operator=(const MapComposeStateTable<A, F>&);
};

}  // namespace trainc

#endif  // MAP_STATE_TABLE_H_
