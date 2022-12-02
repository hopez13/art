module android/soong/art

require (
	android/soong v0.0.0
	github.com/google/blueprint v0.0.0
)

require (
	google.golang.org/protobuf v0.0.0 // indirect
	prebuilts/bazel/common/proto/analysis_v2 v0.0.0-00010101000000-000000000000 // indirect
	prebuilts/bazel/common/proto/build v0.0.0-00010101000000-000000000000 // indirect
)

replace (
	android/soong => ../../build/soong
	github.com/google/blueprint => ../../build/blueprint
	github.com/google/go-cmp => ../../external/go-cmp
	google.golang.org/protobuf => ../../external/golang-protobuf
	prebuilts/bazel/common/proto/analysis_v2 => ../../prebuilts/bazel/common/proto/analysis_v2
	prebuilts/bazel/common/proto/build => ../../prebuilts/bazel/common/proto/build
)

go 1.19
