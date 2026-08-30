package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"net"
)

type Request struct {
	Op     string `json:"op"`
	Path   string `json:"path,omitempty"`
	Handle string `json:"handle,omitempty"`
	Offset int64  `json:"offset,omitempty"`
	Length int    `json:"length,omitempty"`
	Data   string `json:"data,omitempty"`
}

type Response struct {
	OK     bool   `json:"ok"`
	Handle string `json:"handle,omitempty"`
	Data   string `json:"data,omitempty"`
	Error  string `json:"error,omitempty"`
}

func main() {

	conn, err := net.Dial("tcp", "127.0.0.1:9000")
	if err != nil {
		panic(err)
	}

	defer conn.Close()

	reader := bufio.NewReader(conn)

	// LOOKUP
	send(conn, Request{
		Op:   "LOOKUP",
		Path: "hello.txt",
	})

	var resp Response
	json.NewDecoder(reader).Decode(&resp)

	fmt.Println("LOOKUP:", resp)

	handle := resp.Handle

	// WRITE
	send(conn, Request{
		Op:     "WRITE",
		Handle: handle,
		Offset: 0,
		Data:   "hello from mini nfs!",
	})

	json.NewDecoder(reader).Decode(&resp)

	fmt.Println("WRITE:", resp)

	// READ
	send(conn, Request{
		Op:     "READ",
		Handle: handle,
		Offset: 0,
		Length: 1024,
	})

	json.NewDecoder(reader).Decode(&resp)

	fmt.Println("READ:", resp)
}

func send(conn net.Conn, req Request) {

	json.NewEncoder(conn).Encode(req)
}
