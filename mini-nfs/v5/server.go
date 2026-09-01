package main

import (
	"bytes"
	"encoding/binary"
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

const Root = "./export"

// =========================
// RPC Request
// =========================

type RPCRequest struct {
	XID       uint32
	Program   uint32
	Version   uint32
	Procedure uint32
	Args      []byte
}

// =========================
// XDR Writer
// =========================

func xdrUint32(v uint32) []byte {
	buf := make([]byte, 4)
	binary.BigEndian.PutUint32(buf, v)
	return buf
}

func xdrUint64(v uint64) []byte {
	buf := make([]byte, 8)
	binary.BigEndian.PutUint64(buf, v)
	return buf
}

// XDR opaque:
// 4 bytes length
// data
// padding to 4-byte boundary
func xdrOpaque(data []byte) []byte {
	var buf bytes.Buffer

	// length
	buf.Write(xdrUint32(uint32(len(data))))

	// data
	buf.Write(data)

	// 4-byte alignment
	padding := (4 - len(data)%4) % 4

	buf.Write(make([]byte, padding))

	return buf.Bytes()
}

// XDR string 本质上和 opaque 类似
func xdrString(s string) []byte {
	return xdrOpaque([]byte(s))
}

// =========================
// XDR Reader
// =========================

type XDRReader struct {
	data []byte
	pos  int
}

func NewXDRReader(data []byte) *XDRReader {
	return &XDRReader{
		data: data,
		pos:  0,
	}
}

func (r *XDRReader) Uint32() uint32 {
	if r.pos+4 > len(r.data) {
		panic("XDR: uint32 out of bounds")
	}

	v := binary.BigEndian.Uint32(
		r.data[r.pos : r.pos+4],
	)

	r.pos += 4

	return v
}

func (r *XDRReader) Uint64() uint64 {
	if r.pos+8 > len(r.data) {
		panic("XDR: uint64 out of bounds")
	}

	v := binary.BigEndian.Uint64(
		r.data[r.pos : r.pos+8],
	)

	r.pos += 8

	return v
}

func (r *XDRReader) Opaque() []byte {
	length := int(r.Uint32())

	if r.pos+length > len(r.data) {
		panic("XDR: opaque out of bounds")
	}

	data := r.data[
		r.pos : r.pos+length,
	]

	r.pos += length

	// Skip padding
	padding := (4 - length%4) % 4

	if r.pos+padding > len(r.data) {
		panic("XDR: padding out of bounds")
	}

	r.pos += padding

	return data
}

// =========================
// NFS LOOKUP
// =========================

type LookupArgs struct {
	DirHandle []byte
	Name      string
}

func decodeLookupArgs(data []byte) LookupArgs {
	r := NewXDRReader(data)

	return LookupArgs{
		DirHandle: r.Opaque(),
		Name:      string(r.Opaque()),
	}
}

// =========================
// NFS READ
// =========================

type ReadArgs struct {
	Handle []byte
	Offset uint64
	Count  uint32
}

func decodeReadArgs(data []byte) ReadArgs {
	r := NewXDRReader(data)

	return ReadArgs{
		Handle: r.Opaque(),
		Offset: r.Uint64(),
		Count:  r.Uint32(),
	}
}

// =========================
// NFS WRITE
// =========================

type WriteArgs struct {
	Handle []byte
	Offset uint64
	Data   []byte
}

func decodeWriteArgs(data []byte) WriteArgs {
	r := NewXDRReader(data)

	return WriteArgs{
		Handle: r.Opaque(),
		Offset: r.Uint64(),
		Data:   r.Opaque(),
	}
}

// =========================
// File Handle
// =========================

// 教学版本：
// 直接把服务器路径当成 File Handle。
// 真实 NFS 的 File Handle 不是这么实现的。
func makeHandle(path string) []byte {
	return []byte(path)
}

// =========================
// NFS LOOKUP Procedure
// =========================

func nfsLookup(data []byte) []byte {

	args := decodeLookupArgs(data)

	fmt.Printf(
		"NFS LOOKUP name=%s\n",
		args.Name,
	)

	// 教学版本：
	// 所有 LOOKUP 都从 Root 开始
	path := filepath.Join(
		Root,
		args.Name,
	)

	_, err := os.Stat(path)

	if err != nil {
		fmt.Printf(
			"LOOKUP failed: %v\n",
			err,
		)

		return []byte("NOT_FOUND")
	}

	handle := makeHandle(path)

	fmt.Printf(
		"LOOKUP success path=%s handle=%s\n",
		path,
		string(handle),
	)

	return handle
}

// =========================
// NFS READ Procedure
// =========================

func nfsRead(data []byte) []byte {

	args := decodeReadArgs(data)

	path := string(args.Handle)

	fmt.Printf(
		"NFS READ path=%s offset=%d count=%d\n",
		path,
		args.Offset,
		args.Count,
	)

	f, err := os.Open(path)

	if err != nil {
		fmt.Printf(
			"READ open failed: %v\n",
			err,
		)

		return []byte("ERROR")
	}

	defer f.Close()

	buf := make([]byte, args.Count)

	n, err := f.ReadAt(
		buf,
		int64(args.Offset),
	)

	if err != nil && err != io.EOF {
		fmt.Printf(
			"READ failed: %v\n",
			err,
		)

		return []byte("ERROR")
	}

	fmt.Printf(
		"READ success bytes=%d\n",
		n,
	)

	return buf[:n]
}

// =========================
// NFS WRITE Procedure
// =========================

func nfsWrite(data []byte) []byte {

	args := decodeWriteArgs(data)

	path := string(args.Handle)

	fmt.Printf(
		"NFS WRITE path=%s offset=%d bytes=%d\n",
		path,
		args.Offset,
		len(args.Data),
	)

	f, err := os.OpenFile(
		path,
		os.O_CREATE|os.O_WRONLY,
		0644,
	)

	if err != nil {
		fmt.Printf(
			"WRITE open failed: %v\n",
			err,
		)

		return []byte("ERROR")
	}

	defer f.Close()

	n, err := f.WriteAt(
		args.Data,
		int64(args.Offset),
	)

	if err != nil {
		fmt.Printf(
			"WRITE failed: %v\n",
			err,
		)

		return []byte("ERROR")
	}

	fmt.Printf(
		"WRITE success bytes=%d\n",
		n,
	)

	return []byte("OK")
}

// =========================
// RPC Reply
// =========================

func sendRPCReply(
	conn net.Conn,
	xid uint32,
	result []byte,
) {

	// Reply:
	//
	// XID      4 bytes
	// LENGTH   4 bytes
	// DATA     N bytes

	header := make([]byte, 8)

	binary.BigEndian.PutUint32(
		header[0:4],
		xid,
	)

	binary.BigEndian.PutUint32(
		header[4:8],
		uint32(len(result)),
	)

	// 发送 Header
	_, err := conn.Write(header)

	if err != nil {
		return
	}

	// 发送 Result
	_, err = conn.Write(result)

	if err != nil {
		return
	}
}

// =========================
// RPC Connection Handler
// =========================

func handleRPC(conn net.Conn) {

	defer conn.Close()

	fmt.Println(
		"new connection:",
		conn.RemoteAddr(),
	)

	for {

		// ==================================
		// 1. RPC Header
		// ==================================
		//
		// XID       4 bytes
		// Program   4 bytes
		// Version   4 bytes
		// Procedure 4 bytes
		// Length    4 bytes
		//
		// Total = 20 bytes

		header := make([]byte, 20)

		_, err := io.ReadFull(
			conn,
			header,
		)

		if err != nil {

			if err == io.EOF {
				fmt.Println(
					"client closed connection",
				)
			} else {
				fmt.Println(
					"read RPC header error:",
					err,
				)
			}

			return
		}

		// ==================================
		// 2. Decode RPC Header
		// ==================================

		xid := binary.BigEndian.Uint32(
			header[0:4],
		)

		program := binary.BigEndian.Uint32(
			header[4:8],
		)

		version := binary.BigEndian.Uint32(
			header[8:12],
		)

		procedure := binary.BigEndian.Uint32(
			header[12:16],
		)

		length := binary.BigEndian.Uint32(
			header[16:20],
		)

		fmt.Printf(
			"\nRPC CALL\n"+
				"  XID       = %d\n"+
				"  Program   = %d\n"+
				"  Version   = %d\n"+
				"  Procedure = %d\n"+
				"  ArgsLen   = %d\n",
			xid,
			program,
			version,
			procedure,
			length,
		)

		// ==================================
		// 3. Read RPC Arguments
		// ==================================

		args := make([]byte, length)

		_, err = io.ReadFull(
			conn,
			args,
		)

		if err != nil {

			fmt.Println(
				"read RPC args error:",
				err,
			)

			return
		}

		// ==================================
		// 4. Validate Program
		// ==================================

		if program != NFSProgram {

			fmt.Println(
				"unknown program:",
				program,
			)

			sendRPCReply(
				conn,
				xid,
				[]byte("UNKNOWN PROGRAM"),
			)

			continue
		}

		// ==================================
		// 5. Validate Version
		// ==================================

		if version != NFSVersion {

			fmt.Println(
				"unknown version:",
				version,
			)

			sendRPCReply(
				conn,
				xid,
				[]byte("UNKNOWN VERSION"),
			)

			continue
		}

		// ==================================
		// 6. RPC Dispatcher
		// ==================================

		var result []byte

		switch procedure {

		case NFSProcedureLookup:

			result = nfsLookup(args)

		case NFSProcedureRead:

			result = nfsRead(args)

		case NFSProcedureWrite:

			result = nfsWrite(args)

		default:

			fmt.Println(
				"unknown procedure:",
				procedure,
			)

			result = []byte(
				"UNKNOWN PROCEDURE",
			)
		}

		// ==================================
		// 7. RPC Reply
		// ==================================

		sendRPCReply(
			conn,
			xid,
			result,
		)

		fmt.Printf(
			"RPC REPLY xid=%d result_len=%d\n",
			xid,
			len(result),
		)
	}
}

// =========================
// Main
// =========================

func main() {

	// 创建共享目录
	err := os.MkdirAll(
		Root,
		0755,
	)

	if err != nil {
		panic(err)
	}

	// 创建测试文件
	testFile := filepath.Join(
		Root,
		"hello.txt",
	)

	if _, err := os.Stat(testFile); os.IsNotExist(err) {

		err := os.WriteFile(
			testFile,
			[]byte("hello from Mini NFSv3\n"),
			0644,
		)

		if err != nil {
			panic(err)
		}
	}

	// ==================================
	// Listen TCP
	// ==================================

	ln, err := net.Listen(
		"tcp",
		":9000",
	)

	if err != nil {
		panic(err)
	}

	defer ln.Close()

	fmt.Println(
		"=================================",
	)

	fmt.Println(
		" Mini NFSv3 Server",
	)

	fmt.Println(
		" Program :", NFSProgram,
	)

	fmt.Println(
		" Version :", NFSVersion,
	)

	fmt.Println(
		" TCP     :9000",
	)

	fmt.Println(
		" Root    :", Root,
	)

	fmt.Println(
		"=================================",
	)

	// ==================================
	// Accept connections
	// ==================================

	for {

		conn, err := ln.Accept()

		if err != nil {

			fmt.Println(
				"accept error:",
				err,
			)

			continue
		}

		go handleRPC(conn)
	}
}