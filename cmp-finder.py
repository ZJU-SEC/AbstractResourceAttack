#! /usr/bin/python3

# -*- coding: utf-8 -*-
"""
Created on Wed Oct 28 22:02:14 2020

@author: PC
"""

import re

file_object = open('./cmp-finder.txt', 'r', encoding='UTF-8')
file_object_1 = open('./location-finder.txt','r',encoding='UTF-8')
f = open('./out_cpm_finder.txt', 'w', encoding='UTF-8')
structresult = {}
globalresult = {}
funclocation = {}
otherlocation = {}
ResourceLocation = {}
countResourceLoc = {}.fromkeys(('arch','block','certs','crypto','Documentation','drivers','fs','include','init','ipc','kernel','lib','LICENSES','mm','net','samples','scripts','security','sound','tools','user','virt','noloc'),0)
funcnamesum=set()
falsestructresult = []
falseglobalresult = []
nolocresult = []
structcount = 0
archcount = 0
blockcount = 0
certscount = 0
cryptocount = 0
Documentationcount = 0
driverscount = 0
fscount = 0
includecount = 0
initcount = 0
ipccount = 0
kernelcount = 0
libcount = 0
licensescount = 0
mmcount = 0
netcount = 0
samplescount = 0
scriptscount = 0
securitycount = 0
soundcount = 0
toolscount = 0
usrcount = 0
virtcount = 0
globalcount = 0
noloccount = 0

def coutValue (countmap,locbegin):
    Count = countmap[locbegin]
    Count = Count + 1
    countmap[locbegin] = Count

def ResourceLoc (fnlist,funclocation):
    countmap = {}.fromkeys(('arch','block','certs','crypto','Documentation','drivers','fs','include','init','ipc','kernel','lib','LICENSES','mm','net','samples','scripts','security','sound','tools','user','virt','noloc'),0)
    for fn in fnlist:
        directory = '' 
        if fn in funclocation:
            directory = funclocation[fn]
        if directory == '':
            nolocCount = countmap['noloc']
            nolocCount = nolocCount + 1
            countmap['noloc'] = nolocCount
            continue

        locsplit=re.split('/',directory)
        locbegin=locsplit[0]
        if locbegin == ".":
            locbegin = locsplit[1]
        coutValue(countmap,locbegin)

    countlist = []
    for key in countmap:
        countlist.append(countmap[key])
    Max = max(countlist)
    for key in countmap:
        if countmap[key] == Max:
            return key

try:
    for line in file_object:
        g = re.split(',',line)
        left = g[0]
        right = g[1]
        r = re.split(':',right)
        l = re.split(':',left)
        
        if r[0] == "ProtectedStruct":   
            struct = r[1]
            struct = struct.strip('%')
            tn = struct.split('.',2)
            if len(tn) >= 2 :
                struct = tn[0] + " " + tn[1]
            functionname = l[1]
            fn = functionname.split('.',2)
            ActualFuncName = fn[0] 
            structresult.setdefault(struct,[]).append(ActualFuncName)
            funcnamesum.add(ActualFuncName)
        
        if r[0] == "Global Variable":
            glbvar = r[1]
            functionname = l[1]
            fn = functionname.split('.',2)
            ActualFuncName = fn[0] 
            globalresult.setdefault(glbvar,[]).append(ActualFuncName)
            funcnamesum.add(ActualFuncName)
    
    for line in file_object_1:
        g = re.split(',',line)
        fn = g[0]
        lo = g[2]
        fnsplit = re.split(':',fn)
        losplit = re.split(':',lo)
        functionname = fnsplit[1]
        location = losplit[1]
        funclocation[functionname] = location

    for funcnametest in funcnamesum:
        gloc = ''
        if funcnametest in funclocation:
            gloc=funclocation[funcnametest]
        if gloc == '':
            noloccount = noloccount + 1
            nolocresult.append(funcnametest)
            continue

        locsplit=re.split('/',gloc)
        locbegin=locsplit[0]
        if locbegin == ".":
            locbegin = locsplit[1]

        if locbegin=="arch":
            archcount=archcount+1 
        elif locbegin=="block":
            blockcount=blockcount+1
        elif locbegin=="certs":
            certscount=certscount+1
        elif locbegin=="crypto":
            cryptocount=cryptocount+1
        elif locbegin=="Documentation":
            Documentationcount=Documentationcount+1
        elif locbegin=="drivers":
            driverscount=driverscount+1
        elif locbegin=="fs":
            fscount=fscount+1
        elif locbegin=="include":
            includecount=includecount+1
        elif locbegin=="init":
            initcount=initcount+1
        elif locbegin=="ipc":
            ipccount=ipccount+1
        elif locbegin=="kernel":
            kernelcount=kernelcount+1
        elif locbegin=="lib":
            libcount=libcount+1
        elif locbegin=="LICENSES":
            licensescount=licensescount+1
        elif locbegin=="mm":
            mmcount=mmcount+1
        elif locbegin=="net":
            netcount=netcount+1
        elif locbegin=="samples":
            samplescount=samplescount+1
        elif locbegin=="scripts":
            scriptscount=scriptscount+1
        elif locbegin=="security":
            securitycount=securitycount+1
        elif locbegin=="sound":
            soundcount=soundcount+1
        elif locbegin=="tools":
            toolscount=toolscount+1
        elif locbegin=="usr":
            usrcount=usrcount+1
        elif locbegin=="virt":
            virtcount=virtcount+1


    for key,value in structresult.items():
        structcount = structcount + 1 
        pattern1 = 'struct'
        pattern2 = 'union'
        if pattern1 not in key and pattern2 not in key:
            falsestructresult.append(key)
          
    for key,value in globalresult.items():
        globalcount = globalcount + 1
        pattern = '.'
        if pattern in key:
            falseglobalresult.append(key)
    
    structcount_str = str(structcount) 
    globalcount_str = str(globalcount)

    for key,value in structresult.items():
        se = set(value)
        li = list(se)
        structresult[key] = li
    
    for key,value in globalresult.items():
        se = set(value)
        li = list(se)
        globalresult[key] = li
    


    f.writelines("Total Struct Type: " + structcount_str + "\n")
    f.writelines("Total Global Variable: " + globalcount_str + "\n")
       
    f.writelines("Total Function: " +str(len(funcnamesum)) + "\n")
    f.writelines("Func under arch dir: " + str(archcount) + "\n")
    f.writelines("Func under block dir: " + str(blockcount) + "\n")
    f.writelines("Func under certs dir: " + str(certscount) + "\n")
    f.writelines("Func under crypto dir: " + str(cryptocount) + "\n")
    f.writelines("Func under Documentation dir: " + str(Documentationcount) + "\n")
    f.writelines("Func under drivers dir: " + str(driverscount) + "\n")
    f.writelines("Func under fs dir: " + str(fscount) + "\n")
    f.writelines("Func under include dir: " + str(includecount) + "\n")
    f.writelines("Func under init dir: " + str(initcount) + "\n")
    f.writelines("Func under ipc dir: " + str(ipccount) + "\n")
    f.writelines("Func under kernel dir: " + str(kernelcount) + "\n")
    f.writelines("Func under lib dir: " + str(libcount) + "\n")
    f.writelines("Func under LICENSES dir: " + str(licensescount) + "\n")
    f.writelines("Func under mm dir: " + str(mmcount) + "\n")
    f.writelines("Func under net dir: " + str(netcount) + "\n")
    f.writelines("Func under samples dir: " + str(samplescount) + "\n")
    f.writelines("Func under scripts dir: " + str(scriptscount) + "\n")
    f.writelines("Func under security dir: " + str(securitycount) + "\n")
    f.writelines("Func under sound dir: " + str(soundcount) + "\n")
    f.writelines("Func under tools dir: " + str(toolscount) + "\n")
    f.writelines("Func under usr dir: " + str(usrcount) + "\n")
    f.writelines("Func under virt dir: " + str(virtcount) + "\n")
    f.writelines("Func directory not found: " + str(noloccount) + "\n")

    #for i in nolocresult:
   #     f.writelines("\n" + "Directory not found Func: " + str(i) + "\n")
    f.writelines("\n")
        
    for key,value in structresult.items():
        Rloc = ''
        Rloc = ResourceLoc(value,funclocation)
        ResourceLocation[key] = Rloc
    
    for key,value in globalresult.items():
        Rloc = ''
        Rloc = ResourceLoc(value,funclocation)
        ResourceLocation[key] = Rloc
    
    for value in ResourceLocation.values():
        Count = countResourceLoc[value]
        Count = Count + 1
        countResourceLoc[value] = Count
    

    
    for key in countResourceLoc:
        f.writelines("Resource under " + key + " dir: " + str(countResourceLoc[key]) + "\n")
        

    for key,value in structresult.items():
        Rloc = ''
        Rloc = ResourceLoc(value,funclocation)
        f.writelines("\n" + "Protected Struct:" + key + "Directory: "+ str(Rloc) + "\n")
        for fn in value:
           directory = '' 
           if fn in funclocation:
               directory = funclocation[fn]
           f.writelines("------:"+fn+"      Directory:"+directory+"\n")
    
           
    f.writelines("\n")
    f.writelines("         Global Variable        " + "\n")
    f.writelines("\n")
    
    for key,value in globalresult.items():
        Rloc = ''
        Rloc = ResourceLoc(value,funclocation)
        f.writelines("\n" + "Global Variable:" + key + "Directory: " + str(Rloc) + "\n")
        for fn in value:
            directory = '' 
            if fn in funclocation:
               directory = funclocation[fn]
            f.writelines("------:"+fn+"      Directory:"+directory+"\n")


finally:
    file_object.close()
    file_object_1.close()
    f.close()
