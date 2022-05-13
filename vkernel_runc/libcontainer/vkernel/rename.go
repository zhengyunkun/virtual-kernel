package vkernel

import (
	"bytes"
	"debug/elf"
	"encoding/binary"
	"fmt"
	"io"
	"os"
)

const sectionNameModName = ".gnu.linkonce.this_module"

type enumIdent struct {
	Endianness binary.ByteOrder
	Arch       elf.Class
	Machine    elf.Machine
}

type shdrTble struct {
	Section     interface{}
	SectionName []string
}

type elfFile struct {
	Fh             *os.File
	Ident          [16]byte // magic number
	FileHdr        enumIdent
	Hdr            interface{} // ELF header
	ElfSections    shdrTble
	Size           int64
	Symbols        map[uint32]interface{}
	SymbolsName    map[uint32]string
	DynSymbols     map[uint32]interface{}
	DynSymbolsName map[uint32]string
	Rels           map[uint32]interface{} // relocation entries are mapped to section index
}

// Arch: 32 or 64
func (elfFs *elfFile) setArch() {
	elfFs.Fh.Read(elfFs.Ident[:16]) // read magic number
	switch elf.Class(elfFs.Ident[elf.EI_CLASS]) {
	case elf.ELFCLASS64:
		elfFs.Hdr = new(elf.Header64)
		elfFs.FileHdr.Arch = elf.ELFCLASS64
	case elf.ELFCLASS32:
		elfFs.Hdr = new(elf.Header32)
		elfFs.FileHdr.Arch = elf.ELFCLASS32
	default:
		fmt.Println("Elf Arch Class Invalid !")
	}
}

func (elfFs *elfFile) mapHeader() {

	// LittleEndian or BigEndian
	switch elf.Data(elfFs.Ident[elf.EI_DATA]) {
	case elf.ELFDATA2LSB:
		elfFs.FileHdr.Endianness = binary.LittleEndian
	case elf.ELFDATA2MSB:
		elfFs.FileHdr.Endianness = binary.BigEndian
	default:
		fmt.Println("Possible Corruption, Endianness unknown")
	}

	elfFs.Fh.Seek(0, io.SeekStart)
	err := binary.Read(elfFs.Fh, elfFs.FileHdr.Endianness, elfFs.Hdr)
	if err != nil {
		fmt.Println(err)
	}

	// ELF64 or ELF32 File header
	switch h := elfFs.Hdr.(type) {
	case *elf.Header32:
		elfFs.FileHdr.Machine = elf.Machine(h.Machine)
	case *elf.Header64:
		elfFs.FileHdr.Machine = elf.Machine(h.Machine)
	}
}

func getSectionName(sIndex uint32, sectionShstrTab []byte) string {
	end := sIndex
	for end < uint32(len(sectionShstrTab)) {
		if sectionShstrTab[end] == 0x0 {
			break
		}
		end++
	}

	var name bytes.Buffer
	name.Write(sectionShstrTab[sIndex:end])
	return name.String()
}

func (elfFs *elfFile) rename(image []byte, oldname []byte, newname []byte) []byte {

	// ELF64
	if h, ok := elfFs.Hdr.(*elf.Header64); ok {

		shdrTableSize := h.Shentsize * h.Shnum

		elfFs.ElfSections.Section = make([]elf.Section64, h.Shnum)
		elfFs.ElfSections.SectionName = make([]string, h.Shnum)

		sr := io.NewSectionReader(elfFs.Fh, int64(h.Shoff), int64(shdrTableSize))
		err := binary.Read(sr, elfFs.FileHdr.Endianness, elfFs.ElfSections.Section.([]elf.Section64))
		if err != nil {
			fmt.Println(err)
		}

		shstrtab := make([]byte, elfFs.ElfSections.Section.([]elf.Section64)[h.Shstrndx].Size)
		shstrtabOff := elfFs.ElfSections.Section.([]elf.Section64)[h.Shstrndx].Off
		shstrtabSize := elfFs.ElfSections.Section.([]elf.Section64)[h.Shstrndx].Size

		shstrtabSec := io.NewSectionReader(elfFs.Fh, int64(shstrtabOff), int64(shstrtabSize)+int64(shstrtabOff))
		err = binary.Read(shstrtabSec, elfFs.FileHdr.Endianness, shstrtab)

		for i := 0; i < int(h.Shnum); i++ {
			sIndex := elfFs.ElfSections.Section.([]elf.Section64)[i].Name
			elfFs.ElfSections.SectionName[i] = getSectionName(sIndex, shstrtab)
			if elfFs.ElfSections.SectionName[i] == sectionNameModName {
				oldNameLen := uint64(len(oldname))
				newNameLen := uint64(len(newname))
				offset := elfFs.ElfSections.Section.([]elf.Section64)[i].Off
				size := elfFs.ElfSections.Section.([]elf.Section64)[i].Size
				for p := offset; p < offset+size-oldNameLen; p++ {
					if bytes.Equal(image[p:p+oldNameLen], oldname) {
						for i := p; i < p+newNameLen; i++ {
							image[i] = newname[i-p]
						}
						return image
					}
				}
			}
		}

	}

	// ELF32
	if h, ok := elfFs.Hdr.(*elf.Header32); ok {

		shdrTableSize := h.Shentsize * h.Shnum

		elfFs.ElfSections.Section = make([]elf.Section32, h.Shnum)
		elfFs.ElfSections.SectionName = make([]string, h.Shnum)

		sr := io.NewSectionReader(elfFs.Fh, int64(h.Shoff), int64(shdrTableSize))
		err := binary.Read(sr, elfFs.FileHdr.Endianness, elfFs.ElfSections.Section.([]elf.Section32))
		if err != nil {
			fmt.Println(err)
		}

		shstrtab := make([]byte, elfFs.ElfSections.Section.([]elf.Section32)[h.Shstrndx].Size)
		shstrtabOff := elfFs.ElfSections.Section.([]elf.Section32)[h.Shstrndx].Off
		shstrtabSize := elfFs.ElfSections.Section.([]elf.Section32)[h.Shstrndx].Size
		shstrTableEnd := shstrtabOff + shstrtabSize

		shstrtabSec := io.NewSectionReader(elfFs.Fh, int64(shstrtabOff), int64(shstrTableEnd))
		err = binary.Read(shstrtabSec, elfFs.FileHdr.Endianness, shstrtab)

		for i := 0; i < int(h.Shnum); i++ {
			sIndex := elfFs.ElfSections.Section.([]elf.Section32)[i].Name
			elfFs.ElfSections.SectionName[i] = getSectionName(sIndex, shstrtab)
			if elfFs.ElfSections.SectionName[i] == sectionNameModName {
				oldNameLen := uint32(len(oldname))
				newNameLen := uint32(len(newname))
				offset := elfFs.ElfSections.Section.([]elf.Section32)[i].Off
				size := elfFs.ElfSections.Section.([]elf.Section32)[i].Size
				for p := offset; p < offset+size-oldNameLen; p++ {
					if bytes.Equal(image[p:p+oldNameLen], oldname) {
						for i := p; i < p+newNameLen; i++ {
							image[i] = newname[i-p]
						}
						return image
					}
				}
			}
		}
	}

	return image
}
