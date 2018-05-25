Build instructions for boost-1.64.0 for KeyTalk server and KeyTalk Linux client
---------------------------------------------------------------------------------

1. Download boost

wget https://dl.bintray.com/boostorg/release/1.64.0/source/boost_1_64_0.tar.gz
tar -xzf boost_1_64_0.tar.gz

2. Setup build engine:

cd boost_1_64_0/tools/build
./bootstrap.sh
mkdir bjam-inst
./b2 --prefix=./bjam-inst install

3. Build boost libraries

for lib in date_time filesystem program_options regex serialization system ; do \
pushd ../../libs/${lib}/build ; \
 ../../../tools/build/bjam-inst/bin/bjam release cxxflags=-fPIC -a link=static threading=multi stage ; \
popd ; \
done ;

pushd ../../libs/thread/build
../../../tools/build/bjam-inst/bin/bjam release cxxflags=-fPIC -a link=static
popd


Notice that compiling  with -fPIC flag is only needed to include boost static libs into libralogger.so executables on 64-bit platform.
Another alternative would be to build boost as a shared library.


4. Install

OS_SPEC=$(uname -s | tr "[:upper:]" "[:lower:]")-$(uname -r | cut -d '-' -f 1)-$(uname -m | cut -d '-' -f 1)

pushd ../../
cp -Rf boost/ /keytalk/Software/Import/boost-1.64
for lib in date_time filesystem program_options regex serialization system thread; do \
  rm -rf /keytalk/Software/Import/boost-1.64/bin.v2/libs/${lib}/${OS_SPEC}; \
  mkdir -p /keytalk/Software/Import/boost-1.64/bin.v2/libs/${lib}/${OS_SPEC}; \
  cp bin.v2/libs/${lib}/build/gcc-$(gcc -dumpversion)/release/link-static/threading-multi/libboost_${lib}.a /keytalk/Software/Import/boost-1.64/bin.v2/libs/${lib}/${OS_SPEC}/; \
done ;

sed -i -E 's/^(BOOST_VERSION_DIR)\=.+$/\1=boost-1.64/' /keytalk/Software/mk/keytalk.common.mk


5. Rebuild KeyTalk and re-run all tests

6. Add new files to the version control and commit your changes
