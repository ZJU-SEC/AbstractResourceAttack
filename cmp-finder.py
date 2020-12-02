#! /usr/bin/python3

# -*- coding: utf-8 -*-
"""
Created on Wed Oct 28 22:02:14 2020

@author: PC
"""

import re

file_object = open('./cmp-finder.txt', 'r', encoding='UTF-8')
f = open('./out_cpm_finder.txt', 'w', encoding='UTF-8')
structresult = {}   #structresult是一个<结构体,函数列表>的字典
globalresult = {}   #structresult是一个<全局变量,函数列表>的字典
structcount = 0
globalcount = 0

try:
    for line in file_object:
        g = re.split(',',line)  #用逗号分隔前后两部分，前面是FuncitonName：xxxxx后面是Protected Struct：xxxxx或Global Variable：xxxxx
        left = g[0]           #FuncitonName：xxxxx
        right = g[1]            #Protected Struct：xxxxx或Global Variable：xxxxx
        r = re.split(':',right) #左边再以 ‘ ：’ 号分割
        l = re.split(':',left) #右边也一样
        
        if r[0] == "ProtectedStruct":   
            struct = r[1]
            struct = struct.strip('%')
            tn = struct.split('.',2)
            if len(tn) >= 2 :                 #处理字符串的名字
                struct = tn[0] + " " + tn[1]
            functionname = l[1]
            fn = functionname.split('.',2)
            ActualFuncName = fn[0] 
            structresult.setdefault(struct,[]).append(ActualFuncName) #构造<结构体,函数列表>的字典
        
        if r[0] == "Global Variable":
            glbvar = r[1]
            functionname = l[1]
            fn = functionname.split('.',2)
            ActualFuncName = fn[0] 
            globalresult.setdefault(glbvar,[]).append(ActualFuncName) #构造<全局变量,函数列表>的字典
    
    for key in structresult.items():   #统计struct数目
        structcount = structcount + 1 
    
    for key in globalresult.items():   #统计Global数目
        globalcount = globalcount + 1
    
    structcount_str = str(structcount) 
    globalcount_str = str(globalcount)

    for key,value in structresult.items():   #将列表转换为集合，去重
        se = set(value)
        li = list(se)
        structresult[key] = li
    
    for key,value in globalresult.items():
        se = set(value)
        li = list(se)
        globalresult[key] = li
    
    f.writelines("Total Struct Type: " + structcount_str + "\n")
    f.writelines("Total Global Variable: " + globalcount_str + "\n")
            
    for key,value in structresult.items():
        f.writelines("\n" + "Protected Struct:" + key + "\n")
        for fn in value:
           f.writelines("------:"+fn+"\n")
           
    f.writelines("\n")
    f.writelines("         Global Variable        " + "\n")
    f.writelines("\n")
    
    for key,value in globalresult.items():
        f.writelines("\n" + "Global Variable:" + key + "\n")
        for fn in value:
           f.writelines("------:"+fn+"\n")

finally:
    file_object.close()
    f.close()
