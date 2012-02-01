// gaussian_model.h
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
// Copyright 2010 RWTH Aachen University. All Rights Reserved.
// Author: rybach@cs.rwth-aachen.de (David Rybach)
//
// \file
// Gaussian mixture model

#ifndef MIXTURE_MODEL_H_
#define MIXTURE_MODEL_H_

#include <map>
#include <string>
#include <vector>
#include "debug.h"
#include "file.h"

namespace trainc {

class Statistics;

// Gaussian single density models with (untied) diagonal covariance
class GaussianModel {
  typedef std::map<const std::string, size_t> NameMap;
public:
  typedef std::vector<float> ModelVector;

  // Iterator over all models, sorted by index.
  // Can't use NameMap::const_iterator because of the ordering.
  class Iterator {
  public:
    Iterator(const GaussianModel &model) :
      index_(0), end_(model.index_map_.size()),
      names_(model.index_map_) {}
    bool Done() const { return index_ >= end_; }
    void Next() { ++index_; }
    size_t Index() const { return index_; }
    const std::string& Name() { return names_[index_]; }
  private:
    size_t index_, end_;
    const std::vector<std::string> &names_;
  };
  friend class Iterator;

  // add a Gaussian to the model
  void AddModel(const std::string &name,
                const ModelVector &mean,
                const ModelVector &variance);

  // estimate a Gaussian from the given sufficient statistics
  // and add it to the model
  void Estimate(const std::string &name,
                const Statistics &sufficient_statitics,
                float variance_floor);

  Iterator GetIterator() const { return Iterator(*this); }

  int NumModels() const {
    DCHECK_EQ(means_.size(), variances_.size());
    return means_.size();
  }
  int Dimension() const;

  // get or add index for name
  int GetIndex(const std::string &name, bool add = false);


  const ModelVector& Mean(size_t index) const {
    DCHECK_LT(index, means_.size());
    return means_[index];
  }
  const ModelVector& Variance(size_t index) const {
    DCHECK_LT(index, variances_.size());
    return variances_[index];
  }


  // add feature meta information (required by some writers)
  void SetFeatureDescription(const std::string &description) {
    feature_description_ = description;
  }
  // feature meta information
  const std::string& FeatureDescription() const {
    return feature_description_;
  }
  // set frontend meta information (required by some writers)
  void SetFrontendDescription(const std::string &description) {
    frontend_description_ = description;
  }
  // frontend meta information
  const std::string& FrontendDescription() const {
    return frontend_description_;
  }
private:
  std::map<const std::string, size_t> name_map_;
  std::vector<std::string> index_map_;

  std::vector<ModelVector> means_, variances_;
  std::string feature_description_, frontend_description_;
};


// base class for model writting classes
class ModelWriter {
public:
  virtual ~ModelWriter() {}
  virtual bool Write(const std::string &filename, const GaussianModel &model) const = 0;

  // Factory method
  static ModelWriter* Create(const std::string &type);
};

// write model in a simple text file
class ModelTextWriter : public ModelWriter {
public:
  virtual ~ModelTextWriter() {}
  virtual bool Write(const std::string &filename, const GaussianModel &model) const;
  static std::string Name() { return "text"; }
protected:
  void WriteVector(OutputBuffer *ob, const GaussianModel::ModelVector &v) const;
private:
  static const int kFormatVersion;
};

class RwthModelTextWriter : public ModelTextWriter {
public:
  virtual ~RwthModelTextWriter() {}
  virtual bool Write(const std::string &filename, const GaussianModel &model) const;
  static std::string Name() { return "rwth-text"; }
private:
  static const char *kFormatHeader;
};


}  // namespace trainc

#endif  // MIXTURE_MODEL_H_
