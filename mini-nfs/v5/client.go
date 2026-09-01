package v5

import (
	"fmt"
	"net"
)

func main() {

	conn, err := net.Dial(
		"tcp",
		"127.0.0.1:9000",
	)

	if err != nil {
		panic(err)
	}

	defer conn.Close()

	// LOOKUP
	args := encodeLookupArgs(
		LookupArgs{
			DirHandle: []byte("/"),
			Name:      "hello.txt",
		},
	)

	handle := rpcCall(
		conn,
		1,
		NFSProcedureLookup,
		args,
	)

	fmt.Println(
		"FileHandle:",
		string(handle),
	)

	// WRITE

	writeArgs := encodeWriteArgs(
		WriteArgs{
			Handle: handle,
			Offset: 0,
			Data:   []byte("hello from NFS"),
		},
	)

	rpcCall(
		conn,
		2,
		NFSProcedureWrite,
		writeArgs,
	)

	// READ

	readArgs := encodeReadArgs(
		ReadArgs{
			Handle: handle,
			Offset: 0,
			Count:  1024,
		},
	)

	data := rpcCall(
		conn,
		3,
		NFSProcedureRead,
		readArgs,
	)

	fmt.Println(
		"READ:",
		string(data),
	)
}

func main() {

	conn, err := net.Dial(
		"tcp",
		"127.0.0.1:9000",
	)

	if err != nil {
		panic(err)
	}

	defer conn.Close()

	// LOOKUP
	args := encodeLookupArgs(
		LookupArgs{
			DirHandle: []byte("/"),
			Name:      "hello.txt",
		},
	)

	handle := rpcCall(
		conn,
		1,
		NFSProcedureLookup,
		args,
	)

	fmt.Println(
		"FileHandle:",
		string(handle),
	)

	// WRITE

	writeArgs := encodeWriteArgs(
		WriteArgs{
			Handle: handle,
			Offset: 0,
			Data:   []byte("hello from NFS"),
		},
	)

	rpcCall(
		conn,
		2,
		NFSProcedureWrite,
		writeArgs,
	)

	// READ

	readArgs := encodeReadArgs(
		ReadArgs{
			Handle: handle,
			Offset: 0,
			Count:  1024,
		},
	)

	data := rpcCall(
		conn,
		3,
		NFSProcedureRead,
		readArgs,
	)

	fmt.Println(
		"READ:",
		string(data),
	)
}

func decodeLookupArgs(data []byte) LookupArgs {

	r := NewXDRReader(data)

	return LookupArgs{
		DirHandle: r.Opaque(),
		Name:      string(r.Opaque()),
	}
}
