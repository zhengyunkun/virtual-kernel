package vkernel

import (
	"fmt"

	"github.com/syndtr/gocapability/capability"
)

type capData struct {
	effective   uint32
	permitted   uint32
	inheritable uint32
}

type capsV3 struct {
	data    [2]capData
	bounds  [2]uint32
	ambient [2]uint32
}

// GenerateCapabilityParams perform bit operation to generate capability value, and pass them to vkernel module
// default capabilities: "00000000,a80425fb,00000000,a80425fb,00000000,a80425fb 00000000,a80425fb 00000000,00000000"
func GenerateCapabilityParams(effective, permitted, inheritable, bounding, ambient []capability.Cap) string {

	eff, per, inh, bou, amb := [2]uint32{0, 0}, [2]uint32{0, 0}, [2]uint32{0, 0}, [2]uint32{0, 0}, [2]uint32{0, 0}

	for idx, c := range [][]capability.Cap{effective, permitted, inheritable, bounding, ambient} {
		for _, what := range c {
			var i uint
			if what > 31 {
				i = uint(what) >> 5
				what %= 32
			}

			if idx == 0 {
				eff[i] |= 1 << uint(what)
			}
			if idx == 1 {
				per[i] |= 1 << uint(what)
			}
			if idx == 2 {
				inh[i] |= 1 << uint(what)
			}
			if idx == 3 {
				bou[i] |= 1 << uint(what)
			}
			if idx == 4 {
				amb[i] |= 1 << uint(what)
			}
		}
	}

	capsData := fmt.Sprintf("%d,%d,%d,%d,%d,%d", eff[0], eff[1], per[0], per[1], inh[0], inh[1])
	capsBounding := fmt.Sprintf("%d,%d", bou[0], bou[1])
	capsAmbient := fmt.Sprintf("%d,%d", amb[0], amb[1])

	params := fmt.Sprintf("caps_data=%s caps_bounding=%s caps_ambient=%s", capsData, capsBounding, capsAmbient)

	return params
}
