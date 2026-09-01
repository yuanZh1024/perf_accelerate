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

	ln, err := net.Listen("tcp", ":9000")
	if err != nil {
		panic(err)
	}

	fmt.Println("Mini RPC Server :9000")

	for {
		conn, err := ln.Accept()
		if err != nil {
			continue
		}

		go handle(conn)
	}
}

func handle(conn net.Conn) {

	defer conn.Close()

	for {

		// ----------------
		// 读取 RPC Header
		// ----------------

		header := make([]byte, 20)

		_, err := io.ReadFull(conn, header)
		if err != nil {
			return
		}

		xid := binary.BigEndian.Uint32(header[0:4])
		program := binary.BigEndian.Uint32(header[4:8])
		version := binary.BigEndian.Uint32(header[8:12])
		procedure := binary.BigEndian.Uint32(header[12:16])
		length := binary.BigEndian.Uint32(header[16:20])

		// ----------------
		// 读取 arguments
		// ----------------

		args := make([]byte, length)

		_, err = io.ReadFull(conn, args)
		if err != nil {
			return
		}

		fmt.Printf(
			"RPC CALL xid=%d program=%d version=%d procedure=%d args=%s\n",
			xid,
			program,
			version,
			procedure,
			string(args),
		)

		// ----------------
		// Dispatch
		// ----------------

		var result string

		if program != ProgramNFS {
			result = "unknown program"

		} else if version != VersionNFS {
			result = "unknown version"

		} else {

			switch procedure {

			case ProcRead:

				result = handleRead(string(args))

			case ProcWrite:

				result = handleWrite(string(args))

			default:

				result = "unknown procedure"
			}
		}

		// ----------------
		// REPLY
		// ----------------

		sendReply(conn, xid, result)
	}
}

func handleRead(args string) string {

	fmt.Println("READ procedure")

	return "READ RESULT: " + args
}

func handleWrite(args string) string {

	fmt.Println("WRITE procedure")

	return "WRITE RESULT: " + args
}

func sendReply(conn net.Conn, xid uint32, result string) {

	data := []byte(result)

	// XID + result length
	header := make([]byte, 8)

	binary.BigEndian.PutUint32(
		header[0:4],
		xid,
	)

	binary.BigEndian.PutUint32(
		header[4:8],
		uint32(len(data)),
	)

	conn.Write(header)
	conn.Write(data)
}
