#!/usr/bin/env python3
import os
import sys
import optparse
import util
import json


if __name__ == '__main__':

    if os.geteuid() != 0:
        print("This script must be run as ROOT only! Exiting...")
        sys.exit(-1)

    usage = """
            Usage:
            -i <input  docker images info>
            -o <output folder path, default: "./output" >
            -s <the path of \"syscall.c\" file ,default: "./input/syscall.c" >
            -v <path of \"apparmor.c\" file, default: "./input/apparmor.c">
            """

    parser = optparse.OptionParser(usage=usage, version="1")

    parser.add_option("-i", "--input", dest="input", default=None,
                      help="Input a path of a json file which contains docker images information ")

    parser.add_option("-o", "--outputfolder", dest="outputfolder", default=None,
                      help="output path test")

    parser.add_option("-s", "--syscallfunc", dest="syscallfunc", default=None,
                      help="path of \"syscall.c\" file")

    parser.add_option("-v", "--vknhashfunc", dest="vknhashfunc", default=None,
                      help="path of \"apparmor.c\" file")

    (options, args) = parser.parse_args()

    if (not util.isValidOpts(options)):
        sys.exit(-1)

    '''
          set the paths of output files and "syscall.c" files.
    '''

    outputfolder = "./output"  # default path

    if (options.outputfolder is not None):
        outputfolder = options.outputfolder

    accessRights = 0o775

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

    syscallfunc = "./input/syscall.c"  # default path

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

    vknhashfunc = "./input/apparmor.c"  # default path

    if (options.vknhashfunc is not None):
        vknhashfunc = options.vknhashfunc

    while (vknhashfunc.endswith("/")):
        vknhashfunc = vknhashfunc[:-1]

    if (not os.path.exists(vknhashfunc)):
        print("cannnot find the \"apparmor.c\" file")
        sys.exit(-1)

    elif (os.path.isdir(vknhashfunc)):
        print(  # logger.error
            "path (-v) exists but it's a dir. \
            Please change the path and re-run the script.")
        sys.exit(-1)

    """--------------------------------------------------------------------------------------------------"""
    try:
        with open(options.input, "r") as f:
            image = json.load(f)

    except Exception as e:
        print("Trying to load image list map json from: %s, but doesn't exist: %s" % (options.input, str(e)))
        print("Exiting...")
        sys.exit(-1)

    ''' ------------------------------------------------------------------------------- '''

    imagename = image["name"]
    imageurl = image["image-url"]
    imagelabel = ""
    if image["label"]:
        print("true")
        imagelabel = ":"+image["label"]
    else:
        print("false")

    imageopts = image["opts"]

    pullcmd = "docker pull {0}{1}".format(imageurl, imagelabel)

    print("waiting for docker pull {0}>>>......".format(imageurl))
    code, outstr, errstr = util.runCommand(pullcmd)
    print("docker pull end <<<......")

    slimcmd = "docker-slim --report ./tmp/slim.report.json build  --copy-meta-artifacts ./tmp {0}  {1}{2}"
    slimcmd = slimcmd.format(imageopts, imageurl, imagelabel)

    print("waiting for running docker-slim, it will take a few minutes. \
            If there is no response for a long time, please press <ENTER>      >>>......")
    code1, outstr1, errstr1 = util.runCommand(slimcmd)
    print("docker-slim end <<<......")

    if code1 == 0:
        print("SUCCESS!")
    else:
        print("failed, something wrong....")
