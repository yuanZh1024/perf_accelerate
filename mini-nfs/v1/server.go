package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"net"
	"os"
	"path/filepath"
)

const ROOT = "./export"

type Request struct {
	Op     string `json:"op"`
	Path   string `json:"path"`
	Handle string `json:"handle"`
	Offset int64  `json:"offset"`
	Length int    `json:"length"`
	Data   string `json:"data"`
}

type Response struct {
	OK     bool   `json:"ok"`
	Handle string `json:"handle,omitempty"`
	Data   string `json:"data,omitempty"`
	Error  string `json:"error,omitempty"`
}

func main() {

	os.MkdirAll(ROOT, 0755)

	listener, err := net.Listen("tcp", ":9000")
	if err != nil {
		panic(err)
	}

	fmt.Println("Mini NFS Server listening on :9000")

	for {
		conn, err := listener.Accept()
		if err != nil {
			continue
		}

		go handleConnection(conn)
	}
}

func handleConnection(conn net.Conn) {

	defer conn.Close()

	reader := bufio.NewReader(conn)
	writer := bufio.NewWriter(conn)

	for {

		var req Request

		err := json.NewDecoder(reader).Decode(&req)
		if err != nil {
			return
		}

		var resp Response

		switch req.Op {

		case "LOOKUP":
			resp = lookup(req)

		case "READ":
			resp = readFile(req)

		case "WRITE":
			resp = writeFile(req)

		case "GETATTR":
			resp = getattr(req)

		default:
			resp.Error = "unknown operation"
		}

		json.NewEncoder(writer).Encode(resp)
		writer.Flush()
	}
}

func getattr(req Request) Response {

	path := filepath.Join(ROOT, req.Handle)

	info, err := os.Stat(path)

	if err != nil {
		return Response{
			OK:    false,
			Error: err.Error(),
		}
	}

	data := fmt.Sprintf(
		"name=%s size=%d mode=%s",
		info.Name(),
		info.Size(),
		info.Mode(),
	)

	return Response{
		OK:   true,
		Data: data,
	}
}

func writeFile(req Request) Response {

	path := filepath.Join(ROOT, req.Handle)

	f, err := os.OpenFile(
		path,
		os.O_WRONLY,
		0644,
	)

	if err != nil {
		return Response{
			OK:    false,
			Error: err.Error(),
		}
	}

	defer f.Close()

	data := []byte(req.Data)

	n, err := f.WriteAt(data, req.Offset)

	if err != nil {
		return Response{
			OK:    false,
			Error: err.Error(),
		}
	}

	return Response{
		OK:   true,
		Data: fmt.Sprintf("written %d bytes", n),
	}
}

func readFile(req Request) Response {

	path := filepath.Join(ROOT, req.Handle)

	f, err := os.Open(path)
	if err != nil {
		return Response{
			OK:    false,
			Error: err.Error(),
		}
	}

	defer f.Close()

	buf := make([]byte, req.Length)

	n, err := f.ReadAt(buf, req.Offset)

	if err != nil && n == 0 {
		return Response{
			OK:    false,
			Error: err.Error(),
		}
	}

	return Response{
		OK:   true,
		Data: string(buf[:n]),
	}
}

func lookup(req Request) Response {

	path := filepath.Join(ROOT, req.Path)

	info, err := os.Stat(path)
	if err != nil {
		return Response{
			OK:    false,
			Error: err.Error(),
		}
	}

	if info.IsDir() {
		return Response{
			OK:    false,
			Error: "directory not supported yet",
		}
	}

	// 教学版本直接把路径作为 file handle
	// Handle  是req结构体里面定义的一个变量
	handle := req.Path

	return Response{
		OK:     true,
		Handle: handle,
	}
}
