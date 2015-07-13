
# morituri -- the COMMercial2OSM converter

comm2osm creates an OSM (OpenStreetMap) PBF/XML/etc. file from commercial shapefiles to be able to use OSM tools on the data. <br>

[![Build Status](https://secure.travis-ci.org/knowname/morituri.png)](http://travis-ci.org/knowname/morituri)

The architecture is plugin based. Currently there is a plugin (navteq) for converting routable Navteq data from the "NAVSTREETS Street Data Reference Manual v5.4" format into OSM format.

WARNING: DO NOT UPLOAD CONVERTED DATA TO OSM.
Even if you were legally allowed to do so, imports into OSM
are problematic due to the following reasons:

* the converted data is not clean for imports
* duplicates (deduplication is hard)
* hinders community development
* data might be bad - reviews missing
* shallow data

# Prerequisites
* libosmium
* bz2 
* gdal 
* expat
* geos
* pthread
* libz
* osmpbf
* protobuf-lite
* boost
* boost-filesystem
* shapelib ([http://download.osgeo.org/shapelib/](http://download.osgeo.org/shapelib/))

##### For Testing
* gcc-4.9
* gdal-bin (for ogr2ogr)


#### Ubuntu install instructions: *(tested with 14.04.2 LTS)*

CAUTION: Have a look at `install_prerequisites.sh` before executing it.
Because gcc-4.9 isn't in Ubuntu 14.04 LTS yet this script adds the apt-repository `ppa:ubuntu-toolchain-r/test`.
Along with this repository you may get asked to update grub.
 
Install prerequisits with the `install_prerequisites.sh`. You may have to make this executable 
(e.g. with `chmod +x install_prerequisites.sh`).

build with: `make -j2`

run with:	`./comm2osm /path/to/navteq/data/ output_file.{desired-output-format}`

e.g.
 `./comm2osm ~/navteq-testdata/ ~/navteq-testdata/routable.osm` to produce 
an XML file <br> or `./comm2osm ~/navteq-testdata/ ~/navteq-testdata/routable.pbf` 
to produce a PBF file.

### Test data

If you want to test this program and you don't have data of your own you may get sample downloads from the following list:

* [Test data in NAVSTREETS format](http://www.navmart.com/download.php)

---

# For users

## Output has been tested with:
* osm2pgsql
* josm
* Maperitive

### Untested:
* osmosis

### Issues:
##### Maperitive:
* The plugin makes use of oneway="-1" tag, due to how the Navteq data is aligned.
  Most OSM programmes can handle this, but Maperitive ignores it.

---

## Simplifications:
- 	The Navteq to osm admin-level mapping is currently navteq-admin-level * 2. 
	This provides a simple mapping which works for the current 
	testdata, but may fail on datasets outside the U.S.
- 	All textual tag values are camel case with spaces. The correct naming 
   	schemes may be obtained from the Navteq data but is missing for now.
- 	Turn restrictions are always mapped as `no_straight_on`. To do this 
   	correctly, we will have to check the direction of the geometries. This is
   	currently not implemented because the correctness is not guaranteed and the
   	router only uses the information to show the correct traffic sign to the
   	user. The actual turn restriction will be respected. 
-	Inner rings in administrative boundaries are currently ignored.
-	In Navteq, z_levels are stored in nodes, hence ways in Navteq may have 
	different z_levels. 
	By contrast, OSM stores z_levels in ways ("layer" tag), which implies that a way in OSM 
	can only have a single z_level.
	Because of that a single Navteq way with different z_levels has to be split
	into several OSM ways to be able to apply the z_levels.
	
## Limitations:
-  In Navteq the z-levels are applied per node. In OSM z-levels are applied per way. Hence the data must be converted from the Navteq style to the OSM style. This simplification can lead to the two datasets not being the same.
   e.g. the following five nodes (with 3 different z level values) would be mapped to the two ways (with 2 different z level values) below: 
  
         node---node---node---node---node       
          z0     z1     z0     z2     z0
                         |
                         V
           |-----way-----|-----way-----|
               z1            z2
           
   To understand the mapping look at the test cases in `test_comm2osm.cpp`.
  
## Notes:
- Sometimes, in Navteq data there are multiple copies of administrative boundaries -- should be investigated.
  
---
  
  
  
# For developers

Feel free to add plugins to convert data from other suppliers.<br>
There is only a single real test. More tests are welcome.

## Notes:

- Object ids are assigned consecutively, in ascending order, from one. There will be no negative object ids.
 

## Pitfalls:

   - In libosmium, all OSM objects live in buffers to ensure that objects can 
	 only be built by objects builders. To be able to create an object with an 
	 object builder, you have to pay attention to the following:
   - A builder *must* have a user-tag - even if the tag is empty e.g. 
   	 `builder.add_user("");` Otherwise the buffer is not aligned and 
   	 `buffer.commit()` will cause a crash.
   - Before you can call `buffer.commit()` you have to be sure that the 
   	 destructor is called, because osmium alignes the buffer in the 
   	 destructor of the builder.
   - When creating objects with tags and/or objects which reference other 
   	 objects, you have to create several builders successively.<br>
   	 E.g. to create a relation with tags and members you first have to
   	 create a `RelationBuilder`, then a `TagListBuilder`, and then 
   	 a `RelationMemberListBuilder`.
   	 It is crucial that the destructor of the `TagListBuilder` is called
   	 before the `RelationMemberListBuilder` writes anything to the 
   	 buffer, otherwise the buffer may not be aligned.
   	 The easiest way to call the destructor of `TagListBuilder` is to
   	 make the `TagListBuilder` go out of scope.
   	 (see `process_admin_boundaries()` in `navteq.hpp`)
   	
  *If your program fails with: Assertion `buffer.is_aligned()` failed, then you have probably fallen for one of these pitfalls.*
  	   
---
  
# Testing

Testing is done with Catch. Currently we are using this 
[Catch fork](https://github.com/kentsangkm/Catch/) so that eclipse can process catch.hpp
correctly.
Also see [this pull request](https://github.com/philsquared/Catch/pull/393).
As soon as eclipse fixes 
[this issue](https://bugs.eclipse.org/bugs/show_bug.cgi?id=455501)
you may switch to the 
[Catch master branch](https://github.com/philsquared/Catch).

If you run tests from terminal use the project root directory and call it with:

> ./tests/navteq_test

If you run tests with eclipse simply execute the test-binary.

Tests are currently only covering the `z_level` mapping from Navteq nodes to 
OSM ways. 

* many testcases for splitting the ways with different `z_level`

More tests are welcome.

# License

morituri is licensed under the GNU General Public License v3 (GPL-3). See LICENSE.txt.


# Credits

morituri was developed at Geofabrik as part of the TOTARI project, which is sponsored by the 
German Ministry of Education and Research (Bundesministeriums für Bildung und Forschung)


---
<img src="images/bmbf.jpeg" height="150"> 
<img src="images/PT-DLR-englisch.jpg" height="80" vspace="30">
<img src="images/geofabrik.png" height="90" vspace="30" hspace="20">

##### *Das diesem Bericht zugrundeliegende Vorhaben wurde mit Mitteln des Bundesministeriums für Bildung und Forschung unter dem Förderkennzeichen 01IS13033D gefördert. Die Verantwortung für den Inhalt dieser Veröffentlichung liegt beim Autor.*
