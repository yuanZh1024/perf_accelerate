package main

import (
	"encoding/binary"
	"fmt"
	"os"
	"syscall"
)

type FileHandle struct {
	FSID       uint64
	Inode      uint64
	Generation uint32
}

var handlePathMap = map[string]string{}

func EncodeFileHandle(h FileHandle) []byte {

	buf := make([]byte, 20)

	binary.BigEndian.PutUint64(
		buf[0:8],
		h.FSID,
	)

	binary.BigEndian.PutUint64(
		buf[8:16],
		h.Inode,
	)

	binary.BigEndian.PutUint32(
		buf[16:20],
		h.Generation,
	)

	return buf
}

func DecodeFileHandle(
	data []byte,
) (FileHandle, error) {

	if len(data) != 20 {

		return FileHandle{}, fmt.Errorf(
			"invalid file handle length: %d",
			len(data),
		)
	}

	return FileHandle{
		FSID: binary.BigEndian.Uint64(
			data[0:8],
		),

		Inode: binary.BigEndian.Uint64(
			data[8:16],
		),

		Generation: binary.BigEndian.Uint32(
			data[16:20],
		),
	}, nil
}

func createFileHandle(
	path string,
) (FileHandle, error) {

	info, err := os.Stat(path)

	if err != nil {
		return FileHandle{}, err
	}

	st, ok := info.Sys().(*syscall.Stat_t)

	if !ok {
		return FileHandle{}, fmt.Errorf(
			"cannot get syscall.Stat_t",
		)
	}

	return FileHandle{
		FSID:       uint64(st.Dev),
		Inode:      uint64(st.Ino),
		Generation: 1,
	}, nil
}

func registerHandle(
	path string,
) ([]byte, error) {

	handle, err := createFileHandle(path)

	if err != nil {
		return nil, err
	}

	data := EncodeFileHandle(handle)

	handlePathMap[string(data)] = path

	return data, nil
}

func resolveHandle(
	data []byte,
) (string, error) {

	path, ok := handlePathMap[string(data)]

	if !ok {
		return "", fmt.Errorf(
			"unknown file handle",
		)
	}

	return path, nil
}
