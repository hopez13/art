target_config = {

# Configuration syntax:
#
#   Required keys: (Use one of these)
#    * cmd' - Runs an arbitrary (shell) command.
#    * 'target' - Runs a 'make <target>' command.
#    * none of the above - Runs a testrunner.py command.
#
#   Optional keys: (Use any of these)
#    * env - Add additional environment variable to the current environment.
#    * flags - Pass extra flags to the command being run.
#    * forward_flags - Forward the flags from run_build_test_target.py to this command.
#
#
# *** IMPORTANT ***:
#    This configuration is used by the android build server. Targets must not be renamed
#    or removed.
#

##########################################

    # ART run-test configurations
    # (calls testrunner which builds and then runs the test targets)

    'art-test' : {
        'flags' : [],
        'env' : {
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    'art-interpreter' : {
        'flags' : ['--interpreter'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    'art-interpreter-access-checks' : {
        'flags' : ['--interp-ac'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    'art-jit' : {
        'flags' : ['--jit'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    'art-gcstress-gcverify': {
        'flags' : ['--gcstress',
                   '--gcverify'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'false',
            'ART_DEFAULT_GC_TYPE' : 'SS'
        }
    },
    'art-interpreter-gcstress' : {
        'flags': ['--interpreter',
                  '--gcstress'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'false',
            'ART_DEFAULT_GC_TYPE' : 'SS'
        }
    },
    'art-optimizing-gcstress' : {
        'flags': ['--gcstress',
                  '--optimizing'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'false',
            'ART_DEFAULT_GC_TYPE' : 'SS'
        }
    },
    'art-jit-gcstress' : {
        'flags': ['--jit',
                  '--gcstress'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'false',
            'ART_DEFAULT_GC_TYPE' : 'SS'
        }
    },
    'art-read-barrier' : {
        'flags': ['--interpreter',
                  '--optimizing'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'true',
            'ART_HEAP_POISONING' : 'true'
        }
    },
    'art-read-barrier-gcstress' : {
        'flags' : ['--interpreter',
                   '--optimizing',
                   '--gcstress'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'true',
            'ART_HEAP_POISONING' : 'true'
        }
    },
    'art-read-barrier-table-lookup' : {
        'flags' : ['--interpreter',
                   '--optimizing'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'true',
            'ART_READ_BARRIER_TYPE' : 'TABLELOOKUP',
            'ART_HEAP_POISONING' : 'true'
        }
    },
    'art-debug-gc' : {
        'flags' : ['--interpreter',
                   '--optimizing'],
        'env' : {
            'ART_TEST_DEBUG_GC' : 'true',
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    'art-ss-gc' : {
        'flags' : ['--interpreter',
                   '--optimizing',
                   '--jit'],
        'env' : {
            'ART_DEFAULT_GC_TYPE' : 'SS',
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    'art-gss-gc' : {
        'flags' : ['--interpreter',
                   '--optimizing',
                   '--jit'],
        'env' : {
            'ART_DEFAULT_GC_TYPE' : 'GSS',
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    'art-ss-gc-tlab' : {
        'flags' : ['--interpreter',
                   '--optimizing',
                   '--jit'],
        'env' : {
            'ART_DEFAULT_GC_TYPE' : 'SS',
            'ART_USE_TLAB' : 'true',
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    'art-gss-gc-tlab' : {
        'flags' : ['--interpreter',
                   '--optimizing',
                   '--jit'],
        'env' : {
            'ART_DEFAULT_GC_TYPE' : 'GSS',
            'ART_USE_TLAB' : 'true',
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    'art-tracing' : {
        'flags' : ['--trace'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    'art-interpreter-tracing' : {
        'flags' : ['--interpreter',
                   '--trace'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'false',
        }
    },
    'art-forcecopy' : {
        'flags' : ['--forcecopy'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'false',
        }
    },
    'art-no-prebuild' : {
        'flags' : ['--no-prebuild'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'false',
        }
    },
    'art-no-image' : {
        'flags' : ['--no-image'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'false',
        }
    },
    'art-interpreter-no-image' : {
        'flags' : ['--interpreter',
                   '--no-image'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'false',
        }
    },
    'art-relocate-no-patchoat' : {
        'flags' : ['--relocate-npatchoat'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'false',
        }
    },
    'art-no-dex2oat' : {
        'flags' : ['--no-dex2oat'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'false',
        }
    },
    'art-heap-poisoning' : {
        'flags' : ['--interpreter',
                   '--optimizing'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'false',
            'ART_HEAP_POISONING' : 'true'
        }
    },

    # ART gtest configurations
    # (calls make 'target' which builds and then runs the gtests).

    'art-gtest' : {
        'target' :  'test-art-host-gtest',
        'env' : {
            'ART_USE_READ_BARRIER' : 'true'
        }
    },
    'art-gtest-read-barrier': {
        'target' :  'test-art-host-gtest',
        'env' : {
            'ART_USE_READ_BARRIER' : 'true',
            'ART_HEAP_POISONING' : 'true'
        }
    },
    'art-gtest-read-barrier-table-lookup': {
        'target' :  'test-art-host-gtest',
        'env': {
            'ART_USE_READ_BARRIER' : 'true',
            'ART_READ_BARRIER_TYPE' : 'TABLELOOKUP',
            'ART_HEAP_POISONING' : 'true'
        }
    },
    'art-gtest-ss-gc': {
        'target' :  'test-art-host-gtest',
        'env': {
            'ART_DEFAULT_GC_TYPE' : 'SS',
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    'art-gtest-gss-gc': {
        'target' :  'test-art-host-gtest',
        'env' : {
            'ART_DEFAULT_GC_TYPE' : 'GSS',
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    'art-gtest-ss-gc-tlab': {
        'target' :  'test-art-host-gtest',
        'env': {
            'ART_DEFAULT_GC_TYPE' : 'SS',
            'ART_USE_TLAB' : 'true',
            'ART_USE_READ_BARRIER' : 'false',
        }
    },
    'art-gtest-gss-gc-tlab': {
        'target' :  'test-art-host-gtest',
        'env': {
            'ART_DEFAULT_GC_TYPE' : 'GSS',
            'ART_USE_TLAB' : 'true',
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    'art-gtest-debug-gc' : {
        'target' :  'test-art-host-gtest',
        'env' : {
            'ART_TEST_DEBUG_GC' : 'true',
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    'art-gtest-valgrind32': {
        'target' : 'valgrind-test-art-host32',
        'env': {
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    'art-gtest-valgrind64': {
        'target' : 'valgrind-test-art-host64',
        'env': {
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    'art-gtest-heap-poisoning': {
        'target' : 'valgrind-test-art-host64',
        'env' : {
            'ART_HEAP_POISONING' : 'true',
            'ART_USE_READ_BARRIER' : 'false'
        }
    },

   # ART Golem build targets used by go/lem (continuous ART benchmarking),
   # (art-opt-cc is used by default since it mimics the default preopt config),
   #
   # calls golem/build-target.sh which builds a golem tarball

    'art-golem-android-armv7': {
        'cmd' : 'art/tools/golem/build-target.sh',
        'forward_flags' : '-j',
        'flags' : [
            '--showcommands',
            '--machine-type=android-armv7',
            '--golem=art-opt-cc',
            '--tarball',
        ]
    },
    'art-golem-android-armv8': {
        'cmd' : 'art/tools/golem/build-target.sh',
        'forward_flags' : '-j',
        'flags' : [
            '--showcommands',
            '--machine-type=android-armv8',
            '--golem=art-opt-cc',
            '--tarball',
        ]
    },
    'art-golem-linux-armv7': {
        'cmd' : 'art/tools/golem/build-target.sh',
        'forward_flags' : '-j',
        'flags' : [
            '--showcommands',
            '--machine-type=linux-armv7',
            '--golem=art-opt-cc',
            '--tarball',
        ]
    },
    'art-golem-linux-armv8': {
        'cmd' : 'art/tools/golem/build-target.sh',
        'forward_flags' : '-j',
        'flags' : [
            '--showcommands',
            '--machine-type=linux-armv8',
            '--golem=art-opt-cc',
            '--tarball',
        ]
    },
    'art-golem-linux-ia32': {
        'cmd' : 'art/tools/golem/build-target.sh',
        'forward_flags' : '-j',
        'flags' : [
            '--showcommands',
            '--machine-type=linux-ia32',
            '--golem=art-opt-cc',
            '--tarball',
        ]
    },
    'art-golem-linux-x64': {
        'cmd' : 'art/tools/golem/build-target.sh',
        'forward_flags' : ['-j'],
        'flags' : [
            '--showcommands',
            '--machine-type=linux-x64',
            '--golem=art-opt-cc',
            '--tarball',
        ]
    },
}
