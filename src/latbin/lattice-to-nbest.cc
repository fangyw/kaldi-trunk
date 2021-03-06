// latbin/lattice-to-nbest.cc

// Copyright 2009-2012  Stefan Kombrink  Johns Hopkins University (Author: Daniel Povey)

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


#include "base/kaldi-common.h"
#include "util/common-utils.h"
#include "fstext/fstext-lib.h"
#include "lat/kaldi-lattice.h"

int main(int argc, char *argv[]) {
  try {
    using namespace kaldi;
    typedef kaldi::int32 int32;
    typedef kaldi::int64 int64;
    using fst::SymbolTable;
    using fst::VectorFst;
    using fst::StdArc;

    const char *usage =
        "Work out N-best paths in lattices and write out as FSTs\n"
        "Usage: lattice-to-nbest [options] lattice-rspecifier lattice-wspecifier\n"
        " e.g.: lattice-to-nbest --acoustic-scale=0.1 --n=10 ark:1.lats ark:nbest.lats\n";
      
    ParseOptions po(usage);
    BaseFloat acoustic_scale = 1.0;
    int32 n = 1;
    
    po.Register("acoustic-scale", &acoustic_scale, "Scaling factor for acoustic likelihoods");
    po.Register("n", &n, "Number of distinct paths");
    
    po.Read(argc, argv);

    KALDI_ASSERT(n > 0);

    if (po.NumArgs() != 2) {
      po.PrintUsage();
      exit(1);
    }

    std::string lats_rspecifier = po.GetArg(1),
        lats_wspecifier = po.GetArg(2);


    // Read as regular lattice-- this is the form we need it in for efficient
    // pruning.
    SequentialLatticeReader lattice_reader(lats_rspecifier);
    
    // Write as compact lattice.
    CompactLatticeWriter compact_nbest_writer(lats_wspecifier); 

    int32 n_done = 0;
    int64 n_paths_out = 0;

    if (acoustic_scale == 0.0)
      KALDI_ERR << "Do not use a zero acoustic scale (cannot be inverted)";
    for (; !lattice_reader.Done(); lattice_reader.Next()) {
      std::string key = lattice_reader.Key();
      Lattice lat = lattice_reader.Value();
      lattice_reader.FreeCurrent();
      fst::ScaleLattice(fst::AcousticLatticeScale(acoustic_scale), &lat);

      std::vector<Lattice> nbest_lats;
      fst::NbestAsFsts(lat, n, &nbest_lats);

      if (nbest_lats.empty()) {
        KALDI_WARN << "Possibly empty lattice for utterance-id " << key
                   << "(no N-best entries)";
      } else {
        for (int32 k = 0; k < static_cast<int32>(nbest_lats.size()); k++) {
          std::ostringstream s;
          s << key << "-" << (k+1); // so if key is "utt_id", the keys
          // of the n-best are utt_id-1, utt_id-2, utt_id-3, etc.
          std::string nbest_key = s.str();
          fst::ScaleLattice(fst::AcousticLatticeScale(1.0/acoustic_scale),
                            &(nbest_lats[k]));
          CompactLattice nbest_clat;
          ConvertLattice(nbest_lats[k], &nbest_clat); // write in compact form.
          compact_nbest_writer.Write(nbest_key, nbest_clat);
        }
        n_done++;
        n_paths_out += nbest_lats.size();
      }
    }
      
    KALDI_LOG << "Done applying N-best algorithm to " << n_done << " lattices with n = "
              << n << ", average actual #paths is "
              << (n_paths_out/(n_done+1.0e-20));
    return (n_done != 0 ? 0 : 1);
  } catch(const std::exception &e) {
    std::cerr << e.what();
    return -1;
  }
}
