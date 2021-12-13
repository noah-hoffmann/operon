# Use gcc-9 (or later)
export CC=gcc-9
export CXX=gcc-9

# run cmake with options
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release  -DBUILD_PYBIND=ON -DUSE_OPENLIBM=ON -DUSE_SINGLE_PRECISION=ON -DCERES_TINY_SOLVER=ON

# build
make VERBOSE=1 -j pyoperon

# install python package
make install
