import json
import os
import optparse
import sys

if __name__ == '__main__':

    usage = """
                Usage:
                -i <Input a path of seccomp profiles. >
                -o <output path, default: "./newresults" >
            """

    parser = optparse.OptionParser(usage=usage, version="1")

    parser.add_option("-i", "--input", dest="inputpath", default=None,
                      help="Input a path of seccomp profiles.  ")

    parser.add_option("-o", "--output", dest="outputpath", default=None,
                      help="output path")

    (options, args) = parser.parse_args()

    inputpath = options.inputpath

    while (inputpath.endswith("/")):
        inputpath = inputpath[:-1]

    outputpath = "./newresults"  # default output path

    if (options.outputpath is not None):  # != None?
        outputpath = options.outputpath

    while (outputpath.endswith("/")):
        outputpath = outputpath[:-1]

    if (not os.path.exists(outputpath)):
        try:
            os.mkdir(outputpath)
        except OSError:
            print("There was a problem creating the output (-o) folder")
            sys.exit(-1)

    elif (os.path.isfile(outputpath)):
        print(  # logger.error
            "The folder specified for the output (-o) already exists and is a file. \
            Please change the path or delete the file and re-run the script.")
        sys.exit(-1)

    syscalls = []   # all syscalls

    with open('./input/syscall_64.tbl', 'r') as f:
        for line in f:
            sline = line.split()[2]
            syscalls.append(sline)

    for item in os.listdir(inputpath):
        profile = item
        if not profile.endswith(".json"):
            continue

        with open(inputpath+"/"+profile, "r") as f:
            profiledict = json.load(f)

        action = profiledict["defaultAction"]

        if action != "SCMP_ACT_ALLOW":
            print("please check your profile: " + profile)
            sys.exit(-1)

        list1 = profiledict["syscalls"]
        syscalldeny = []

        for item1 in list1:
            syscalldeny.append(item1["name"])  # name !!!!

        with open("./input/default1.json", "r") as f:
            newprofile = json.load(f)

        syscallallow = []

        for item2 in syscalls:
            if item2 not in syscalldeny:
                syscallallow.append(item2)

        newprofile["syscalls"][0]["names"] = syscallallow

        with open(outputpath+"/"+profile, "w") as f:
            json.dump(newprofile, f, indent=4)
    print("SUCCESS!")
