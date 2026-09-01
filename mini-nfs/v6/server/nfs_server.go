package main

import (
	"fmt"
	"io"
	"net"
	"os"
	"path/filepath"
)

const (
	NFSProgram uint32 = 100003
	NFSVersion uint32 = 3

	NFSProcedureLookup uint32 = 3
	NFSProcedureRead   uint32 = 6
	NFSProcedureWrite  uint32 = 7
)

func nfsLookup(data []byte) []byte {

	r := NewXDRReader(data)

	parentHandle, err := r.Opaque()

	if err != nil {
		return []byte("BAD XDR")
	}

	nameBytes, err := r.Opaque()

	if err != nil {
		return []byte("BAD XDR")
	}

	name := string(nameBytes)

	parentPath, err := resolveHandle(
		parentHandle,
	)

	if err != nil {
		return []byte("STALE HANDLE")
	}

	path := filepath.Join(
		parentPath,
		name,
	)

	_, err = os.Stat(path)

	if err != nil {
		return []byte("NOT FOUND")
	}

	// 为找到的文件建立 Handle
	handle, err := registerHandle(path)

	if err != nil {
		return []byte("HANDLE ERROR")
	}

	fmt.Printf(
		"LOOKUP\n"+
			"  parent = %s\n"+
			"  name   = %s\n"+
			"  path   = %s\n",
		parentPath,
		name,
		path,
	)

	return handle
}

func nfsRead(data []byte) []byte {

	r := NewXDRReader(data)

	handle, err := r.Opaque()

	if err != nil {
		return []byte("BAD XDR")
	}

	offset, err := r.Uint64()

	if err != nil {
		return []byte("BAD XDR")
	}

	count, err := r.Uint32()

	if err != nil {
		return []byte("BAD XDR")
	}

	path, err := resolveHandle(handle)

	if err != nil {
		return []byte("STALE HANDLE")
	}

	fmt.Printf(
		"READ\n"+
			"  path   = %s\n"+
			"  offset = %d\n"+
			"  count  = %d\n",
		path,
		offset,
		count,
	)

	f, err := os.Open(path)

	if err != nil {
		return []byte("OPEN ERROR")
	}

	defer f.Close()

	buf := make([]byte, count)

	n, err := f.ReadAt(
		buf,
		int64(offset),
	)

	if err != nil && err != io.EOF {
		return []byte("READ ERROR")
	}

	return buf[:n]
}

func nfsWrite(data []byte) []byte {

	r := NewXDRReader(data)

	handle, err := r.Opaque()

	if err != nil {
		return []byte("BAD XDR")
	}

	offset, err := r.Uint64()

	if err != nil {
		return []byte("BAD XDR")
	}

	writeData, err := r.Opaque()

	if err != nil {
		return []byte("BAD XDR")
	}

	path, err := resolveHandle(handle)

	if err != nil {
		return []byte("STALE HANDLE")
	}

	fmt.Printf(
		"WRITE\n"+
			"  path   = %s\n"+
			"  offset = %d\n"+
			"  bytes  = %d\n",
		path,
		offset,
		len(writeData),
	)

	f, err := os.OpenFile(
		path,
		os.O_WRONLY|os.O_CREATE,
		0644,
	)

	if err != nil {
		return []byte("OPEN ERROR")
	}

	defer f.Close()

	_, err = f.WriteAt(
		writeData,
		int64(offset),
	)

	if err != nil {
		return []byte("WRITE ERROR")
	}

	return []byte("OK")
}

func handleNFS(conn net.Conn) {

	defer conn.Close()

	for {

		req, err := readRPCRequest(conn)

		if err != nil {
			return
		}

		fmt.Printf(
			"\nNFS RPC\n"+
				"  xid       = %d\n"+
				"  program   = %d\n"+
				"  version   = %d\n"+
				"  procedure = %d\n",
			req.XID,
			req.Program,
			req.Version,
			req.Procedure,
		)

		var result []byte

		switch req.Procedure {

		case NFSProcedureLookup:

			result = nfsLookup(req.Args)

		case NFSProcedureRead:

			result = nfsRead(req.Args)

		case NFSProcedureWrite:

			result = nfsWrite(req.Args)

		default:

			result = []byte(
				"UNKNOWN PROCEDURE",
			)
		}

		sendRPCReply(
			conn,
			req.XID,
			result,
		)
	}
}

func startNFSServer() {

	// 确保 export 存在
	err := os.MkdirAll(
		"./export",
		0755,
	)

	if err != nil {
		panic(err)
	}

	ln, err := net.Listen(
		"tcp",
		":9000",
	)

	if err != nil {
		panic(err)
	}

	fmt.Println(
		"NFS server listening :9000",
	)

	for {

		conn, err := ln.Accept()

		if err != nil {
			continue
		}

		go handleNFS(conn)
	}
}
