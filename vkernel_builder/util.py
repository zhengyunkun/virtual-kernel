import parser
import glob
import re
import subprocess


def runCommand(cmd):
    proc = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    (out, err) = proc.communicate()
    outStr = str(out.decode("utf-8"))
    errStr = str(err.decode("utf-8"))
    return (proc.returncode, outStr, errStr)


def isValidOpts(options):
    """
    Check if the required options are sane to be accepted
        - Check if the provided files exist
    :param opts:
    :return:
    """
    if not options.input:
        parser.error("options -i should be provided.")
        return False
    return True


'''

-----------------------For seccomp.py--------------------

'''


def getFuncName(str):  # get syscall function name from its definition.

    s = str.split("(")
    funcname = s[0].split()[-1]
    return funcname

#
# def test_data(path):
#     s = set()
#     with open(path, "r") as f:
#         for a in f.readlines():
#             name = getSyscallName(a)
#             if name in s:
#                 print(name + " already in set")
#                 print(a)
#             else:
#                 s.add(name)


def getSyscallName(str):   # get syscall name from its definition.

    syscallName = getFuncName(str)
    special = {"sys_newstat": "stat", "sys_newfstat": "fstat", "sys_newlstat": "lstat", "sys_sendfile64": "sendfile",
               "sys_newuname": "uname", "sys_ni_syscall": "_sysctl", "sys_umount": "umount2"}
    if syscallName in special.keys():
        syscallName = special[syscallName]
    else:
        syscallName = syscallName[4:]

    return syscallName


# def getvknName(name, syscall2func):
#
#     str = syscall2func[name]
#     vknName = getFuncName(str)
#     vknName = "vkn_"+vknName
#     return vknName

#
# def getOrigName(name,syscall2func):
#
#     str = syscall2func[name]
#     origName = getFuncName(str)
#     origName = "orig_" + origName
#     return origName


def getVknName(name):   # convert syscall name to vkn_syscall name.

    special = {"stat": "sys_newstat", "fstat": "sys_newfstat", "lstat": "sys_newlstat", "sendfile": "sys_sendfile64",
               "uname": "sys_newuname", "_sysctl": "sys_ni_syscall", "umount2": "sys_umount"}

    if name in special.keys():
        name = special[name]
    else:
        name = "sys_" + name

    vknName = "vkn_"+name
    return vknName


def getOrigName(name):   # convert syscall name to orig_syscall name.

    special = {"stat": "sys_newstat", "fstat": "sys_newfstat", "lstat": "sys_newlstat", "sendfile": "sys_sendfile64",
               "uname": "sys_newuname", "_sysctl": "sys_ni_syscall", "umount2": "sys_umount"}

    if name in special.keys():
        name = special[name]
    else:
        name = "sys_" + name

    origName = "orig_" + name
    return origName


def getArgsList(str):       # get args list of syscall function  from its definition.
    s = str.split("(")
    arg_list = []
    s[1] = s[1].strip(");\n")
    args = s[1].split(",")
    for item in args:
        arg_list.append(item.split()[-1].strip("*"))
    return arg_list


def getOrigFunc(str):    # convert syscall name to orig_syscall function name.
    s = str.split("(")
    left = s[0].split()
    left[2] = "(*orig_" + left[2] + ")"
    func = " ".join(left) + "(" + s[1]
    return func


def getVknFunc(str):        # convert syscall name to vkn_syscall function name.
    s = str.split("(")
    left = s[0].split()
    left[2] = "vkn_"+left[2]
    func = " ".join(left) + "(" + s[1]
    return func


def parseArgs(args, name, syscall2Args):  # generate parameter checking statements.
    conditions = ""
    for item2 in args:
        index = item2["index"]
        value = item2["value"]
        op = item2["op"]
        if index == 0:
            arg_check = "regs->di"
        elif index == 1:
            arg_check = "regs->si"
        elif index == 2:
            arg_check = "regs->dx"
        elif index == 3:
            arg_check = "regs->r10"

        if "valueTwo" in item2:
            valueTwo = item2["valueTwo"]
        else:
            valueTwo = 0

        if op == "SCMP_CMP_MASKED_EQ":
            condition = " ( ({0} & {1}) == {2}) "
            condition = condition.format(value, arg_check, valueTwo)
        elif op == "SCMP_CMP_NE":
            condition = "({0} != {1}) "
            condition = condition.format(arg_check, value)
        elif op == "SCMP_CMP_LT":
            condition = "({0} < {1}) "
            condition = condition.format(arg_check, value)
        elif op == "SCMP_CMP_LE":
            condition = "({0} <= {1}) "
            condition = condition.format(arg_check, value)
        elif op == "SCMP_CMP_EQ":
            condition = "({0} == {1}) "
            condition = condition.format(arg_check, value)
        elif op == "SCMP_CMP_GE":
            condition = "({0} >= {1}) "
            condition = condition.format(arg_check, value)
        elif op == "SCMP_CMP_GT":
            condition = "({0} > {1}) "
            condition = condition.format(arg_check, value)

        conditions = conditions + condition + "&& "
        # print(conditions)
    conditions = conditions.strip("&& ")
    return conditions


'''

----------------------For apparmor.py-------------------

'''


def get_mode(capa, flag, recur):   # flag = 1, for deny. get the permission of file.
    capabilitys = "0b{re}000000000{m}{k}{l}{r}{w}{x}"  # m  k  l  r  w  x
    m = flag
    k = flag
    l = flag
    r = flag
    w = flag
    x = flag
    if "m" in capa:
        m = 1-flag
    if "k" in capa:
        k = 1-flag
    if "l" in capa:
        l = 1-flag
    if "r" in capa:
        r = 1-flag
    if "w" in capa:
        w = 1-flag
    if "x" in capa:
        x = 1-flag
    capabilitys = capabilitys.format(re=recur, m=m, k=k, l=l, r=r, w=w, x=x)
    return capabilitys


def and_mode(capa1, capa2):  # conbine the permission of the same file.
    mode = ""
    for i in range(len(capa1)):
        if capa1[i:i+1] == capa2[i:i+1]:
            mode = mode + capa1[i:i+1]
        else:
            mode = mode + "1"
    return mode


def get_filepath(path):  # get filepath from apparmor profile items.
    pathlist = []
    if path.startswith("@{PROC}"):
        path = "/proc" + path[7:]

    if r"{" in path:
        head = path.split("{")
        tail = head[1].split("}")
        list1 = tail[0].split(",")
        for i in list1:
            r = head[0] + i + tail[1]
            pathlist.append(r)
    else:
        pathlist.append(path)

    return pathlist


def get_filelist(path):  # get files in filepath.

    recur = 0
    filelist = []

    if path.endswith("/**"):
        recur = 1
        path = path[:-3]

    if path.endswith("**"):
        recur = 1
        path = path[:-2]

    if "^" not in path:
        list1 = glob.iglob(pathname=path)

        for file in list1:
            filelist.append(file)
    else:
        list_test = glob_parser(path)
        filelist = filelist + list_test
        # todo

    '''test'''
    # if not filelist:
    #     filelist.append(path)
    #  I'm not sure if it will be useful.
    '''test'''

    return filelist, recur


# TODO

def glob_parser(str):

    pattern = r"[\[].*[\]]"
    p = re.findall(pattern, str)
    newstr = str.replace(p[0], "*")

    if newstr.endswith("***"):
        newstr = newstr[:-2]
        print("impossible")
    elif newstr.endswith("**"):
        newstr = newstr[:-1]

    a, b = get_filelist(newstr)
    list1 = []
    str = str.strip("*")
    pattern = str
    for item in a:
        if re.match(pattern=pattern, string=item):
            list1.append(item)
    return list1
