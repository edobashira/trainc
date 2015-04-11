# Input Files #
  * `--ci_state_list` (_string_): file containing context independent HMM state models. [HMM state list](InputFormats.md).
  * `--counting_transducer` (_string_): transducer used for counting states. Usually a lexicon transducer. If not given, the C transducer will be used for counting. OpenFst `vector` FST.
  * `--final_phones` (_string_): file containing word end phones. [phone list](InputFormats.md).
  * `--initial_phones` (_string_): file containing word initial phones. [phone list](InputFormats.md).
  * `--phone_length` (_string_): phone lengths definition, i.e. number of HMM states per phone. If not set, the number of states is deduced from the statistics. Unobserved states may then lead to wrong phone length. [phone lengths](InputFormats.md).
  * `--phone_map` (_string_): mapping of phones with tied models. Map central phones of allophone models to another phone. This is useful for adding wordboundary information. [phone map](InputFormats.md).
  * `--phone_sets` (_string_): context class definitions. Phonetic questions used for splitting. [question sets](InputFormats.md).
  * `--phone_sets_pos` (_string_): context class per position. Define separate question sets per context position. Format for this option is `pos=file,pos=file,...` with `pos` in `[-num_left_contexts .. 0 .. num_right_contexts]`. Positions which are not defined, are assigned to the default question set (option `--phone_sets`). [question sets](InputFormats.md).
  * `--phone_syms` (_string_): symbol table for CI phone (output) symbols. OpenFst symbol table (text format).
  * `--replay` (_string_): execute the splits from the given file. Binary split sequence, generated with `--save_splits`.
  * `--samples_file` (_string_): sample data file. File type depends on `--sample_type`.
  * `--sample_type` (_enum_): sample file type. default: `text`
    * `text`: [samples text format](InputFormats.md).


# Options #
  * `--boundary_context` (_string_): context label to use at boundaries. default: `"sil"`
  * `--determistic_split` (_bool_): spliting of (un-shifted) transducers produces input determistic arcs (requires `--shifted_models=false`). default: `true`
  * `--ignore_absent_models` (_bool_): do not consider models for splitting which are not part of the used counting transducer. default: `false`
  * `--max_hyps` (_int_): maximum number of hypotheses evaluated. default: `0`
  * `--min_observations` (_int_): minimum number of observations per leaf. default: `1000`
  * `--min_seen_contexts` (_int_): minimum number of seen contexts per leaf. default: `0`
  * `--min_split_gain` (_double_): minimum gain for a node split. default: `0.0`
  * `--num_left_contexts` (_int_): number of left context symbols. default: `1`
  * `--num_right_contexts` (_int_): number of right context symbols. default: `1`
  * `--num_threads` (_int_): number of threads used for split calculations. default: `1`
  * `--shifted_models` (_bool_): shift models to arcs with the right context input label (requires --use\_composition=true). default: `true`
  * `--split_center_phone` (_bool_): split sets of center phones. Required when states are shared between several phones (`--transducer_init=sharedstate`). default: `false`
  * `--state_penalty_weight` (_double_): weight of the transducer size penalty. Control for the size of the resulting transducer by weighting the number of new states required by a phone model split. default: `10.0`
  * `--target_num_models` (_int_): maximum number of HMM state models. 0 for unlimited. default: `0`
  * `--target_num_states` (_int_): maximum number of transducer states. 0 for unlimited. default: `0`
  * `--transducer_init` (_enum_): type of transducer initialization. default: `"basic"`
    * `basic`: Initializes the transducer with one state per phone, a monophone model for each phone, and all valid arcs between the states.
    * `tiedmodel`: Initialize the transducer with one state per phone, and monophone models that may be shared among several phones.
    * `sharedstate`: Initialize in the same way as with `tiedmodel` but create only one state in the transducer for a set of mapped phone symbols.
    * `wordboundary`: Initialize in the same way as with `tiedmodel`  but from final phones only allow transitions to initial phones .
  * `--use_composition` (_bool_): use composition of C and the counting transducer. default: `true`
  * `--variance_floor` (_double_): minimum variance threshold for Gaussian models. default: `0.001`


# Output Files #
  * `--cd2phone_hmm_name_map` (_string_): Name map from CD to phone HMMs.
  * `--cd2ci_state_name_map` (_string_): State name map from CD to CI states.
  * `--ci_hmmlist` (_string_): CI HMM list file.
  * `--CLtrans` (_string_): CL transducer file.
  * `--Ctrans` (_string_): C transducer file.
  * `--hmm_list` (_string_): HMM list.
  * `--hmm_syms` (_string_): HMM symbol table file.
  * `--hmm_to_phone_map` (_string_): CD HMM to phone map file.
  * `--Htrans` (_string_): H transducer file.
  * `--leaf_model` (_string_): state distribution model  file.
  * `--leaf_model_type` (_string_): type of state model file.
  * `--save_splits` (_string_): sequence of applied splits.
  * `--state_model_log` (_string_): state model information.
  * `--state_syms` (_string_): HMM state symbol table file.
  * `--transducer_log` (_string_): transducer state information.