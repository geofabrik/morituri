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
        "ST_NAME":"AN DER REUTHE",
        "FUNC_CLASS":"1",
        "SPEED_CAT":"1",
        "FR_SPD_LIM":"100",
        "TO_SPD_LIM":"100",
        "DIR_TRAVEL":"T",
        "AR_AUTO":"N",
        "AR_BUS":"N",
        "AR_TAXIS":"N",
        "AR_CARPOOL":"N",
        "AR_PEDEST":"N",
        "AR_TRUCKS":"N",
        "AR_TRAFF":"N",
        "AR_DELIV":"N",
        "AR_EMERVEH":"N",
        "AR_MOTOR":"N",
        "PAVED":"Y",
        "PRIVATE":"Y",
        "BRIDGE":"Y",
        "TUNNEL":"Y",
        "TOLLWAY":"Y",
        "ROUNDABOUT":"N",
        "FOURWHLDR":"N",
        "PHYS_LANES":"1",
        "PUB_ACCESS":"Y"
    }   
}"""
    


    
