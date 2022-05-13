// +build linux

package vkernel

import (
	"bytes"
	"io"
	"io/ioutil"
	"os"

	"github.com/opencontainers/runc/libcontainer/configs"
	"golang.org/x/sys/unix"
)

const (
	fileName = "vkernel"
)

// VKernel defines configuration information for current container's vkernel module.
type VKernel struct {
	Name string
	Path string // module path inside container
}

func getRelease() (string, error) {

	var u unix.Utsname
	if err := unix.Uname(&u); err != nil {
		return "", err
	}

	return string(u.Release[:bytes.IndexByte(u.Release[:], 0)]), nil
}

// NewVKernel create a vkernel instance and initializes some global variables
func NewVKernel(id string) (vkn *VKernel, err error) {

	name := fileName
	if id != "" {
		name = fileName + "_" + id
	}

	release, err := getRelease()
	if err != nil {
		return nil, err
	}

	vkn = &VKernel{
		Name: name,
		Path: "/lib/modules/" + release + "/extra/vkernel",
	}
	return vkn, nil
}

// ConfigureMountPath add vkernel module mount path to config
func (vkn *VKernel) ConfigureMountPath(mounts []*configs.Mount) ([]*configs.Mount, error) {

	dir := vkn.Path
	mounts = append(mounts, &configs.Mount{
		Source:      dir,
		Destination: dir,
		Device:      "bind",
		Flags:       unix.MS_BIND | unix.MS_REC,
		PremountCmds: []configs.Command{
			{Path: "touch", Args: []string{dir}},
		},
	})

	return mounts, nil
}

// InitVKernel initializes a vkernel module
func (vkn *VKernel) InitVKernel(params string) error {

	var mod elfFile
	var image []byte
	var err error

	// read .ko
	mod.Fh, err = os.Open(vkn.Path + "/" + fileName + ".ko")
	if err != nil {
		return err
	}
	defer mod.Fh.Close()

	mod.setArch()
	mod.mapHeader()

	// rename vkernel module
	if vkn.Name != "" && vkn.Name != fileName {
		mod.Fh.Seek(0, io.SeekStart) // rewind file pointer
		image, err = ioutil.ReadAll(mod.Fh)
		if err != nil {
			return err
		}
		image = mod.rename(image, []byte(fileName), []byte(vkn.Name))
	}

	if err := unix.InitModule(image, params); err != nil {
		return err
	}

	return nil

}

// DeleteVKernel uninstall the vkernel module
func (vkn *VKernel) DeleteVKernel() error {

	return unix.DeleteModule(vkn.Name, 0)

}
