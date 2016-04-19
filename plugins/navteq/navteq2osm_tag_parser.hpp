#ifndef NAVTEQ2OSMTAGPARSE_HPP_
#define NAVTEQ2OSMTAGPARSE_HPP_

#include <iostream>
#include <fstream>

#include "navteq_util.hpp"
#include "navteq_mappings.hpp"
#include "navteq_types.hpp"

boost::filesystem::path g_executable_path;
const boost::filesystem::path PLUGINS_NAVTEQ_ISO_639_2_UTF_8_TXT("plugins/navteq/ISO-639-2_utf-8.txt");
int ctr = 0;

// helper
bool parse_bool(const char* value) {
    if (!strcmp(value, "Y")) return true;
    return false;
}

bool fits_street_ref(std::string& st_name) {
    if (st_name.size() == 0) return false;
    if (st_name.size() > 6) return false;
    
    bool number_started = false;
    for (auto it = st_name.begin(); it != st_name.end(); ++it) {
        if (std::isdigit(*it)) {
             number_started = true;
        } else if (number_started) {
            return false;
        }
    }
    
    return number_started;
}

bool begins_with(std::string& str, const char* start_str) {
    return ( strncmp(str.c_str(), start_str, strlen(start_str)) == 0 );
}

uint get_number_after(std::string& str, const char* start_str) {
    if( !begins_with(str, start_str) )
        return 0; /* doesn't start with start_str */
    
    /* Get number string after start_str until first non-digit appears */
    std::string end_str = str.substr(strlen(start_str));
    std::string number_str;
    for (auto it = end_str.begin(); it != end_str.end(); ++it) {
        if (std::isdigit(*it)) {
            number_str += *it;
        } else {
            /* break because B107a should return 107*/
            break;
        }
    }
    
    try {
        return std::stoul( number_str );
    } catch (const std::invalid_argument &) {
        return 0;
    }
}

bool is_motorized_allowed(ogr_feature_uptr& f) {
    if (parse_bool(get_field_from_feature(f, AR_AUTO)))
        return true;
    if (parse_bool(get_field_from_feature(f, AR_BUS)))
        return true;
    if (parse_bool(get_field_from_feature(f, AR_TAXIS)))
        return true;
    if (parse_bool(get_field_from_feature(f, AR_TRUCKS)))
        return true;
    if (parse_bool(get_field_from_feature(f, AR_EMERVEH)))
        return true;
    if (parse_bool(get_field_from_feature(f, AR_MOTORCYCLES)))
        return true;
    
    return false;
}

uint get_area_code_l(ogr_feature_uptr& f, mtd_area_map_type* mtd_area_map = nullptr) {
    area_id_type l_area_id = get_uint_from_feature(f, L_AREA_ID);
    area_id_type r_area_id = get_uint_from_feature(f, R_AREA_ID);

    area_id_type area_id;
    if (mtd_area_map->find(l_area_id) != mtd_area_map->end())
        area_id = l_area_id;
    else if (mtd_area_map->find(r_area_id) != mtd_area_map->end())
        area_id = r_area_id;
    else
        std::cerr << "could not find area_id " << ++ctr << ", " << mtd_area_map->size() << std::endl;

    return mtd_area_map->at(area_id).area_code_1;
}

std::vector<std::string> get_hwy_vector(std::map<int, std::vector<std::string>> const HWY_TYPE_MAP, uint area_code_1) {
	std::vector<std::string> hwy_route_type_vec;
	if (HWY_TYPE_MAP.find(area_code_1) != HWY_TYPE_MAP.end()) {
		hwy_route_type_vec = HWY_TYPE_MAP.at(area_code_1);
	} else {
//		std::cerr << "could not find area_id " << area_code_1 << " use default" << std::endl;
		hwy_route_type_vec = HWY_TYPE_MAP.at(0);
	}
	return hwy_route_type_vec;
}

std::string get_hwy_value(ushort route_type, uint area_code_1, std::string& ref_name) {
    /* some exceptional cases for better route type parsing */
    if (area_code_1 == 5 && route_type == 3) {          /*"BEL"*/
         /* N# and N## is like PRIMARY
          * N### SECONDARY
          * N#### TERTIARY */
        uint hwy_num = get_number_after(ref_name, "N");
        if (hwy_num > 999)
            return TERTIARY;
        if (hwy_num > 99)
            return SECONDARY;
    } else if (area_code_1 == 9) {                      /*"AUT"*/
        if (route_type == 4) {
            // Type 4 streets named B### are primary otherwise secondary
            if (!begins_with(ref_name, "B"))
                return SECONDARY;
        } else if (route_type == 5) {
            // Type 5 streets named L### are secondary otherwise tertiary
            if (begins_with(ref_name, "L"))
                return SECONDARY;
        }
    } else if (area_code_1 == 23 && route_type == 2) {  /*"IRL"*/
        /* N## is TRUNK if ## smaller 50 otherwise PRIMARY */
        uint hwy_num = get_number_after(ref_name, "N");
        if (hwy_num > 0 && hwy_num < 50)
            return TRUNK;
    }
    
    /* default case */
    return get_hwy_vector(HWY_ROUTE_TYPE_MAP, area_code_1).at(route_type);
}

void add_highway_tag(osmium::builder::TagListBuilder* builder, ogr_feature_uptr& f, link_id_type link_id,
        ushort route_type, ushort func_class, mtd_area_map_type* mtd_area_map = nullptr, std::string ref_name = "") {
    const char* highway = "highway";
    bool paved = parse_bool(get_field_from_feature(f, PAVED));
    bool motorized_allowed = is_motorized_allowed(f);
    
    if (!paved) {
        if (!motorized_allowed) {
            //unpaved + non-motorized => path
            builder->add_tag(highway, PATH);
        } else {
            //unpaved + motorized allowed => track
            builder->add_tag(highway, TRACK);
        }
    } else {
        if (!motorized_allowed) {
            //paved + non-motorized => footway
            //it seems impossible to distinguish footways from cycle ways or pedestrian zones
            builder->add_tag(highway, FOOTWAY);
        } else {
            // paved + motorized allowed
            bool controlled_access = parse_bool(get_field_from_feature(f, CONTRACC));
            bool urban = parse_bool(get_field_from_feature(f, URBAN));
            uint area_code_1 = get_area_code_l(f, mtd_area_map);

            if (controlled_access) {
                // controlled_access => motorway 
                builder->add_tag(highway, MOTORWAY);
            } else if (route_type) {
                std::string hwy_value = get_hwy_value(route_type, area_code_1, ref_name);
                if (!hwy_value.empty()) {
                    builder->add_tag(highway, hwy_value);
                } else {
                    std::cerr << "ignoring highway_level'" << std::to_string(route_type) 
                            << "' for " << area_code_1 << std::endl;
                }
            } else if (func_class) {               
                std::vector<std::string> hwy_func_class_vec = get_hwy_vector(HWY_FUNC_CLASS_MAP, area_code_1);
                uint apply_func_class = func_class;
                if (apply_func_class > 4 && urban)
                    apply_func_class++;
                
                builder->add_tag(highway, hwy_func_class_vec.at(apply_func_class));
            } else {
                std::cerr << " highway misses route_type and func_class! ";
            }
        }
    }
}

const char* parse_one_way_tag(const char* value) {
    const char* one_way = "oneway";
    if (!strcmp(value, "F"))				// F --> FROM reference node
        return YES;
    else if (!strcmp(value, "T"))			// T --> TO reference node
        return "-1";    // todo reverse way instead using "-1"
    else if (!strcmp(value, "B"))			// B --> BOTH ways are allowed
        return nullptr;
    throw(format_error("value '" + std::string(value) + "' for oneway not valid"));
}

void add_one_way_tag(osmium::builder::TagListBuilder* builder, const char* value) {
    const char* one_way = "oneway";
    const char* parsed_value = parse_one_way_tag(value);
    if (parsed_value) builder->add_tag(one_way, parsed_value);
}

void add_access_tags(osmium::builder::TagListBuilder* builder, ogr_feature_uptr& f) {
    if (! parse_bool(get_field_from_feature(f, AR_AUTO))) builder->add_tag("motorcar",  NO);
    if (! parse_bool(get_field_from_feature(f, AR_BUS))) builder->add_tag("bus",  NO);
    if (! parse_bool(get_field_from_feature(f, AR_TAXIS))) builder->add_tag("taxi",  NO);
//    if (! parse_bool(get_field_from_feature(f, AR_CARPOOL))) builder->add_tag("hov",  NO);
    if (! parse_bool(get_field_from_feature(f, AR_PEDESTRIANS))) builder->add_tag("foot",  NO);
    if (! parse_bool(get_field_from_feature(f, AR_TRUCKS))) builder->add_tag("hgv", NO);
    if (! parse_bool(get_field_from_feature(f, AR_EMERVEH))) builder->add_tag("emergency",  NO);
    if (! parse_bool(get_field_from_feature(f, AR_MOTORCYCLES))) builder->add_tag("motorcycle",  NO);
    if (!parse_bool(get_field_from_feature(f, PUB_ACCESS)) || parse_bool(get_field_from_feature(f, PRIVATE))){
        builder->add_tag("access", "private");
    } else if (!parse_bool(get_field_from_feature(f, AR_THROUGH_TRAFFIC))){
        builder->add_tag("access", "destination");
    }
}

/**
 * \brief adds maxspeed tag
 */
void add_maxspeed_tags(osmium::builder::TagListBuilder* builder, ogr_feature_uptr& f) {
    const char* from_speed_limit_s = strdup(get_field_from_feature(f, FR_SPEED_LIMIT));
    const char* to_speed_limit_s = strdup(get_field_from_feature(f, TO_SPEED_LIMIT));

    uint from_speed_limit = get_uint_from_feature(f, FR_SPEED_LIMIT);
    uint to_speed_limit = get_uint_from_feature(f, TO_SPEED_LIMIT);

    if (from_speed_limit >= 1000 || to_speed_limit >= 1000)
        throw(format_error(
                "from_speed_limit='" + std::string(from_speed_limit_s) + "' or to_speed_limit='"
                        + std::string(to_speed_limit_s) + "' is not valid (>= 1000)"));

    // 998 is a ramp without speed limit information
    if (from_speed_limit == 998 || to_speed_limit == 998)
        return;

    // 999 means no speed limit at all
    const char* from = from_speed_limit == 999 ? "none" : from_speed_limit_s;
    const char* to = to_speed_limit == 999 ? "none" : to_speed_limit_s;

    if (from_speed_limit != 0 && to_speed_limit != 0) {
        if (from_speed_limit != to_speed_limit) {
            builder->add_tag("maxspeed:forward", from);
            builder->add_tag("maxspeed:backward", to);
        } else {
            builder->add_tag("maxspeed", from);
        }
    } else if (from_speed_limit != 0 && to_speed_limit == 0) {
        builder->add_tag("maxspeed", from);
    } else if (from_speed_limit == 0 && to_speed_limit != 0) {
        builder->add_tag("maxspeed", to);
    }

    if (from_speed_limit > 130 && from_speed_limit < 999)
        std::cerr << "Warning: Found speed limit > 130 (from_speed_limit): " << from_speed_limit << std::endl;
    if (to_speed_limit > 130 && to_speed_limit < 999)
        std::cerr << "Warning: Found speed limit > 130 (to_speed_limit): " << to_speed_limit << std::endl;
}

/**
 * \brief adds here:speed_cat tag
 */
void add_here_speed_cat_tag(osmium::builder::TagListBuilder* builder, ogr_feature_uptr& f) {
    auto speed_cat = get_uint_from_feature(f, SPEED_CAT);
    if (0 < speed_cat && speed_cat < (sizeof(speed_cat_metric) / sizeof(const char*))) builder->add_tag(
            "here:speed_cat", speed_cat_metric[speed_cat]);
    else throw format_error("SPEED_CAT=" + std::to_string(speed_cat) + " is not valid.");
}

/**
 * \brief check if unit is imperial. area_id(Streets.dbf) -> govt_id(MtdArea.dbf) -> unit_measure(MtdCntryRef.dbf)
 * \param area_id area_id
 * \param area_govt_map maps area_ids to govt_codes
 * \param cntry_map maps govt_codes to cntry_ref_types
 * \return returns false if any of the areas contain metric units or if its unclear
 */
bool is_imperial(area_id_type area_id, area_id_govt_code_map_type* area_govt_map,
        cntry_ref_map_type* cntry_map) {
    if (area_govt_map->find(area_id) != area_govt_map->end()) {
        if (cntry_map->find(area_govt_map->at(area_id)) != cntry_map->end()) {
            auto unit_measure = cntry_map->at(area_govt_map->at(area_id)).unit_measure;
            if (unit_measure == 'E') {
                return true;
            } else if (unit_measure != 'M'){
                format_error("unit_measure in navteq data is invalid: '" + std::to_string(unit_measure) + "'");
            }
        }
    }
    return false;
}

/**
 * \brief check if unit is imperial. area_id(Streets.dbf) -> govt_id(MtdArea.dbf) -> unit_measure(MtdCntryRef.dbf)
 * \param l_area_id area_id on the left side of the link
 * \param r_area_id area_id on the right side of the link
 * \param area_govt_map maps area_ids to govt_codes
 * \param cntry_map maps govt_codes to cntry_ref_types
 * \return returns false if any of the areas contain metric units or if its unclear
 */
bool is_imperial(area_id_type l_area_id, area_id_type r_area_id, area_id_govt_code_map_type* area_govt_map,
        cntry_ref_map_type* cntry_map) {
    if (is_imperial(l_area_id, area_govt_map, cntry_map)) return true;
    if (is_imperial(r_area_id, area_govt_map, cntry_map)) return true;
    return false;
}

void add_hazmat_tag(osmium::builder::TagListBuilder* builder, mod_val_type mod_val) {
    if (mod_val == 20) { // || mod_val == 21
        builder->add_tag("hazmat", "no");
    } else if (mod_val == 22) {
        builder->add_tag("hazmat:water", "no");
    } else if (mod_val == 24) {
        builder->add_tag("hazmat:B", "no");
    } else if (mod_val == 28) {
        builder->add_tag("hazmat:C", "no");
    } else if (mod_val == 32) {
        builder->add_tag("hazmat:D", "no");
    } else if (mod_val == 34) {
        builder->add_tag("hazmat:E", "no");
    } else if (mod_val == 23) {
         /* 23 = Explosive and Flammable */
    } else {
        /**
         * Do nothing for the residual values,
         * which do not occur and/or have no proper OSM tagging equivalent
         * 1 = Explosives
         * 2 = Gas
         * 3 = Flammable
         * 4 = Flammable solid/Combustible
         * 5 = Organic
         * 6 = Poison
         * 7 = Radioactive
         * 8 = Corrosive
         * 9 = Other
         */
        std::cerr << "Hazardous material value " << mod_val << " hasn't been parsed!" << std::endl;
    }
}

/**
 * \brief adds maxheight, maxwidth, maxlength, maxweight and maxaxleload tags.
 */
void add_additional_restrictions(osmium::builder::TagListBuilder* builder, link_id_type link_id, area_id_type l_area_id,
        area_id_type r_area_id, cdms_map_type* cdms_map, cnd_mod_map_type* cnd_mod_map,
        area_id_govt_code_map_type* area_govt_map, cntry_ref_map_type* cntry_map) {
    if (!cdms_map || !cnd_mod_map) return;

    // default is metric units
    bool imperial_units = false;
    if (area_govt_map && cntry_map ){
        imperial_units = is_imperial(l_area_id, r_area_id, area_govt_map, cntry_map);
    }

    uint64_t max_height = 0, max_width = 0, max_length = 0, max_weight = 0, max_axleload = 0;

	std::vector<mod_group_type> mod_group_vector;
	auto range = cdms_map->equal_range(link_id);
	for (auto it = range.first; it != range.second; ++it) {
		cond_id_type cond_id = it->second;
		auto it2 = cnd_mod_map->find(cond_id);
		if (it2 != cnd_mod_map->end()) {
			auto mod_group = it2->second;
			mod_group_vector.push_back(mod_group);
		}
	}

	for (auto mod_group : mod_group_vector) {
		auto mod_type = mod_group.mod_type;
		auto mod_val = mod_group.mod_val;
		if (mod_type == MT_HEIGHT_RESTRICTION) {
                    if (!max_height || mod_val < max_height) max_height = mod_val;
		} else if (mod_type == MT_WIDTH_RESTRICTION) {
                    if (!max_width || mod_val < max_width) max_width = mod_val;
		} else if (mod_type == MT_LENGTH_RESTRICTION) {
                    if (!max_length || mod_val < max_length) max_length = mod_val;
		} else if (mod_type == MT_WEIGHT_RESTRICTION) {
                    if (!max_weight || mod_val < max_weight) max_weight = mod_val;
		} else if (mod_type == MT_WEIGHT_PER_AXLE_RESTRICTION) {
                    if (!max_axleload || mod_val < max_axleload) max_axleload = mod_val;
		} else if (mod_type == MT_HAZARDOUS_RESTRICTION) {
                    add_hazmat_tag(builder, mod_val);
		}
	}

	if (max_height > 0) builder->add_tag("maxheight", imperial_units ? inch_to_feet(max_height) : cm_to_m(max_height));
	if (max_width > 0)  builder->add_tag("maxwidth", imperial_units ? inch_to_feet(max_width) : cm_to_m(max_width));
	if (max_length > 0)  builder->add_tag("maxlength", imperial_units ? inch_to_feet(max_length) : cm_to_m(max_length));
	if (max_weight > 0)  builder->add_tag("maxweight", imperial_units ? lbs_to_metric_ton(max_weight) : kg_to_t(max_weight));
	if (max_axleload > 0)  builder->add_tag("maxaxleload", imperial_units ? lbs_to_metric_ton(max_axleload) : kg_to_t(max_axleload));
}

bool is_ferry(const char* value) {
    if (!strcmp(value, "H")) return false;      // H --> not a ferry
    else if (!strcmp(value, "B")) return true;  // T --> boat ferry
    else if (!strcmp(value, "R")) return true;  // B --> rail ferry
    throw(format_error("value '" + std::string(value) + "' for " + std::string(FERRY) + " not valid"));
}

bool only_pedestrians(ogr_feature_uptr& f) {
    if (strcmp(get_field_from_feature(f, AR_PEDESTRIANS), "Y")) return false;
    if (! strcmp(get_field_from_feature(f, AR_AUTO),"Y")) return false;
    if (! strcmp(get_field_from_feature(f, AR_BUS),"Y")) return false;
//    if (! strcmp(get_field_from_feature(f, AR_CARPOOL),"Y")) return false;
    if (! strcmp(get_field_from_feature(f, AR_EMERVEH),"Y")) return false;
    if (! strcmp(get_field_from_feature(f, AR_MOTORCYCLES),"Y")) return false;
    if (! strcmp(get_field_from_feature(f, AR_TAXIS),"Y")) return false;
    if (! strcmp(get_field_from_feature(f, AR_THROUGH_TRAFFIC),"Y")) return false;
    return true;
}

void add_ferry_tag(osmium::builder::TagListBuilder* builder, ogr_feature_uptr& f) {
    const char* ferry = get_field_from_feature(f, FERRY);
    builder->add_tag("route", "ferry");
    if (!strcmp(ferry, "B")) {
        if (only_pedestrians(f)) {
            builder->add_tag("foot", YES);
        } else {
            builder->add_tag("foot", parse_bool(get_field_from_feature(f, AR_PEDESTRIANS)) ? YES : NO);
            builder->add_tag("motorcar", parse_bool(get_field_from_feature(f, AR_AUTO)) ? YES : NO);
        }

    } else if (!strcmp(ferry, "R")) {
        builder->add_tag("railway", "ferry");
    } else throw(format_error("value '" + std::string(ferry) + "' for " + std::string(FERRY) + " not valid"));
}

void add_lanes_tag(osmium::builder::TagListBuilder* builder, ogr_feature_uptr& f) {
    const char* number_of_physical_lanes = get_field_from_feature(f, PHYS_LANES);
    if (strcmp(number_of_physical_lanes, "0")) builder->add_tag("lanes", number_of_physical_lanes);
}

void add_postcode_tag(osmium::builder::TagListBuilder* builder, ogr_feature_uptr& f) {
	std::string l_postcode = get_field_from_feature(f, L_POSTCODE);
	std::string r_postcode = get_field_from_feature(f, R_POSTCODE);

	if (l_postcode.empty() && r_postcode.empty()) return;

	std::string postcode;
	if (l_postcode == r_postcode) postcode = l_postcode;
	else postcode = l_postcode + ";" + r_postcode;

	builder->add_tag("addr:postcode", postcode);
}

std::string add_highway_name_tags(osmium::builder::TagListBuilder *builder, link_id_to_names_map* names_map, link_id_type link_id) {
    std::string ref_tag;
    if (names_map->find(link_id) != names_map->end()) {
        std::vector<std::string> &highway_names_vector = names_map->at(link_id);
        std::string int_ref_tag;
       
        for (auto it : highway_names_vector) {
            
            if (ref_tag.empty() || !ref_tag.compare(0, 1, "E"))
                ref_tag = it;
            
            if (!it.compare(0, 1, "E"))
                int_ref_tag = it;
             
            //TODO add to int_ref if way is asian highway ("AH")
        }
        
        if (!ref_tag.empty()) builder->add_tag("ref", ref_tag.c_str());
        if (!int_ref_tag.empty()) builder->add_tag("int_ref", int_ref_tag.c_str());    
    }
    
    return ref_tag;
}

void add_highway_tags(osmium::builder::TagListBuilder* builder, ogr_feature_uptr& f, link_id_type link_id,
		ushort route_type, mtd_area_map_type* mtd_area_map = nullptr, std::string ref_name = "") {

	ushort func_class = 0;
	std::string func_class_s = get_field_from_feature(f, FUNC_CLASS);
	if (!func_class_s.empty()) func_class = get_uint_from_feature(f, FUNC_CLASS);
	
        add_highway_tag(builder, f, link_id, route_type, func_class, mtd_area_map, ref_name);

	add_one_way_tag(builder, get_field_from_feature(f, DIR_TRAVEL));
	add_access_tags(builder, f);
	add_maxspeed_tags(builder, f);
	add_lanes_tag(builder, f);
	add_postcode_tag(builder, f);

	if (parse_bool(get_field_from_feature(f, PAVED))) builder->add_tag("surface", "paved");
	if (parse_bool(get_field_from_feature(f, BRIDGE))) builder->add_tag("bridge", YES);
	if (parse_bool(get_field_from_feature(f, TUNNEL))) builder->add_tag("tunnel", YES);
	if (parse_bool(get_field_from_feature(f, TOLLWAY))) builder->add_tag("toll", YES);
	if (parse_bool(get_field_from_feature(f, ROUNDABOUT))) builder->add_tag("junction", "roundabout");
	if (parse_bool(get_field_from_feature(f, FOURWHLDR))) builder->add_tag("4wd_only", YES);
}

/**
 * \brief maps navteq tags for access, tunnel, bridge, etc. to osm tags
 * \return link id of processed feature.
 */
link_id_type parse_street_tags(osmium::builder::TagListBuilder *builder, ogr_feature_uptr& f, cdms_map_type* cdms_map =
        nullptr, cnd_mod_map_type* cnd_mod_map = nullptr, area_id_govt_code_map_type* area_govt_map = nullptr,
		cntry_ref_map_type* cntry_map = nullptr, mtd_area_map_type* mtd_area_map = nullptr,
                link_id_route_type_map* route_type_map = nullptr, link_id_to_names_map* names_map = nullptr) {

    const char* link_id_s = get_field_from_feature(f, LINK_ID);
    link_id_type link_id = std::stoul(link_id_s);
    builder->add_tag(LINK_ID, link_id_s); // tag for debug purpose
    
    ushort route_type = 0;
    if ( !((std::string)get_field_from_feature(f, ROUTE)).empty() )
        route_type = get_uint_from_feature(f, ROUTE);
    if ( route_type_map->find(link_id) != route_type_map->end()
            && ( !route_type || route_type_map->at(link_id) < route_type ))
        route_type = route_type_map->at(link_id);
        
    //add tags for ref and int_ref to major highways
    std::string ref_name = add_highway_name_tags(builder, names_map, link_id);

    std::string street_name = to_camel_case_with_spaces(get_field_from_feature(f, ST_NAME));
    if (!street_name.empty())
        builder->add_tag("name", street_name.c_str());
    if (is_ferry(get_field_from_feature(f, FERRY))) {
        add_ferry_tag(builder, f);
    } else {  // usual highways
        add_highway_tags(builder, f, link_id, route_type, mtd_area_map, ref_name);
    }

    area_id_type l_area_id = get_uint_from_feature(f, L_AREA_ID);
    area_id_type r_area_id = get_uint_from_feature(f, R_AREA_ID);
    // tags which apply to highways and ferry routes
    add_additional_restrictions(builder, link_id, l_area_id, r_area_id, cdms_map, cnd_mod_map, area_govt_map,
            cntry_map);
    add_here_speed_cat_tag(builder, f);
    if (parse_bool(get_field_from_feature(f, TOLLWAY))) builder->add_tag("here:tollway", YES);
    if (parse_bool(get_field_from_feature(f, URBAN))) builder->add_tag("here:urban", YES);
    if (parse_bool(get_field_from_feature(f, CONTRACC))) builder->add_tag("here:controll_access", YES);
    if (route_type) add_uint_tag(builder, "here:route_type", route_type);


    std::string func_class = get_field_from_feature(f, FUNC_CLASS);
    if (!func_class.empty()) builder->add_tag("here:func_class", func_class.c_str());

    add_uint_tag(builder, "here:area_code", get_area_code_l(f, mtd_area_map));
    
    return link_id;
}

const char* get_place_value(uint population, uint capital) {
    if ( capital == 1 || capital == 2
            || population > 100000)
        return "city";
    
    if (  capital == 3 || capital == 4
            || population > 10000)
        return "town";
    
    if (  capital == 5
            || population > 100)
        return "village";
    
    if ( population > 0 )
        return "hamlet";
    
    //population = 0 is more often a village than a hamlet
    return "village";
}

// matching from http://www.loc.gov/standards/iso639-2/php/code_list.php
// http://www.loc.gov/standards/iso639-2/ISO-639-2_utf-8.txt
// ISO-639 conversion
std::map<std::string, std::string> g_lang_code_map;
void parse_lang_code_file() {
    if(g_executable_path.empty()) throw(std::runtime_error("executable_path is empty"));

    boost::filesystem::path iso_file(g_executable_path/PLUGINS_NAVTEQ_ISO_639_2_UTF_8_TXT);
    std::ifstream file(iso_file.string());
    assert(file.is_open());
    std::string line;
    std::string delim = "|";
    if (file.is_open()){
        while (getline(file, line, '\n')){
            std::vector<std::string> lv;
            auto start = 0u;
            auto end = line.find(delim);
            while (end != std::string::npos){
                lv.push_back(line.substr(start, end-start));
                start = end + delim.length();
                end = line.find(delim, start);
            }
            std::string iso_639_2 = lv.at(0);
            std::string iso_639_1 = lv.at(2);
            if (!iso_639_1.empty()) g_lang_code_map.insert(std::make_pair(iso_639_2, iso_639_1));
        }
        file.close();
    }
}

std::string parse_lang_code(std::string lang_code){
    std::transform(lang_code.begin(), lang_code.end(), lang_code.begin(), ::tolower);
    if (g_lang_code_map.empty()) parse_lang_code_file();
    if (g_lang_code_map.find(lang_code) != g_lang_code_map.end()) return g_lang_code_map.at(lang_code);
    std::cerr << lang_code << " not found!" << std::endl;
    throw std::runtime_error("Language code '" + lang_code + "' not found");
}

std::string navteq_2_osm_admin_lvl(std::string navteq_admin_lvl) {
    if (string_is_not_unsigned_integer(navteq_admin_lvl)) throw std::runtime_error("admin level contains invalid character");

    int navteq_admin_lvl_int = stoi(navteq_admin_lvl);

    if (!is_in_range(navteq_admin_lvl_int, NAVTEQ_ADMIN_LVL_MIN, NAVTEQ_ADMIN_LVL_MAX))
        throw std::runtime_error(
                "invalid admin level. admin level '" + std::to_string(navteq_admin_lvl_int) + "' is out of range.");
    return std::to_string(2 * std::stoi(navteq_admin_lvl)).c_str();
}

const char* parse_house_number_schema(const char* schema){
    if (!strcmp(schema, "E")) return "even";
    if (!strcmp(schema, "O")) return "odd";
    std::cerr << "schema = " << schema << " unsupported" << std::endl;
    return "";
    throw std::runtime_error("scheme "+std::string(schema)+" is currently not supported");
}

#endif /* NAVTEQ2OSMTAGPARSE_HPP_ */

