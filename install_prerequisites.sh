#!/bin/bash
sudo apt-get -y install gcc-4.8 g++-4.8
sudo apt-get -y install gdal-bin
sudo apt-get -y install libbz2-dev libgdal-dev libexpat1-dev libgeos++-dev libpthread-stubs0-dev zlib1g-dev libosmpbf-dev libprotobuf-dev libboost-dev libboost-filesystem-dev
sudo apt-get -y install git 

mkdir -p ~/libs
cd ~/libs
git clone https://github.com/osmcode/libosmium

curl http://download.osgeo.org/shapelib/shapelib-1.3.0.tar.gz -o shapelib-1.3.0.tar.gz
tar xzf shapelib-1.3.0.tar.gz
rm shapelib-1.3.0.tar.gz
cd shapelib-1.3.0
make
sudo make install
sudo chmod 644 /usr/local/include/shapefil.h

