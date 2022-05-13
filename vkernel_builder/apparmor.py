#!/usr/bin/env python3
from util import *
import os
import sys
import optparse

if __name__ == '__main__':

    usage = """
                Usage:
                -i <Input a path of a apparmor profile by a json file. >
                -o <output path, default : "./output">
                -v <path of \"apparmor.c\" file, default: "./apparmor.c">
            """

    parser = optparse.OptionParser(usage=usage, version="1")

    parser.add_option("-i", "--input", dest="input", default=None,
                      help="Input a path of a apparmor profile.  ")

    parser.add_option("-o", "--outputfolder", dest="outputfolder", default=None,
                      help="output path")

    parser.add_option("-v", "--vknhashfunc", dest="vknhashfunc", default=None,
                      help="path of \"apparmor.c\" file")

    (options, args) = parser.parse_args()

    if (not isValidOpts(options)):
        sys.exit(-1)

    outputfolder = "./output"   # default output path

    if (options.outputfolder is not None):
        outputfolder = options.outputfolder

    accessRights = 0o777

    while (outputfolder.endswith("/")):
        outputfolder = outputfolder[:-1]

    if (not os.path.exists(outputfolder)):
        try:
            os.mkdir(outputfolder, accessRights)
        except OSError:
            print("There was a problem creating the output (-o) folder")
            sys.exit(-1)

    elif (os.path.isfile(outputfolder)):
        print(  # logger.error
            "The folder specified for the output (-o) already exists and is a file. \
            Please change the path or delete the file and re-run the script.")
        sys.exit(-1)

    vknhashfunc = "./apparmor.c"  # default path of apparmor.c

    if (options.vknhashfunc is not None):
        vknhashfunc = options.vknhashfunc

    while (vknhashfunc.endswith("/")):
        vknhashfunc = vknhashfunc[:-1]

    if (not os.path.exists(vknhashfunc)):
        print("cannnot find the \"apparmor.c\" file")
        sys.exit(-1)

    elif (os.path.isdir(vknhashfunc)):
        print(  # logger.error
            "path (-v) exists but it's a dir. Please change the path and re-run the script.")
        sys.exit(-1)

    '''********************************************************************************************'''

    deny_list = []    # for "deny filepath permission" apparmor items
    allow_list = []   # for "filepath permission"  items

    with open(options.input, "r") as f:
        list = f.readlines()

    for item in list:
        item = item.split("#")[0].strip(", \n")

        if item.startswith("deny") and (item.split()[1].startswith("@{PROC}") or item.split()[1].startswith("/")):
            deny_list.append(item)

        if item.startswith("/"):
            allow_list.append(item)

    deny_dict = {}   # key:file, value:permission
    allow_dict = {}
    all_dict = {}

    for item in deny_list:
        _, path, capa = item.split()
        filepath = get_filepath(path)

        filelistall = []
        for item1 in filepath:
            list1, recur = get_filelist(item1)
            filelistall = filelistall + list1
        mode = get_mode(capa, 1, recur)   # mode is the permission of specific file
        for item2 in filelistall:
            if item2 in all_dict.keys():
                mode = and_mode(mode, all_dict[item2])
            all_dict[item2] = mode

    for item in allow_list:
        path, capa = item.split()
        filepath = get_filepath(path)
        filelistall = []
        for item1 in filepath:
            list1, recur = get_filelist(item1)
            filelistall = filelistall + list1
        mode = get_mode(capa, 0, recur)
        for item2 in filelistall:
            if item2 in all_dict.keys():
                mode = and_mode(mode, all_dict[item2])
            all_dict[item2] = mode

    code = '''
    struct inode *temp = NULL;
    char *path[] = {{ {0}
        "/sys/fs/cgroup/blkio","/sys/fs/cgroup/cpu","/sys/fs/cgroup/cpu,cpuacct","/sys/fs/cgroup/cpuacct","/sys/fs/cgroup/cpuset",
        "/sys/fs/cgroup/devices","/sys/fs/cgroup/freezer","/sys/fs/cgroup/hugetlb","/sys/fs/cgroup/memory","/sys/fs/cgroup/net_cls",
        "/sys/fs/cgroup/net_cls,net_prio","/sys/fs/cgroup/net_prio","/sys/fs/cgroup/perf_event","/sys/fs/cgroup/pids","/sys/fs/cgroup/rdma",
        "/sys/fs/cgroup/systemd",
    }};
    unsigned short v_mode[] = {{ {1}
        0x00,0x00,0x00,0x00,0x00,
    	0x00,0x00,0x00,0x00,0x00,
    	0x00,0x00,0x00,0x00,0x00,
    	0x00,
      }};

    int length = sizeof(v_mode)/sizeof(v_mode[0]);
    int error = 0, i;
    for(i=0;i<length;i++){{
        temp = path_to_inode(path[i]);
        if(!temp){{
            continue;
        }}else{{
            if(S_ISDIR(temp->i_mode)){{
                error = InsertVkernel_hashmap(vkn->vknhash_dir, temp->i_ino, v_mode[i]);
                temp->i_opflags |= IOP_VKERNEL_DIR;
            }} else {{
                error = InsertVkernel_hashmap(vkn->vknhash_reg, temp->i_ino, v_mode[i]);
                temp->i_opflags |= IOP_VKERNEL_REG;
            }}
            if (error)
            {{
                temp->i_opflags &= ~(IOP_VKERNEL_DIR | IOP_VKERNEL_REG);
            }}

        }}
    }}
     return true;
}}
'''
    length = 0
    path = ""
    mode = ""
    for k, v in all_dict.items():
        path = path+"\"" + k + "\", "
        mode = mode + v + ", "
        length = length + 1
        if length % 5 == 0:
            path = path + "\n   "
            mode = mode + "\n   "

    code = code.format(path, mode)

    ''' -----------------write apparmor.c ------------------------'''
    with open(vknhashfunc, "r") as f:
        s1 = f.readlines()
        while s1[-1].strip("\n }") == "":
            s1 = s1[:-1]
        if s1[-1].strip(" \n").startswith("return"):
            s1 = s1[:-1]

    s2 = "".join(s1)
    with open(outputfolder+"/apparmor.c", "w") as f:
        s2 = s2 + "\n" + code
        f.write(s2)

    ''' -----------------write apparmor.h ------------------------'''
    with open(outputfolder+"/apparmor.h", "w") as f:
        code1 = """
#ifndef _VKERNEL_APPARMOR_H
#define _VKERNEL_APPARMOR_H

    bool vkernel_hash_init(void);

#endif /* _VKERNEL_APPARMOR_H */
"""
        f.write(code1)
