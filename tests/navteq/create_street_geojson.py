#!/usr/bin/python
import sys

if __name__ == '__main__':
    #z_lvls = [0,4,4,0,0,0,5,5,0,0]
    z_lvls = sys.argv[1:]

    l = len(z_lvls)

    print """{
    "type":"Feature",
    "geometry":{
        "type":"LineString",
        "coordinates":["""
    
    x = 10.000
    y = 10.000
    for i in range(0,l):
        print ("[%s,%s]"%(x,y))
        if i < l-1:
            print ","

        x = x+0.001
        y = y+0.001
    print """],
        "properties":{}
    },  
    "properties":{
        "LINK_ID":"2147483647",
        "ST_NAME":"E20 ",
        "FEAT_ID":"719409379",
        "ST_LANGCD":"DAN",
        "NUM_STNMES":"1",
        "ST_NM_PREF":"",
        "ST_TYP_BEF":" ",
        "ST_NM_BASE":"E20 ",
        "ST_NM_SUFF":"",
        "ST_TYP_AFT":" ",
        "ST_TYP_ATT":"N",
        "ADDR_TYPE":"",
        "L_REFADDR":" ",
        "L_NREFADDR":" ",
        "L_ADDRSCH":"",
        "L_ADDRFORM":"",
        "R_REFADDR":" ",
        "R_NREFADDR":" ",
        "R_ADDRSCH":"",
        "R_ADDRFORM":"",
        "REF_IN_ID":"93201823",
        "NREF_IN_ID":"85759357",
        "N_SHAPEPNT":"0",
        "FUNC_CLASS":"1",
        "SPEED_CAT":"2",
        "FR_SPD_LIM":"110",
        "TO_SPD_LIM":"0",
        "TO_LANES":"0",
        "FROM_LANES":"3",
        "ENH_GEOM":"Y",
        "LANE_CAT":"2",
        "DIVIDER":"N",
        "DIR_TRAVEL":"F",
        "L_AREA_ID":"20367962",
        "R_AREA_ID":"20367962",
        "L_POSTCODE":"5500 ",
        "R_POSTCODE":"5500 ",
        "L_NUMZONES":"0",
        "R_NUMZONES":"0",
        "NUM_AD_RNG":"0",
        "AR_AUTO":"Y",
        "AR_BUS":"Y",
        "AR_TAXIS":"Y",
        "AR_CARPOOL":"Y",
        "AR_PEDEST":"N",
        "AR_TRUCKS":"Y",
        "AR_TRAFF":"Y",
        "AR_DELIV":"Y",
        "AR_EMERVEH":"Y",
        "AR_MOTOR":"Y",
        "PAVED":"Y",
        "PRIVATE":"N",
        "FRONTAGE":"N",
        "BRIDGE":"Y",
        "TUNNEL":"N",
        "RAMP":"N",
        "TOLLWAY":"N",
        "POIACCESS":"N",
        "CONTRACC":"Y",
        "ROUNDABOUT":"N",
        "INTERINTER":"N",
        "UNDEFTRAFF":"N",
        "FERRY_TYPE":"H",
        "MULTIDIGIT":"Y",
        "MAXATTR":"Y",
        "SPECTRFIG":"N",
        "INDESCRIB":"N",
        "MANOEUVRE":"N",
        "DIVIDERLEG":"N",
        "INPROCDATA":"N",
        "FULL_GEOM":"Y",
        "URBAN":"N",
        "ROUTE_TYPE":"1",
        "DIRONSIGN":"",
        "EXPLICATBL":"Y",
        "NAMEONRDSN":"Y",
        "POSTALNAME":"N",
        "STALENAME":"N",
        "VANITYNAME":"N",
        "JUNCTIONNM":"N",
        "EXITNAME":"N",
        "SCENIC_RT":"N",
        "SCENIC_NM":"N",
        "FOURWHLDR":"N",
        "COVERIND":"N0",
        "PLOT_ROAD":"N",
        "REVERSIBLE":"N",
        "EXPR_LANE":"N",
        "CARPOOLRD":"N",
        "PHYS_LANES":"0",
        "VER_TRANS":"Y",
        "PUB_ACCESS":"Y",
        "LOW_MBLTY":"3",
        "PRIORITYRD":"N",
        "SPD_LM_SRC":"1 ",
        "EXPAND_INC":"",
        "TRANS_AREA":"N"
    }   
}"""
    


    
