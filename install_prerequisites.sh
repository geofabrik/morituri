#!/bin/bash
sudo apt-get -y install software-properties-common
sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
sudo apt-get -y update
sudo apt-get -y upgrade
sudo apt-get -y install gcc-4.9 
sudo apt-get -y install gdal-bin
sudo apt-get -y install libbz2-dev libgdal-dev libexpat1-dev libgeos++-dev libpthread-stubs0-dev zlib1g-dev libosmpbf-dev libprotobuf-dev libboost-dev libboost-filesystem-dev

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

sudo apt-get -y update
sudo apt-get -y upgrade
