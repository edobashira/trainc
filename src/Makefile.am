AM_CXXFLAGS = -Wall -Wno-sign-compare -Wno-long-long -Wno-deprecated

if DEBUG
AM_CXXFLAGS += -fno-omit-frame-pointer -fno-inline 
AM_LDFLAGS = -rdynamic
endif

bin_PROGRAMS = builder
if WITH_TESTS
noinst_PROGRAMS = unittests
endif


noinst_LIBRARIES = libbuilder.a

libbuilder_a_SOURCES = \
	array.h \
	composed_transducer.cc composed_transducer.h \
	context_builder.cc context_builder.h \
	context_set.cc context_set.h \
	debug.h \
	epsilon_closure.cc epsilon_closure.h \
	file.cc file.h \
	fst_interface.cc fst_interface.h \
	gaussian_model.cc gaussian_model.h \
	hash.h \
	hmm_compiler.cc hmm_compiler.h \
	integer_set.h \
	lexicon_check.cc lexicon_check.h \
	lexicon_compiler.cc lexicon_compiler.h \
	lexicon_init.cc lexicon_init.h \
	lexicon_split_predictor.cc lexicon_split_predictor.h \
	lexicon_state_splitter.cc lexicon_state_splitter.h \
	lexicon_transducer.cc lexicon_transducer.h \
	map_statetable.h \
	model_splitter.cc model_splitter.h \
	phone_models.cc phone_models.h \
	phone_sequence.cc phone_sequence.h \
	recipe.cc recipe.h \
	sample.cc sample.h \
	sample_reader.cc sample_reader.h \
	set_inventory.h \
	scorer.h \
	shifted_init.cc shifted_init.h \
	shifted_state_splitter.cc shifted_state_splitter.h \
	shifted_split_predictor.cc shifted_split_predictor.h \
	state_siblings.cc state_siblings.h \
	state_splitter.cc state_splitter.h \
	split_optimizer.cc split_optimizer.h \
	split_predictor.cc split_predictor.h \
	split_generator.cc split_generator.h \
	stringmap.cc stringmap.h \
	stringutil.cc stringutil.h \
	thread.h \
	transducer.cc transducer.h \
	transducer_check.cc transducer_check.h \
	transducer_compiler.cc transducer_compiler.h \
	transducer_init.cc transducer_init.h \
	util.h
	
builder_SOURCES = builder.cc
builder_LDADD = libbuilder.a

unittests_SOURCES = \
	composed_transducer_test.cc \
	context_builder_test.cc \
	context_set_test.cc \
	file_test.cc \
	gaussian_model_test.cc \
	integer_set_test.cc \
	lexicon_split_test.cc lexicon_split_test.h \
	lexicon_transducer_test.cc \
	phone_models_test.cc \
	recipe_test.cc \
	sample_test.cc \
	sample_reader_test.cc \
	shifted_split_test.cc \
	transducer_test.cc transducer_test.h \
	stringutil_test.cc \
	unittest.cc unittest.h

if HAVE_THREADS
unittests_SOURCES += thread_test.cc
endif

unittests_LDADD = -lcppunit libbuilder.a
