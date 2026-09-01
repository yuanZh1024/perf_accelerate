package main

import (
	"fmt"
	"net"
)

func encodeMountArgs(path string) []byte {
	return xdrString(path)
}

func encodeLookupArgs(
	dirHandle []byte,
	name string,
) []byte {

	var buf []byte

	buf = append(
		buf,
		xdrOpaque(dirHandle)...,
	)

	buf = append(
		buf,
		xdrString(name)...,
	)

	return buf
}

func encodeReadArgs(
	handle []byte,
	offset uint64,
	count uint32,
) []byte {

	var buf []byte

	buf = append(
		buf,
		xdrOpaque(handle)...,
	)

	buf = append(
		buf,
		xdrUint64(offset)...,
	)

	buf = append(
		buf,
		xdrUint32(count)...,
	)

	return buf
}

func main() {

	// ==================================
	// 1. MOUNT
	// ==================================

	mountConn, err := net.Dial(
		"tcp",
		"127.0.0.1:9001",
	)

	if err != nil {
		panic(err)
	}

	rootHandle, err := rpcCall(
		mountConn,
		1,
		MountProgram,
		MountVersion,
		MountProcedure,
		encodeMountArgs("./export"),
	)

	if err != nil {
		panic(err)
	}

	mountConn.Close()

	fmt.Println(
		"ROOT HANDLE:",
		rootHandle,
	)

	// ==================================
	// 2. LOOKUP
	// ==================================

	nfsConn, err := net.Dial(
		"tcp",
		"127.0.0.1:9000",
	)

	if err != nil {
		panic(err)
	}

	fileHandle, err := rpcCall(
		nfsConn,
		2,
		NFSProgram,
		NFSVersion,
		NFSProcedureLookup,
		encodeLookupArgs(
			rootHandle,
			"hello.txt",
		),
	)

	if err != nil {
		panic(err)
	}

	fmt.Println(
		"FILE HANDLE:",
		fileHandle,
	)

	// ==================================
	// 3. READ
	// ==================================

	data, err := rpcCall(
		nfsConn,
		3,
		NFSProgram,
		NFSVersion,
		NFSProcedureRead,
		encodeReadArgs(
			fileHandle,
			0,
			1024,
		),
	)

	if err != nil {
		panic(err)
	}

	fmt.Println(
		"READ RESULT:",
		string(data),
	)

	nfsConn.Close()
}
