#!/usr/bin/env python3
from util import *
import sys
import optparse
import os
import json

if __name__ == '__main__':

    usage = """
                Usage:
                -i <Input a path of a seccomp json file. >
                -o <output path, default: "./output" >
                -s <path of \"syscall.c\" file, default: "./syscall.c">
            """

    parser = optparse.OptionParser(usage=usage, version="1")

    parser.add_option("-i", "--input", dest="input", default=None,
                      help="Input a path of a seccomp json file.  ")

    parser.add_option("-o", "--outputfolder", dest="outputfolder", default=None,
                      help="output path")

    parser.add_option("-s", "--syscallfunc", dest="syscallfunc", default=None,
                      help="path of \"syscall.c\" file")

    (options, args) = parser.parse_args()

    if (not isValidOpts(options)):
        sys.exit(-1)

    outputfolder = "./output"   # default

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
            "The folder specified for the output (-o) already exists and is a file.\
             Please change the path or delete the file and re-run the script.")
        sys.exit(-1)

    syscallfunc = "./syscall.c"  # default path

    if (options.syscallfunc is not None):
        syscallfunc = options.syscallfunc

    while (syscallfunc.endswith("/")):
        syscallfunc = syscallfunc[:-1]

    if (not os.path.exists(syscallfunc)):
        print("cannnot find the \"syscall.c\" file")
        sys.exit(-1)

    elif (os.path.isdir(syscallfunc)):
        print(  # logger.error
            "path (-s) exists but it's a dir. Please change the path and re-run the script.")
        sys.exit(-1)
    '''********************************************************************************************'''
    seccomppath = options.input

    syscallname = set()

    syscall2func = {}   # key: syscall name, value: function name
    syscall2Args = {}   # key: syscall name, value: function args list

    with open("./input/syscalls.h_manual", "r") as f:
        for a in f.readlines():
            name = getSyscallName(a)
            args = getArgsList(a)

            syscallname.add(name)
            syscall2func[name] = a
            syscall2Args[name] = args

    with open(seccomppath, 'r') as f:
        dict = json.load(f)
        list1 = dict["syscalls"]

    syscalllist_withargs = []
    syscalllist_withoutargs = []

    for item in list1:
        if "args" in item.keys() and item["args"] is not None and item["args"] != []:
            syscalllist_withargs.append(item)     # syscall which needs to check parameter
        else:
            syscalllist_withoutargs.append(item)   # syscall which does not need to check parameter

    syscallset_withoutargs = set()
    syscallset_withargs = set()

    list2 = []
    for item in syscalllist_withoutargs:
        if(item["names"]):
            list2 = list2 + item["names"]

    syscallset_withoutargs = set(list2)

    list2 = []
    for item in syscalllist_withargs:
        if(item["names"]):
            list2 = list2 + item["names"]

    syscallset_withargs = set(list2)

    vknFuncDef = []   # vkn_function definition
    vknFunc = []     # vkn_function body
    origFunc = []     # orig_function body
    syscallinstall = []    # syscall install statements

    for name in syscallset_withargs:
        if name not in syscallname:
            continue
        func = \
'''
{0}{{
        struct pt_regs *regs;
        regs = task_pt_regs(current);
        {1}
        return -1;
}}
'''
        all_statement = ""
        for item in syscalllist_withargs:
            if name not in item["names"]:
                continue
            if_statement = \
    """
        if( {0} ){{
            return {1}({2});
        }}
    """
            args = item["args"]
            conditions = parseArgs(args, name, syscall2Args)

            if_statement = if_statement.format(conditions, getOrigName(name), ", ".join(syscall2Args[name]))

            all_statement = all_statement + if_statement

        func = func.format(getVknFunc(syscall2func[name]).strip(";\n"), all_statement)
        vknFuncDef.append(func)

        vknFunc.append(getVknFunc(syscall2func[name]))
        origFunc.append(getOrigFunc(syscall2func[name]))

        """syscall_install statements"""
        install1 = '    {0} = (void *)sys_call_table[__NR_{1}];\n    sys_call_table[__NR_{2}] = (sys_call_ptr_t)&{3};\n'
        install1 = install1.format(getOrigName(name), name, name, getVknName(name))

        syscallinstall.append(install1)
        syscallinstall.append("\n")

    '''----------------------------------------------------------------------------------------------'''

    syscallset_deny = syscallname - syscallset_withoutargs - syscallset_withargs
    print(syscallset_deny)

    for name in syscallset_deny:
        func = \
'''
{0}{{
    return -1;
}}
'''
        func = func.format(getVknFunc(syscall2func[name]).strip(";\n"))
        vknFuncDef.append(func)
        vknFunc.append(getVknFunc(syscall2func[name]))
        origFunc.append(getOrigFunc(syscall2func[name]))

        """syscall_install statements"""
        install1 = '    {0} = (void *)sys_call_table[__NR_{1}];\n    sys_call_table[__NR_{2}] = (sys_call_ptr_t)&{3};\n'
        install1 = install1.format(getOrigName(name), name, name, getVknName(name))
        # print(install1)
        syscallinstall.append(install1)
        syscallinstall.append("\n")

    ''' -----------------write custom.h ------------------------'''

    with open(outputfolder+"/custom.h", "w") as f:
        s = \
'''#ifndef _VKERNEL_CUSTOM_H
#define _VKERNEL_CUSTOM_H
#include <linux/key.h>
#include <linux/kexec.h>
#include <linux/utsname.h>
#include <linux/syscalls.h>
'''
        f.write(s)
        for item in origFunc:
            f.write(item)
        f.write("\n")
        for item in vknFunc:
            f.write(item)
        f.write("\n")
        for item in vknFuncDef:
            f.write(item)

        '''write note'''
        s = \
    '''
    /**
     *   ### ToDo:
     *   0: deny this syscall.(default)
     *   1: allow this syscall.
     *   2: we made this syscall in our own way, like, futex.
     *   3: syscall with args check .(default : personality(135)and clone(56))
     *   6: non-exist
     *
     * */
#endif /* _VKERNEL_CUSTOM_H */
    '''
        f.write(s)

    ''' -----------------write syscall.c ------------------------'''
    with open(syscallfunc, "r") as f:
        s = f.read()
        s = s.strip("}\n ")

    with open(outputfolder+"/syscall.c", "w") as f:
        header = "#include\"custom.h\" \n"
        s = header + s + "\n" + "".join(syscallinstall) + "}"
        f.write(s)
