// gaussian_model.cc
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

#include <algorithm>
#include "gaussian_model.h"
#include "stringutil.h"
#include "sample.h"

namespace trainc {

int GaussianModel::GetIndex(const std::string &name, bool add) {
  NameMap::iterator n = name_map_.find(name);
  if (n != name_map_.end())
    return n->second;
  if (add) {
    int index = index_map_.size();
    index_map_.push_back(name);
    means_.resize(index + 1);
    variances_.resize(index + 1);
    name_map_.insert(NameMap::value_type(name, index));
    return index;
  } else {
    return -1;
  }
}

void GaussianModel::AddModel(const std::string &name,
                             const ModelVector &mean,
                             const ModelVector &variance) {
  CHECK_EQ(mean.size(), variance.size());
  size_t index = GetIndex(name, true);
  means_[index] = mean;
  variances_[index] = variance;
}

void GaussianModel::Estimate(const std::string &name,
                             const Statistics &sufficient_statistics,
                             float variance_floor) {
  size_t index = GetIndex(name, true);
  int d = sufficient_statistics.dimension();
  means_[index].resize(d);
  variances_[index].resize(d);
  const float *s = sufficient_statistics.sum();
  const float *s2 = sufficient_statistics.sum2();
  float weight = sufficient_statistics.weight();
  ModelVector::iterator mean = means_[index].begin(),
      var = variances_[index].begin(),
      mean_end = means_[index].end();
  for (; mean != mean_end; ++s, ++s2, ++mean, ++var) {
    *mean = *s / weight;
    *var = *s2 / weight;
    *var -= (*mean * *mean);
  }
}

int GaussianModel::Dimension() const {
  if (means_.empty()) return 0;
  return means_.front().size();
}

// ====================================================================

ModelWriter* ModelWriter::Create(const std::string &type) {
  if (type == RwthModelTextWriter::Name())
    return new RwthModelTextWriter;
  return new ModelTextWriter;
}

// ====================================================================

const int ModelTextWriter::kFormatVersion = 1;

bool ModelTextWriter::Write(const std::string &filename,
                            const GaussianModel &model) const {
  File *file = File::Create(filename, "w");
  if (!file) return false;
  CHECK(file->Open());
  OutputBuffer ob(file);
  ob.WriteString(StringPrintf("%d %d %d\n", kFormatVersion,
                                            model.Dimension(),
                                            model.NumModels()));
  for (GaussianModel::Iterator m = model.GetIterator(); !m.Done(); m.Next()) {
    ob.WriteString(StringPrintf("%s ", m.Name().c_str()));
    size_t index = m.Index();
    WriteVector(&ob, model.Mean(index));
    WriteVector(&ob, model.Variance(index));
    ob.WriteString("\n");
  }
  return ob.CloseFile();
}

void ModelTextWriter::WriteVector(OutputBuffer *ob, const GaussianModel::ModelVector &v) const {
  typedef GaussianModel::ModelVector::const_iterator Iterator;
  for (Iterator i = v.begin(); i != v.end(); ++i) {
    ob->WriteText(*i);
    ob->WriteString(" ");
  }
}

// ====================================================================

const char *RwthModelTextWriter::kFormatHeader =
    "#Version: 2.0\n#CovarianceType: DiagonalCovariance\n";

bool RwthModelTextWriter::Write(const std::string &filename,
                                const GaussianModel &model) const {
  File *file = File::Create(filename, "w");
  if (!file) return false;
  CHECK(file->Open());
  OutputBuffer ob(file);
  ob.WriteString(kFormatHeader);
  int n_models = model.NumModels();
  int dimension = model.Dimension();
  ob.WriteString(StringPrintf("%d %d %d %d %d\n",
                              dimension,
                              n_models, // nMixtures
                              n_models, // nDensities
                              n_models, // nMeans
                              n_models)); // nCovariances
  for (int m = 0; m < n_models; ++m) {
    ob.WriteString(StringPrintf("%d %d %d\n",
                                1, // nDensities
                                m, // density index
                                0)); // log weight
  }
  for (int m = 0; m < n_models; ++m) {
    ob.WriteString(StringPrintf("%d %d\n",
                                m, // mean index
                                m)); // covariance index
  }
  for (GaussianModel::Iterator m = model.GetIterator(); !m.Done(); m.Next()) {
    ob.WriteString(StringPrintf("%d ", dimension));
    WriteVector(&ob, model.Mean(m.Index()));
    ob.WriteString("\n");
  }
  for (GaussianModel::Iterator m = model.GetIterator(); !m.Done(); m.Next()) {
    ob.WriteString(StringPrintf("%d ", dimension));
    const GaussianModel::ModelVector &v = model.Variance(m.Index());
    for (int d = 0; d < dimension; ++d) {
      ob.WriteText(v[d]); // value
      ob.WriteString(" 1 "); // weight
    }
    ob.WriteString("\n");
  }
  return ob.CloseFile();
}


}  // namespace trainc
