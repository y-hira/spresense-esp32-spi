import pandas as pd
import numpy as np
import pickle
import struct
import sys

args = sys.argv

print(args)

dirname="data"
filename="1"
if len(args)>1:
    filename=args[1]
    print("ファイル名：" + args[1])


filepath="./"+dirname+"/"+filename+""
csv_path=filepath+".csv"

col1 = ["AL", "AM", "BL", "BM", "CL", "CM", "DL", "DM", "A","B","C","D"]
df=pd.DataFrame(columns=col1)
mat=[]

df.to_csv(csv_path)


with open(filepath, "rb") as f:
    data=f.read()
    tmp_data=[]
    for idx, item in enumerate(data):
        if item==165:
            item=np.nan
        tmp_data.append(item)
        if idx>0 and idx%8==7:
            for i in range(4):
                val=tmp_data[2*i+1]*256+tmp_data[2*i+0] #Little Endian
                if val>pow(2,15)-1:
                    val=val-pow(2,16)
                tmp_data.append(val)
            mat.append(tmp_data)
            tmp_data=[]

df=pd.DataFrame(mat, columns=col1)
df.to_csv(csv_path, mode='w')