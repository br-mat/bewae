# Python program to convert text
# file to JSON
import os
import json

# the file to be converted
filename = 'data2.txt'

# resultant dictionary
dict1 = {}

# fields in the sample file
titles = ['group', 'switches']
fields = ['name', 'VPin', 'Pump', 'Time']
bool_f = ['name', 'value']

with open(filename) as fh:
    # count variable for employee id creation
    l = 0
    section = ''
    
    for line in fh:
        #find out which section we are in
        if line.strip() in titles:
            section = line.strip()
            l = 0
        if section == 'group':
            # reading line by line from the text file
            content = list( line.strip().split(None, len(fields)))
            # check content
            if len(content) != len(fields):
                continue
            #print(content)
            # for automatic creation of id for each
            sno ='grp'+str(l)
            i = 0
            # intermediate dictionary
            dict2 = {}
            while i<len(fields):
                # creating dictionary
                dict2[fields[i]]= content[i]
                i = i + 1
            # append it to the main dict
            dict1[sno]= dict2
            l = l + 1
        
        if section == 'switches':
            # read line
            content = list(line.strip().split(None, len(bool_f)))
            # check content
            if len(content) != len(bool_f):
                continue
            # generate id number (name)
            sno = 'sw_' + str(l)
            i=0
            dict2 = {}
            while i<len(bool_f):
                # fill up dict
                dict2[bool_f[i]] = content[i]
                i += 1
            # append it to the main dict
            dict1[sno] = dict2
            l += 1

# creating new and remove old json file
os.remove(filename)
out_file = open("test2.json", "w")
json.dump(dict1, out_file, indent = 4)
out_file.close()
