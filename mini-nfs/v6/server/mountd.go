package main

import (
	"fmt"
	"net"
	"os"
	"path/filepath"
	"strings"
)

const (
	MountProgram   uint32 = 100005
	MountVersion   uint32 = 3
	MountProcedure uint32 = 1
)

type ExportRule struct {
	Path    string
	Network string
	Mode    string
}

var exportRule ExportRule

func loadExports() error {

	data, err := os.ReadFile("exports.conf")

	if err != nil {
		return err
	}

	line := strings.TrimSpace(
		string(data),
	)

	fields := strings.Fields(line)

	if len(fields) < 2 {
		return fmt.Errorf(
			"invalid exports.conf",
		)
	}

	exportRule.Path = fields[0]

	client := fields[1]

	client = strings.TrimSpace(client)

	if strings.Contains(client, "(") {
		parts := strings.SplitN(
			client,
			"(",
			2,
		)

		exportRule.Network = parts[0]

		exportRule.Mode = strings.TrimSuffix(
			parts[1],
			")",
		)
	} else {
		exportRule.Network = client
		exportRule.Mode = "ro"
	}

	return nil
}

func handleMount(conn net.Conn) {

	defer conn.Close()

	req, err := readRPCRequest(conn)

	if err != nil {
		return
	}

	fmt.Printf(
		"\nMOUNT RPC\n"+
			"  XID       = %d\n"+
			"  Program   = %d\n"+
			"  Version   = %d\n"+
			"  Procedure = %d\n",
		req.XID,
		req.Program,
		req.Version,
		req.Procedure,
	)

	if req.Program != MountProgram {

		sendRPCReply(
			conn,
			req.XID,
			[]byte("UNKNOWN PROGRAM"),
		)

		return
	}

	if req.Version != MountVersion {

		sendRPCReply(
			conn,
			req.XID,
			[]byte("UNKNOWN VERSION"),
		)

		return
	}

	if req.Procedure != MountProcedure {

		sendRPCReply(
			conn,
			req.XID,
			[]byte("UNKNOWN PROCEDURE"),
		)

		return
	}

	// ============================
	// XDR decode
	// ============================

	r := NewXDRReader(req.Args)

	pathBytes, err := r.Opaque()

	if err != nil {

		sendRPCReply(
			conn,
			req.XID,
			[]byte("BAD XDR"),
		)

		return
	}

	requestPath := string(pathBytes)

	fmt.Println(
		"mount request:",
		requestPath,
	)

	// ============================
	// 检查 export
	// ============================

	cleanPath := filepath.Clean(
		requestPath,
	)

	exportPath := filepath.Clean(
		exportRule.Path,
	)

	if cleanPath != exportPath {

		fmt.Printf(
			"mount denied: %s\n",
			requestPath,
		)

		sendRPCReply(
			conn,
			req.XID,
			[]byte("ACCESS DENIED"),
		)

		return
	}

	// ============================
	// 检查目录
	// ============================

	info, err := os.Stat(exportPath)

	if err != nil || !info.IsDir() {

		sendRPCReply(
			conn,
			req.XID,
			[]byte("NO SUCH DIRECTORY"),
		)

		return
	}

	// ============================
	// 创建 Root File Handle
	// ============================

	handleBytes, err := registerHandle(exportPath)

	if err != nil {

		sendRPCReply(
			conn,
			req.XID,
			[]byte("HANDLE ERROR"),
		)

		return
	}

	handleBytes := EncodeFileHandle(
		handle,
	)

	fmt.Printf(
		"mount success:\n"+
			"  path       = %s\n"+
			"  fsid       = %d\n"+
			"  inode      = %d\n"+
			"  generation = %d\n",
		exportPath,
		handle.FSID,
		handle.Inode,
		handle.Generation,
	)

	// 返回 File Handle
	sendRPCReply(
		conn,
		req.XID,
		handleBytes,
	)
}

func startMountd() {

	err := loadExports()

	if err != nil {
		panic(err)
	}

	fmt.Println(
		"mountd export:",
		exportRule.Path,
	)

	fmt.Println(
		"mountd clients:",
		exportRule.Network,
	)

	fmt.Println(
		"mountd mode:",
		exportRule.Mode,
	)

	ln, err := net.Listen(
		"tcp",
		":9001",
	)

	if err != nil {
		panic(err)
	}

	fmt.Println(
		"mountd listening :9001",
	)

	for {

		conn, err := ln.Accept()

		if err != nil {
			continue
		}

		go handleMount(conn)
	}
}
