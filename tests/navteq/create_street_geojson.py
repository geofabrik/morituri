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
        "LINK_ID":"2147483647"
    }   
}"""
    


    
