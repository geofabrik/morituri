#!/usr/bin/python
import sys

def print_elem(x,y,z,num):
    print("""        {
            "type":"Feature",
            "geometry":{
                "type":"Point",
                "coordinates":[%s,%s],
                "properties":{}
            },
            "properties":{
                "LINK_ID":"2147483647",
                "POINT_NUM":"%s",
                "NODE_ID":"0",
                "Z_LEVEL":"%s"
            }
        }""" % (x,y,num,z))

if __name__ == '__main__':
    #z_lvls = [0,4,4,0,0,0,5,5,0,0]
    z_lvls = sys.argv[1:]

    l = len(z_lvls)
    print """{
            "type":"FeatureCollection",
                "features":["""
    x = 10.000
    y = 10.000
    for i in range(0,l):
        print_elem(x,y,z_lvls[i],i+1)
        if i < l-1:
            print ","
        x = x+0.001
        y = y+0.001
    print("]}")
    
