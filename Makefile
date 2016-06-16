buildDir=build

#change to Debug for debug mode
buildType=Release
# buildType=Debug

all: build

cmake: CMakeLists.txt
	cd $(buildDir) && cmake -DCMAKE_BUILD_TYPE=$(buildType) ..

build: cmake
	$(MAKE) -C $(buildDir)

clean:
	$(MAKE) -C $(buildDir) clean

cleanup_cache:
	cd $(buildDir) && rm -rf *

