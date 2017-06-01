// kws/kws-scoring.h

// Copyright (c) 2015, Johns Hopkins University (Yenda Trmal<jtrmal@gmail.com>)

// See ../../COPYING for clarification regarding multiple authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
// WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
// See the Apache 2 License for the specific language governing permissions and
// limitations under the License.
#ifndef KALDI_KWS_KWS_SCORING_H_
#define KALDI_KWS_KWS_SCORING_H_

#include <vector>
#include <list>
#include <utility>
#include <string>

#include "util/common-utils.h"
#include "util/stl-utils.h"

namespace kaldi {

class KwsTerm {
 public:
  KwsTerm():
    utt_id_(0),
    kw_id_(""),
    start_time_(0),
    end_time_(0),
    score_(0)
  {  }

  // A convenience function to instantiate the object
  // from the entries from the results files (generated by kws-search)
  // In longer term, should be replaced by Read/Write functions
  KwsTerm(const std::string &kw_id, const std::vector<double> &vec) {
    set_kw_id(kw_id);

    KALDI_ASSERT(vec.size() == 4);

    set_utt_id(vec[0]);
    set_start_time(vec[1]);
    set_end_time(vec[2]);
    set_score(vec[3]);
  }

  inline bool valid() const {
    return (kw_id_ != "");
  }

  // Attribute accessors/mutators
  inline int utt_id() const {return utt_id_;}
  inline void set_utt_id(int utt_id) {utt_id_ = utt_id;}
  inline std::string kw_id() const {return kw_id_;}
  inline void set_kw_id(const std::string &kw_id) {kw_id_ = kw_id;}
  inline int start_time() const  {return start_time_;}
  inline void set_start_time(int start_time) {start_time_ = start_time;}
  inline int end_time() const  {return end_time_;}
  inline void set_end_time(int end_time) {end_time_ = end_time;}
  inline float score() const  {return score_;}
  inline void set_score(float score) {score_ = score;}

 private:
  int utt_id_;
  std::string kw_id_;
  int start_time_;  // in frames
  int end_time_;    // in frames
  float score_;
};

// Not used, yet
enum DetectionDecision {
  kKwsFalseAlarm,      // Marked incorrectly as a hit
  kKwsMiss,            // Not marked as hit while it should be
  kKwsCorr,            // Marked correctly as a hit
  kKwsCorrUndetected,  // Not marked as a hit, correctly
  kKwsUnseen           // Instance not seen in the hypotheses list
};


struct AlignedTermsPair {
  KwsTerm ref;
  KwsTerm hyp;
  float aligner_score;
};

// Container class for the ref-hyp pairs
class KwsAlignment {
  friend class KwsTermsAligner;
 public:
  // TODO(jtrmal):  implement reading/writing CSV
  // void ReadCsv();
  void WriteCsv(std::iostream &os, const float frames_per_sec);

  typedef std::vector<AlignedTermsPair>  AlignedTerms;

  inline AlignedTerms::const_iterator begin() const {return alignment_.begin();}
  inline AlignedTerms::const_iterator end() const {return alignment_.end();}
  inline int size() const {return alignment_.size(); }

 private:
  //  sequence of touples ref, hyp, score
  //  either (in the sense of exlusive OR) of which can be
  //  empty (i.e .valid() will return false)
  //  if ref.valid() == false, then the hyp term does not have
  //  a matching reference
  //  if hyp.valid() == false, then the ref term does not have
  //  a matching reference
  //  Score is the aligned score, i.e.
  AlignedTerms alignment_;

  inline void Add(const AlignedTermsPair &next) {
    alignment_.push_back(next);
  }
};

struct KwsTermsAlignerOptions {
  int max_distance;  // Maximum distance (in frames) of the centers of
                     // the ref and and the hyp to be considered as a potential
                     // match during alignment process
                     // Default: 50 frames (usually 0.5 seconds)

  inline KwsTermsAlignerOptions(): max_distance(50) {}
  void Register(OptionsItf *opts);
};

class KwsTermsAligner {
 public:
  void AddRef(const KwsTerm &ref) {
    refs_[ref.utt_id()][ref.kw_id()].push_back(ref);
    nof_refs_++;
  }

  void AddHyp(const KwsTerm &hyp) {
    hyps_.push_back(hyp);
    nof_hyps_++;
  }

  inline int nof_hyps() const {return nof_hyps_;}
  inline int nof_refs() const {return nof_refs_;}

  explicit KwsTermsAligner(const KwsTermsAlignerOptions &opts);

  // Retrieve the final ref-hyp alignment
  KwsAlignment AlignTerms();

  // Score the quality of a match between ref and hyp
  virtual float AlignerScore(const KwsTerm &ref, const KwsTerm &hyp);

 private:
  typedef std::vector<KwsTerm> TermArray;
  typedef std::vector<KwsTerm>::iterator TermIterator;
  typedef unordered_map<int, bool> TermUseMap;
  unordered_map<int, unordered_map<std::string, TermArray > > refs_;
  unordered_map<int, unordered_map<std::string, TermUseMap > > used_ref_terms_;
  std::list<KwsTerm> hyps_;
  KwsTermsAlignerOptions opts_;
  int nof_refs_;
  int nof_hyps_;

  // Finds the best (if there is one) ref instance for the
  // given hyp term. Returns index >= 0 when found, -1 when
  // not found
  int FindBestRefIndex(const KwsTerm &term);

  // Find the next adept for best match to hyp.
  TermIterator FindNextRef(const KwsTerm &hyp,
                           const TermIterator &prev,
                           const TermIterator &last);
  // A quick test if it's even worth to attempt to look
  // for a ref for the given term -- checks if the combination
  // of utt_id and kw_id exists in the reference.
  bool RefExistsMaybe(const KwsTerm &term);

  // Adds all ref entries which weren't matched to any hyp
  void FillUnmatchedRefs(KwsAlignment *ali);
};

struct TwvMetricsOptions {
  // The option names are taken from the Babel KWS15 eval plan
  // http://www.nist.gov/itl/iad/mig/upload/KWS15-evalplan-v05.pdf
  float cost_fa;             // The cost of an incorrect detection;
                             // defined as 0.1

  float value_corr;          // The value of a correct detection;
                             // defined as 1.0

  float prior_probability;   // The prior probability of a keyword;
                             // defined as 1e-4

  float score_threshold;     // The score threshold for computation of ATWV
                             // defined as 0.5

  float sweep_step;          // Size of the bin during sweeping for
                             // the oracle measures, 0.05 by default

  float audio_duration;      // Total duration of the audio
                             // This has to be set to a correct value
                             // in seconds, unset by default;

  TwvMetricsOptions();

  inline float beta() const {
    return (cost_fa/value_corr) * (1.0/prior_probability - 1);
  }

  void Register(OptionsItf *opts);
};

class TwvMetricsStats;

class TwvMetrics {
 public:
  explicit TwvMetrics(const TwvMetricsOptions &opts);
  ~TwvMetrics();

  // Feed the alignment -- can be done several times
  // so that the statistics will be cumulative
  void AddAlignment(const KwsAlignment &ali);

  // Forget the stats
  void Reset();

  // Actual Term-Weighted Value
  float Atwv();
  // Supreme Term-Weighted Value
  float Stwv();

  // Get the MTWV, OTWV and the MTWV threshold
  // Getting these metrics is computationally intensive
  // and most of the computations are shared between
  // getting MTWV and OTWV, so we compute them at he same time
  void GetOracleMeasures(float *final_mtwv,
                         float *final_mtwv_threshold,
                         float *final_otwv);

 private:
  KALDI_DISALLOW_COPY_AND_ASSIGN(TwvMetrics);

  float audio_duration_;
  float atwv_decision_threshold_;
  float beta_;

  TwvMetricsStats *stats_;

  void AddEvent(const KwsTerm &ref, const KwsTerm &hyp, float ali_score);
  void RefAndHypSeen(const std::string &kw_id, float score);
  void OnlyRefSeen(const std::string &kw_id, float score);
  void OnlyHypSeen(const std::string &kw_id, float score);
};

}  // namespace kaldi
#endif  // KALDI_KWS_KWS_SCORING_H_

