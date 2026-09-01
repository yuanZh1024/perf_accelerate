package v3
package main

import (
	"encoding/binary"
	"fmt"
	"io"
	"net"
)

const (
	ProgramNFS uint32 = 100003
	VersionNFS uint32 = 3

	ProcRead  uint32 = 6
	ProcWrite uint32 = 7
)

func main() {

	conn, err := net.Dial("tcp", "127.0.0.1:9000")

	if err != nil {
		panic(err)
	}

	defer conn.Close()

	// READ
	call(
		conn,
		1001,
		ProgramNFS,
		VersionNFS,
		ProcRead,
		"hello.txt|0|1024",
	)

	// WRITE
	call(
		conn,
		1002,
		ProgramNFS,
		VersionNFS,
		ProcWrite,
		"hello.txt|0|hello",
	)
}

func call(
	conn net.Conn,
	xid uint32,
	program uint32,
	version uint32,
	procedure uint32,
	args string,
) {

	data := []byte(args)

	// 20 byte RPC header
	header := make([]byte, 20)

	binary.BigEndian.PutUint32(header[0:4], xid)
	binary.BigEndian.PutUint32(header[4:8], program)
	binary.BigEndian.PutUint32(header[8:12], version)
	binary.BigEndian.PutUint32(header[12:16], procedure)
	binary.BigEndian.PutUint32(header[16:20], uint32(len(data)))

	// CALL
	conn.Write(header)
	conn.Write(data)

	// REPLY

	replyHeader := make([]byte, 8)

	io.ReadFull(conn, replyHeader)

	replyXID := binary.BigEndian.Uint32(replyHeader[0:4])
	length := binary.BigEndian.Uint32(replyHeader[4:8])

	reply := make([]byte, length)

	io.ReadFull(conn, reply)

	fmt.Printf(
		"REPLY xid=%d result=%s\n",
		replyXID,
		string(reply),
	)
}