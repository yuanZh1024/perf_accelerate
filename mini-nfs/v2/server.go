package v2
package main

import (
	"encoding/binary"
	"fmt"
	"io"
	"net"
	"os"
	"strconv"
	"strings"
)

const ROOT = "./export"

func main() {
	os.MkdirAll(ROOT, 0755)

	ln, err := net.Listen("tcp", ":9000")
	if err != nil {
		panic(err)
	}

	fmt.Println("binary server listening :9000")

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
		// 读取 OP
		op := make([]byte, 1)
		// ReadFull: 从 conn 里，努力读满 op 的全部长度，才返回。
		_, err := io.ReadFull(conn, op)
		if err != nil {
			return
		}

		// 读取 LENGTH
		lenBuf := make([]byte, 4)

		_, err = io.ReadFull(conn, lenBuf)
		if err != nil {
			return
		}

		length := binary.BigEndian.Uint32(lenBuf)

		// 读取 PAYLOAD
		payload := make([]byte, length)

		_, err = io.ReadFull(conn, payload)
		if err != nil {
			return
		}

		switch op[0] {
		case 1:
			lookup(conn, payload)

		case 2:
			readFile(conn, payload)

		case 3:
			writeFile(conn, payload)

		default:
			writeResponse(conn, "unknown op")
		}
	}
}

func lookup(conn net.Conn, payload []byte) {

	path := string(payload)

	fullPath := ROOT + "/" + path

	_, err := os.Stat(fullPath)

	if err != nil {
		writeResponse(conn, "NOT_FOUND")
		return
	}

	writeResponse(conn, "OK")
}

func readFile(conn net.Conn, payload []byte) {

	parts := strings.Split(string(payload), "|")

	path := parts[0]

	offset, _ := strconv.ParseInt(parts[1], 10, 64)
	length, _ := strconv.Atoi(parts[2])

	f, err := os.Open(ROOT + "/" + path)

	if err != nil {
		writeResponse(conn, "ERROR")
		return
	}

	defer f.Close()

	buf := make([]byte, length)

	n, err := f.ReadAt(buf, offset)

	if err != nil && err != io.EOF {
		writeResponse(conn, "ERROR")
		return
	}

	writeResponse(conn, string(buf[:n]))
}


func writeFile(conn net.Conn, payload []byte) {

	parts := strings.SplitN(string(payload), "|", 3)

	path := parts[0]

	offset, _ := strconv.ParseInt(parts[1], 10, 64)

	data := []byte(parts[2])

	f, err := os.OpenFile(
		ROOT+"/"+path,
		os.O_CREATE|os.O_WRONLY,
		0644,
	)

	if err != nil {
		writeResponse(conn, "ERROR")
		return
	}

	defer f.Close()

	_, err = f.WriteAt(data, offset)

	if err != nil {
		writeResponse(conn, "ERROR")
		return
	}

	writeResponse(conn, "OK")
}


func writeResponse(conn net.Conn, data string) {

	payload := []byte(data)

	// 发送长度
	var lenBuf [4]byte

	binary.BigEndian.PutUint32(
		lenBuf[:],
		uint32(len(payload)),
	)

	conn.Write(lenBuf[:])

	// 发送数据
	conn.Write(payload)
}